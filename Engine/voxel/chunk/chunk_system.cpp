#include "chunk_system.h"
#include <utils/spatial/chunk_range_finder.h>
#include <glm/gtx/norm.hpp>
#include <voxel/terrain/terrain_generator.h>


namespace nova_engine::voxel::chunk {
	void ChunkSystem::BuildChunk(ChunkData& chunkData, int x, int y, int z) {
		static const terrain::TerrainGenerator generator(123);
		//将转入的绝对坐标修正为区块原点(左下后)(区块大小32)，可处理负数
		int fx = x & ~31, fy = y & ~31, fz = z & ~31;
		//从池中获取一个可供修改的对象
		std::unique_ptr<Chunk> chunkPtr = chunkData.AcquireChunk();

		if (chunkPtr != nullptr) {
			generator.BuildChunk(*chunkPtr, x, y, z);
			//将对象提交给data容器管理
			chunkData.SubmitChunk(fx, fy, fz, std::move(chunkPtr));
		}
	}

	static const std::vector<glm::ivec3> chunksPos = utils::spatial::FindChunkInRange(320);

	void ChunkSystem::Update(nova_engine::core::world::World& world) {
		glm::ivec3 playerPos = world.playerPos;
		const int MAXCOUNT = 6;
		
		int count = 0;
		for (const glm::ivec3& chunkPos : chunksPos) {
			if (count >= MAXCOUNT) break;
			int cx = chunkPos.x + playerPos.x, 
				cy = chunkPos.y + playerPos.y, 
				cz = chunkPos.z + playerPos.z;
			auto chunk = world.chunkData.FindChunk(cx,cy,cz);
			const glm::ivec3& cp = chunkPos;
			if (((cp.x * cp.x + cp.y * cp.y + cp.z * cp.z) < 256 * 256) && !chunk) {
				count++;
				BuildChunk(world.chunkData, cx, cy, cz);
			}
			else if(((cp.x * cp.x + cp.y * cp.y + cp.z * cp.z) >= 280 * 280) && chunk)
				world.chunkData.RemoveChunk(cx, cy, cz);
		}

		//BuildChunk(world.chunkData, 0, 32, 0);
		//BuildChunk(world.chunkData, 0, 64, 0);
	}
}