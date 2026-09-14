#pragma once

#include <glm/vec3.hpp>
#include <glm/glm.hpp>
#include <cmath>
#include <vector>


namespace nova_engine::utils::spatial {
	struct HitInfo {
		glm::ivec3 position;
		glm::ivec3 normal;
	};

	//发射一条射线，射线沿体素步进，返回沿途经过的所有体素方块
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
}