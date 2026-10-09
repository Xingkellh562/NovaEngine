#pragma once

#include <voxel/chunk/chunk_data.h>
#include <voxel/planet/planet_player_data.h>
#include <string>

namespace nova_engine::core::world {
	//world是游戏中所有核心数据的集合，只存储数据和状态，不进行任何逻辑处理，所有的逻辑处理都应该在系统中进行
	struct World {
		//目前只有一个区块数据
		nova_engine::voxel::chunk::ChunkData chunkData;
		//临时测试用的玩家数据
		glm::vec3 playerPos = {0,128,0};
		glm::vec3 playerRot = { 0,0,1 };

		glm::dvec3 planetNormal = { 0.0,0.0,1.0 };
		glm::dquat planetOrientation = { 1.0,0.0,0.0,0.0 };

		voxel::planet::PlanetPlayerData planetPlayerData;

		glm::ivec3 selectedBlock = { 0,0,0 };
		std::string blockName = "None";

		std::string heldBlock = "None";
		
		//以后可能会有更多的数据，比如实体数据，物理数据，等等
	};
}