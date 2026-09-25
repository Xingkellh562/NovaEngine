#include "input_system.h"
#include "input_behavior.h"
#include <render/opengl/library//camera_library.h>
#include <log/logger.h>
#include <utils/spatial/analyzer.h>
#include <voxel/block/block_library.h>


using namespace nova_engine::render::opengl;

namespace nova_engine::core::input {
	const std::array<std::array<int, 3>, 6> NeighBours = {
		{{-1,0,0},{1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}
	};

	const float SPEED = 10.0f;
	void InputSystem::Update(double time,world::World& world,context::RenderContext& context) {
		world.playerPos += MoveUpdate(time, SPEED, world.playerRot, glm::vec3(0,1,0), state_);
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
			auto chunk = world.chunkData.FindChunk(pos.x, pos.y, pos.z);
			int voxel = 0;
			if (chunk) {
				voxel = chunk->GetVoxel(pos.x & 31,
								pos.y & 31,
								pos.z & 31);
			}
			if (voxel) {
				world.selectedBlock = { pos.x, pos.y, pos.z };
				if (state_.destroyBlock) {
					if (state_.destroyBlock) {
						chunk->SetVoxel(pos.x & 31,
							pos.y & 31,
							pos.z & 31, 0);
						state_.destroyBlock = false;
					}

					auto layer = context.chunkMesh.GetLayer(Default);
					auto layer2 = context.chunkMesh.GetLayer(Lucency);
					if (layer && layer2) {
						layer->FreeMesh(pos.x, pos.y, pos.z);
						layer2->FreeMesh(pos.x, pos.y, pos.z);

						for (size_t i = 0; i < 6; i++)
						{
							layer->FreeMesh(pos.x + NeighBours[i][0], pos.y + NeighBours[i][1], pos.z + NeighBours[i][2]);
							layer2->FreeMesh(pos.x + NeighBours[i][0], pos.y + NeighBours[i][1], pos.z + NeighBours[i][2]);
						}
					}
				}

				if (state_.buildBlock) {
					auto normal = voxelInfos[i].normal;
					int x = pos.x + normal.x;
					int y = pos.y + normal.y;
					int z = pos.z + normal.z;

					auto chunkBuild = world.chunkData.FindChunk(x, y, z);
					if (chunkBuild) {
						chunkBuild->SetVoxel(x & 31,y & 31,z & 31,
							state_.nowBlock);
						state_.buildBlock = false;
					}

					auto layer = context.chunkMesh.GetLayer(Default);
					auto layer2 = context.chunkMesh.GetLayer(Lucency);
					if (layer && layer2) {
						layer->FreeMesh(x, y, z);
						layer2->FreeMesh(x, y, z);

						for (size_t i = 0; i < 6; i++)
						{
							layer->FreeMesh(x + NeighBours[i][0], y + NeighBours[i][1], z + NeighBours[i][2]);
							layer2->FreeMesh(x + NeighBours[i][0], y + NeighBours[i][1],z + NeighBours[i][2]);
						}
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
	}
}