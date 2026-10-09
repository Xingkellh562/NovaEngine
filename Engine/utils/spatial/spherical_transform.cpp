#include "spherical_transform.h"
#include <glm/gtc/quaternion.hpp>
#include <cmath>

namespace nova_engine::utils::spatial {
    static glm::quat rotationBetweenVectors(const glm::vec3& from, const glm::vec3& to) {
        glm::vec3 f = glm::normalize(from);
        glm::vec3 t = glm::normalize(to);
        float cosTheta = glm::dot(f, t);

        if (cosTheta > 0.9999f) {
            // 方向相同，返回单位四元数
            return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        }
        else if (cosTheta < -0.9999f) {
            // 方向相反，绕任意垂直轴旋转 180 度
            glm::vec3 axis = glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), f);
            if (glm::length(axis) < 1e-6f) {
                axis = glm::cross(glm::vec3(0.0f, 0.0f, 1.0f), f);
            }
            axis = glm::normalize(axis);
            return glm::angleAxis(glm::pi<float>(), axis);
        }
        else {
            glm::vec3 axis = glm::cross(f, t);
            float s = std::sqrt((1.0f + cosTheta) * 2.0f);
            float invs = 1.0f / s;
            glm::quat q;
            q.w = s * 0.5f;
            q.x = axis.x * invs;
            q.y = axis.y * invs;
            q.z = axis.z * invs;
            return glm::normalize(q);
        }
    }

	glm::vec3 PlaneToSphere(const glm::vec2& w, float R) {
		float w2 = glm::dot(w, w);
		float denom = R * R + w2;
		return glm::vec3(
			2.0f * R * R * w.x / denom,
			2.0f * R * R * w.y / denom,
			R * (w2 - R * R) / denom
		);
	}
	glm::vec2 PlaneToSpherical(
		const glm::vec2& planePoint,
		const glm::vec2& A,
		float R,
		bool rotateToOrigin) 
	{
        // 1. 平移和缩放：w = (z - A) / 2
        glm::vec2 w = (planePoint - A) / 2.0f;

        // 2. 球极投影到球面
        glm::vec3 p = PlaneToSphere(w, R);

        // 3. 可选旋转，使平面原点映射到 (R, 0, 0)
        if (rotateToOrigin) {
            glm::vec2 w0 = (glm::vec2(0.0f) - A) / 2.0f;
            glm::vec3 p0 = PlaneToSphere(w0, R);

            glm::vec3 from = glm::normalize(p0);
            glm::vec3 to = glm::vec3(1.0f, 0.0f, 0.0f);

            // 处理极端情况：from 与 to 相反
            float dot = glm::dot(from, to);
            glm::quat q;
            if (dot < -0.9999f) {
                // 选择任意垂直轴旋转 180 度
                glm::vec3 axis = glm::cross(from, glm::vec3(0.0f, 1.0f, 0.0f));
                if (glm::length(axis) < 1e-6f)
                    axis = glm::cross(from, glm::vec3(0.0f, 0.0f, 1.0f));
                q = glm::angleAxis(glm::pi<float>(), glm::normalize(axis));
            }
            else {
                q = rotationBetweenVectors(from, to);
            }
            p = q * p;
        }

        // 4. 转经纬度
        float phi = std::asin(p.z / R);       // 纬度
        float lambda = std::atan2(p.y, p.x);     // 经度

        return glm::vec2(lambda, phi);
	}

    // 辅助：从 w 计算球面三维点
    static glm::vec3 planeToSpherePoint(const glm::vec2& w, float R) {
        float w2 = glm::dot(w, w);
        float denom = R * R + w2;
        return glm::vec3(
            2.0f * R * R * w.x / denom,
            2.0f * R * R * w.y / denom,
            R * (w2 - R * R) / denom
        );
    }

    /**
     * 将无限平面上的点转换到球面，并返回该点处切平面向外的单位法线
     * @param planePoint 平面坐标 (u, v)
     * @param A          当前动态点 (Au, Av)，该点附近畸变最小
     * @param R          球半径
     * @param rotateToOrigin 是否旋转使平面原点对应球面经纬度 (0,0)
     * @return glm::vec3 单位外法线（从球心指向该点）
     */
    glm::vec3 PlaneToSphereNormal(
        const glm::vec2& planePoint,
        const glm::vec2& A,
        float R,
        bool rotateToOrigin)
    {
        // 1. 平移和缩放：w = (z - A) / 2
        glm::vec2 w = (planePoint - A) / 2.0f;

        // 2. 球极投影到球面
        glm::vec3 p = planeToSpherePoint(w, R);

        // 3. 可选旋转，使平面原点映射到 (R, 0, 0)
        if (rotateToOrigin) {
            glm::vec2 w0 = (glm::vec2(0.0f) - A) / 2.0f;
            glm::vec3 p0 = planeToSpherePoint(w0, R);

            glm::vec3 from = glm::normalize(p0);
            glm::vec3 to = glm::vec3(1.0f, 0.0f, 0.0f);

            glm::quat q;
            float dot = glm::dot(from, to);
            if (dot < -0.9999f) {
                // 几乎相反，任选垂直轴旋转 180 度
                glm::vec3 axis = glm::cross(from, glm::vec3(0.0f, 1.0f, 0.0f));
                if (glm::length(axis) < 1e-6f)
                    axis = glm::cross(from, glm::vec3(0.0f, 0.0f, 1.0f));
                q = glm::angleAxis(glm::pi<float>(), glm::normalize(axis));
            }
            else {
                q = rotationBetweenVectors(from, to);
            }
            p = q * p;
        }

        // 4. 外法线即球面点的单位方向向量
        return glm::normalize(p) * R;
    }

    glm::dquat ComputeRollQuaternion(const glm::dvec2& delta, double R) {
		double dist = glm::length(delta);
        if (dist <= 1e-12) {
            return glm::dquat(1.0, 0.0, 0.0, 0.0); // 无旋转

        }
        //旋转轴(dy,0,-dx)
		glm::dvec3 axis(delta.y, 0.0, -delta.x);
		axis = glm::normalize(axis);

		double angle = dist / R; // 旋转角度
		return glm::angleAxis(angle, axis);
    }

    /**
     * 计算球面法线滚动偏移 (dx, dz) 后的新法线。
     * 平面为世界 xz 平面，法线 +y。
     * @param n      当前球面单位法线
     * @param offset 平面偏移 (dx, dz)
     * @param R      球半径
     * @return 新的单位法线
     */
    glm::dvec3 OffsetNormal(const glm::dvec3& n, const glm::dvec2& offset, double R) {
        double dist = glm::length(offset);
        if (dist < 1e-12) return n;

        glm::dvec3 axis = glm::dvec3(offset.y, 0.0, -offset.x) / dist;
        double angle = dist / R;

        double c = std::cos(angle);
        double s = std::sin(angle);
        double k = glm::dot(axis, n);

        glm::dvec3 result =
            n * c +
            glm::cross(axis, n) * s +
            axis * (k * (1.0 - c));

        return glm::normalize(result);
    }
}