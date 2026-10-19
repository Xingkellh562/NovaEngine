#pragma once

#include <glm/vec3.hpp>
#include <glm/glm.hpp>
#include <vector>

#include <voxel/planet/planet_chunk_data.h>

namespace nova_engine::utils::spatial {
	struct HitInfo {
		glm::vec3 position;
		glm::vec3 normal;
	};

	//发射一条射线，射线沿体素步进，返回沿途经过的所有体素方块
	std::vector<HitInfo> EmitDDARay(float length, glm::vec3 pos, glm::vec3 forward);

	//发射一条射线，返回射线与等值面的第一个交点以及向外法线
	HitInfo EmitRayInSDF(float length, glm::vec3 pos, glm::vec3 forward, const voxel::planet::PlanetChunkData& voxels);
}