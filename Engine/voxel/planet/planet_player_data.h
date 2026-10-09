#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace nova_engine::voxel::planet {
	struct PlanetPlayerData {
		glm::dvec3 normal = { 0.0,0.0,1.0 }; // 玩家在球面上的法线方向
		glm::dquat orientation = {1.0,0.0,0.0,0.0}; //球对于玩家的姿态
		size_t planetId = 0; //玩家所在的星球ID
	};
}