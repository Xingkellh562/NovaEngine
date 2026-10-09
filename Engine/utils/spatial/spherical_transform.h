#pragma once

#include <glm/glm.hpp>


namespace nova_engine::utils::spatial {
	//这个模块提供了球面坐标系和平面坐标系之间的转换函数
	glm::vec3 PlaneToSphere(const glm::vec2& w, float R);

    /**
    * 将无限平面上的点转换到球面经纬度
    * @param planePoint 平面坐标 (u, v)
    * @param A          当前动态点 (Au, Av)，该点附近畸变最小
    * @param R          球半径
    * @param rotateToOrigin 是否旋转使平面原点对应球面经纬度 (0,0)
    * @return glm::vec2 (经度 lambda, 纬度 phi)，单位弧度
    */
    glm::vec2 PlaneToSpherical(
        const glm::vec2& planePoint,
        const glm::vec2& A,
        float R,
        bool rotateToOrigin = false);

    glm::vec3 PlaneToSphereNormal(
        const glm::vec2& planePoint,
        const glm::vec2& A,
        float R,
        bool rotateToOrigin = false);

	glm::dquat ComputeRollQuaternion(const glm::dvec2& delta, double R);

    glm::dvec3 OffsetNormal(const glm::dvec3& n, const glm::dvec2& offset, double R);
}