#include "chunk_data.h"
#include <array>

namespace nova_engine::voxel::chunk {
	//这里统一传入绝对世界坐标
	Chunk* ChunkData::FindChunk(int x, int y, int z) const{
		//计算八叉树key坐标(涉及负数)
		int ox = x >> 11,oy = y >> 11,oz = z >> 11;
		//计算八叉树内的局部坐标
		int cx = x & 0x7FF, cy = y & 0x7FF, cz = z & 0x7FF;

		auto it = chunkMap_.find(glm::ivec3(ox, oy, oz));
		if (it != chunkMap_.end()) {
			return it->second.Find(cx, cy, cz);
		}
		return nullptr;
	}

	std::unique_ptr<Chunk> ChunkData::AcquireChunk() {
		return chunkPool_.Acquire(32);
	}

	void ChunkData::SubmitChunk(int x,int y,int z, std::unique_ptr<Chunk> chunk) {
		if (!chunk) return;

		int ox = x >> 11, oy = y >> 11, oz = z >> 11;
		int cx = x & 0x7FF, cy = y & 0x7FF, cz = z & 0x7FF;

		auto octree = chunkMap_.find(glm::ivec3(ox, oy, oz));
		if (octree != chunkMap_.end()) {
			octree->second.Add(cx, cy, cz, std::move(*chunk));
		} else {
			collection::data_structure::Octree<Chunk, 2048, 32> newOctree;
			newOctree.Add(cx, cy, cz, std::move(*chunk));
			chunkMap_.emplace(glm::ivec3(ox, oy, oz), std::move(newOctree));
		}
	}

	void ChunkData::RemoveChunk(int x, int y, int z) {
		int ox = x >> 11, oy = y >> 11, oz = z >> 11;
		int cx = x & 0x7FF, cy = y & 0x7FF, cz = z & 0x7FF;

		auto it = chunkMap_.find(glm::ivec3(ox, oy, oz));
		if (it != chunkMap_.end()) {
			auto ptr = it->second.Remove(cx, cy, cz);
			if (ptr) {
				chunkPool_.Recycle(std::move(ptr));
			}
		}
	}

	const std::array<Chunk*, 6> ChunkData::GetChunkNeighbours(int x, int y, int z) const {
		const std::array<std::array<int, 3>, 6> NeighBours = {
		{{-1,0,0},{1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}
		};

		std::array<Chunk*, 6> results{};

		for (int i = 0;i < 6;i++) {
			std::array<int, 3> nei = NeighBours[i];

			results[i] = FindChunk(nei[0] * 32 + x, nei[1] * 32 + y, nei[2] * 32 + z);
		}

		return results;
	}

	//把所有的数据清空，并且释放对象池中的所有块
	void ChunkData::Clear() {
		chunkMap_.clear();
		chunkPool_.Shrink();
	}
}