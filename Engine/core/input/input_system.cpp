#include "input_system.h"
#include "input_behavior.h"
#include <log/logger.h>
#include <utils/spatial/analyzer.h>
#include <voxel/block/block_library.h>
#include <utils/spatial/spherical_transform.h>
#include<utils/assist/des_packer.h>

namespace nova_engine::core::input {
	static const std::array<std::array<int, 3>, 7> NeighBours = {
		{{1,0,0},{0,1,0},{1,1,0},{0,0,1},{1,0,1},{0,1,1},{1,1,1}}
	};

	const float SPEED = 30.0f;
	void InputSystem::Update(double time,world::World& world) {
		auto deltaMove = MoveUpdate(time, SPEED, world.playerRot, glm::vec3(0, 1, 0), state_);
		world.playerPos += deltaMove;

		world.playerRot = UpdateRotation(time, world.playerRot, 40, state_);

		auto voxelInfos = utils::spatial::EmitDDARay(7,world.playerPos, world.playerRot);

		world.selectedBlock = {0,0,0};
		world.blockName = "None";

		auto heldBlock =
			voxel::block::BlockLibrary::GetInstance().FindBlock(state_.nowBlock);
		if (heldBlock)
			world.heldBlock = heldBlock->GetName();
		else
			world.heldBlock = "Null";

		for (int i = 0;i < voxelInfos.size();i++) {
			auto pos = voxelInfos[i].position;
			auto chunk = world.planet.chunkData.FindChunk(pos.x, pos.y, pos.z);
			if (!chunk) continue;
			float data = 0;
			if (chunk) {
				data = chunk->GetVoxel(pos.x & 31,
								pos.y & 31,
								pos.z & 31);
			}
			float voxel = utils::assist::UnpackDensity(data);
			if (voxel < 0.0f) {
				world.selectedBlock = { pos.x, pos.y, pos.z };
				if (state_.destroyBlock) {
					if (state_.destroyBlock) {
						chunk->SetVoxel(pos.x & 31,
							pos.y & 31,
							pos.z & 31, utils::assist::Pack(1.0,0));
						state_.destroyBlock = false;
					}
					int size = config::CHUNK_SIZE;
					for (int nx = -1;nx <= 1;nx++)
						for (int ny = -1;ny <= 1;ny++)
							for (int nz = -1;nz <= 1;nz++) {
								world.planet.chunkData.SignDirty({ pos.x + nx * 2, pos.y + ny * 2, pos.z + nz * 2 });
							}
				}

				if (state_.buildBlock) {
					auto normal = voxelInfos[i].normal;
					int x = pos.x + normal.x;
					int y = pos.y + normal.y;
					int z = pos.z + normal.z;

					auto chunkBuild = world.planet.chunkData.FindChunk(x, y, z);
					if (chunkBuild) {
						chunkBuild->SetVoxel(x & 31,y & 31,z & 31,
							utils::assist::Pack(-1.0f,state_.nowBlock));
						state_.buildBlock = false;
					}

					for (int nx = -1;nx <= 1;nx++)
						for (int ny = -1;ny <= 1;ny++)
							for (int nz = -1;nz <= 1;nz++) {
								world.planet.chunkData.SignDirty({ pos.x + nx * 2, pos.y + ny * 2, pos.z + nz * 2 });
							}
				}

				auto block = 
					voxel::block::BlockLibrary::GetInstance().FindBlock(voxel);
				if (block)
					world.blockName = block->GetName();
				else
					world.blockName = "None";
				break;
			}
		}

		state_.buildBlock = false;
		state_.destroyBlock = false;
	}
}