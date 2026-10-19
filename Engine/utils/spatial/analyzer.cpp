#include "analyzer.h"
#include <cmath>
#include <array>

namespace nova_engine::utils::spatial {
    std::vector<HitInfo> EmitDDARay(float length, glm::vec3 pos, glm::vec3 forward) {
        std::vector<HitInfo> result;

        // 方向为零直接返回
        float dirLen = glm::length(forward);
        if (dirLen < 1e-8f) return result;

        // 归一化，使参数 t 就等于实际前进距离
        glm::vec3 dir = forward / dirLen;

        // 起始体素：向下取整，正确处理负坐标
        glm::ivec3 v = glm::floor(pos);

        // 每个轴的步进方向（-1 / 0 / +1）
        glm::ivec3 step{
            dir.x > 0.f ? 1 : (dir.x < 0.f ? -1 : 0),
            dir.y > 0.f ? 1 : (dir.y < 0.f ? -1 : 0),
            dir.z > 0.f ? 1 : (dir.z < 0.f ? -1 : 0)
        };

        const float INF = std::numeric_limits<float>::infinity();

        // tDelta：每跨过一个体素在该轴上需要前进的参数距离
        glm::vec3 tDelta{
            step.x != 0 ? std::abs(1.0f / dir.x) : INF,
            step.y != 0 ? std::abs(1.0f / dir.y) : INF,
            step.z != 0 ? std::abs(1.0f / dir.z) : INF
        };

        // tMax：到各轴下一个体素边界的参数距离
        glm::vec3 tMax{ INF, INF, INF };
        if (step.x > 0)      tMax.x = (std::floor(pos.x) + 1.0f - pos.x) / dir.x;
        else if (step.x < 0) tMax.x = (std::ceil(pos.x) - 1.0f - pos.x) / dir.x;

        if (step.y > 0)      tMax.y = (std::floor(pos.y) + 1.0f - pos.y) / dir.y;
        else if (step.y < 0) tMax.y = (std::ceil(pos.y) - 1.0f - pos.y) / dir.y;

        if (step.z > 0)      tMax.z = (std::floor(pos.z) + 1.0f - pos.z) / dir.z;
        else if (step.z < 0) tMax.z = (std::ceil(pos.z) - 1.0f - pos.z) / dir.z;

        // 起点体素先加入，法线为 0
        result.push_back({ v, glm::ivec3(0) });

        while (true) {
            // 选择 tMax 最小的轴（tMax 为 INF 的轴天然不会被选中）
            int axis;
            if (tMax.x < tMax.y && tMax.x < tMax.z)      axis = 0;
            else if (tMax.y < tMax.z)                    axis = 1;
            else                                         axis = 2;

            // 下一步已经超出射线长度，结束
            if (tMax[axis] > length) break;

            // 前进一个体素
            v[axis] += step[axis];

            // 进入该体素时对应的面法线
            glm::ivec3 normal(0);
            normal[axis] = -step[axis];

            result.push_back({ v, normal });

            // 该轴的下一段边界
            tMax[axis] += tDelta[axis];
        }

        return result;
    }

    HitInfo EmitRayInSDF(float length, glm::vec3 pos, glm::vec3 forward, 
        const voxel::planet::PlanetChunkData& voxels) {
        const glm::vec3 off(0.5f, 0.5f, 0.5f);

        // 未命中标记：法线为零向量
        HitInfo result = { { 0,0,0 },{ 0,0,0 } };

        float dirLen = glm::length(forward);
        if (dirLen < 1e-8f) return result;
        glm::vec3 dir = forward / dirLen;

        // 体素空间射线起点（世界坐标 = 体素空间坐标 + off）
        const glm::vec3 originVoxel = pos - off;

        // 先做 DDA 得到所有与射线相交的体素
        auto ddaPositions = EmitDDARay(length, originVoxel, forward);

        // 采样体素 8 角点；返回是否可能包含等值面
        // 索引 i = x*4 + y*2 + z，对应局部坐标 (x,y,z)
        auto sampleVoxel = [&voxels](const glm::ivec3& v, std::array<float, 8>& d) -> bool {
            for (int i = 0; i < 8; ++i) {
                int x = (i >> 2) & 1, y = (i >> 1) & 1, z = i & 1;
                d[i] = voxels.GetVoxelDensity((float)(v.x + x),
                    (float)(v.y + y),
                    (float)(v.z + z));
            }
            float dmin = d[0], dmax = d[0];
            for (int i = 1; i < 8; ++i) {
                dmin = std::min(dmin, d[i]);
                dmax = std::max(dmax, d[i]);
            }
            // 角点全部同号 → 该体素内无等值面
            return !(dmin > 0.f || dmax < 0.f);
            };

        // 三线性插值，lp ∈ [0,1]^3
        auto trilinear = [](const std::array<float, 8>& d, glm::vec3 lp) -> float {
            float x = lp.x, y = lp.y, z = lp.z;
            float c00 = d[0] * (1.f - x) + d[4] * x;
            float c01 = d[1] * (1.f - x) + d[5] * x;
            float c10 = d[2] * (1.f - x) + d[6] * x;
            float c11 = d[3] * (1.f - x) + d[7] * x;
            float c0 = c00 * (1.f - y) + c10 * y;
            float c1 = c01 * (1.f - y) + c11 * y;
            return c0 * (1.f - z) + c1 * z;
            };

        // 三线性插值解析梯度（体素边长 = 1，局部梯度即世界梯度）
        auto trilinearGrad = [](const std::array<float, 8>& d, glm::vec3 lp) -> glm::vec3 {
            float x = lp.x, y = lp.y, z = lp.z;
            float gx = (d[4] - d[0]) * (1.f - y) * (1.f - z) + (d[6] - d[2]) * y * (1.f - z)
                + (d[5] - d[1]) * (1.f - y) * z + (d[7] - d[3]) * y * z;
            float gy = (d[2] - d[0]) * (1.f - x) * (1.f - z) + (d[6] - d[4]) * x * (1.f - z)
                + (d[3] - d[1]) * (1.f - x) * z + (d[7] - d[5]) * x * z;
            float gz = (d[1] - d[0]) * (1.f - x) * (1.f - y) + (d[5] - d[4]) * x * (1.f - y)
                + (d[3] - d[2]) * (1.f - x) * y + (d[7] - d[6]) * x * y;
            return glm::vec3(gx, gy, gz);
            };

        // 射线与体素 AABB [v, v+1]^3 求交，返回参数区间 [tEnter, tExit]（体素空间）
        auto rayBox = [&](const glm::ivec3& v, float& tEnter, float& tExit) -> bool {
            glm::vec3 bmin(v);
            glm::vec3 bmax = bmin + 1.f;
            float t0 = -std::numeric_limits<float>::infinity();
            float t1 = std::numeric_limits<float>::infinity();

            for (int a = 0; a < 3; ++a) {
                float o = originVoxel[a];
                float d = dir[a];
                if (std::abs(d) < 1e-12f) {
                    if (o < bmin[a] || o > bmax[a]) return false;
                }
                else {
                    float inv = 1.f / d;
                    float ta = (bmin[a] - o) * inv;
                    float tb = (bmax[a] - o) * inv;
                    if (ta > tb) std::swap(ta, tb);
                    t0 = std::max(t0, ta);
                    t1 = std::min(t1, tb);
                    if (t0 > t1) return false;
                }
            }

            t0 = std::max(t0, 0.f);
            t1 = std::min(t1, length);
            if (t0 > t1) return false;

            tEnter = t0;
            tExit = t1;
            return true;
            };

        std::array<float, 8> densities{};

        for (const auto& ddaHit : ddaPositions) {
            glm::vec3 vf = glm::vec3(ddaHit.position);
            glm::ivec3 v = glm::ivec3(glm::floor(vf));

            // 1) 体素必须与等值面相交
            if (!sampleVoxel(v, densities)) continue;

            // 2) 射线必须穿过该体素
            float tEnter, tExit;
            if (!rayBox(v, tEnter, tExit)) continue;

            // 3) 沿射线采样，找 g(t) 的符号变化
            const int N = 16;
            auto raySDF = [&](float t) {
                glm::vec3 pVoxel = originVoxel + t * dir;
                glm::vec3 lp = glm::clamp(pVoxel - glm::vec3(v), 0.f, 1.f);
                return trilinear(densities, lp);
                };

            float prevT = tEnter;
            float prevF = raySDF(tEnter);
            float tHit = -1.f;

            for (int i = 1; i <= N; ++i) {
                float t = tEnter + (tExit - tEnter) * (float)i / (float)N;
                float f = raySDF(t);
                if (prevF * f <= 0.f) {
                    float denom = f - prevF;
                    tHit = (std::abs(denom) > 1e-12f)
                        ? prevT + (t - prevT) * (-prevF / denom)
                        : t;
                    break;
                }
                prevT = t;
                prevF = f;
            }

            // 射线未穿过该体素内的等值面 → 继续下一个体素
            if (tHit < 0.f) continue;

            // 4) 计算交点与法线
            glm::vec3 pHit = pos + tHit * dir;                    // 世界坐标
            glm::vec3 pVoxel = originVoxel + tHit * dir;            // 体素空间交点
            glm::vec3 lp = glm::clamp(pVoxel - glm::vec3(v), 0.f, 1.f);

            glm::vec3 n = trilinearGrad(densities, lp);
            float nlen = glm::length(n);
            if (nlen < 1e-8f) continue;   // 退化，跳过
            n /= nlen;

            // 若你的密度场是"内正外负"，改为：n = -n;

            result.position = pHit;
            result.normal = n;
            return result;
        }

        return result;
    }
}
 