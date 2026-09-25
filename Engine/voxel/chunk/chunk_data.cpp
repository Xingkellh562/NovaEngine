#include "chunk_data.h"
#include <array>

namespace nova_engine::voxel::chunk {
	//这里统一传入绝对世界坐标
	Chunk* ChunkData::FindChunk(int x, int y, int z) const{
		//计算八叉树key坐标(涉及负数)
		int ox = x >> core::config::OCTREE_BIT1,
			oy = y >> core::config::OCTREE_BIT1,
			oz = z >> core::config::OCTREE_BIT1;
		//计算八叉树内的局部坐标
		int cx = x & (core::config::OCTREE_SIZE - 1),
			cy = y & (core::config::OCTREE_SIZE - 1),
			cz = z & (core::config::OCTREE_SIZE - 1);

		auto it = chunkMap_.find(glm::ivec3(ox, oy, oz));
		if (it != chunkMap_.end()) {
			return it->second.Find(cx, cy, cz);
		}
		return nullptr;
	}

	int ChunkData::GetDepth(int x, int y, int z) const {
		//计算八叉树key坐标(涉及负数)
		int ox = x >> core::config::OCTREE_BIT1,
			oy = y >> core::config::OCTREE_BIT1,
			oz = z >> core::config::OCTREE_BIT1;
		//计算八叉树内的局部坐标
		int cx = x & (core::config::OCTREE_SIZE - 1),
			cy = y & (core::config::OCTREE_SIZE - 1),
			cz = z & (core::config::OCTREE_SIZE - 1);

		auto it = chunkMap_.find(glm::ivec3(ox, oy, oz));
		if (it != chunkMap_.end()) {
			return it->second.GetDepth(cx, cy, cz);
		}
		return 0;
	}

	std::unique_ptr<Chunk> ChunkData::AcquireChunk() {
		return chunkPool_.Acquire(32);
	}

	void ChunkData::SubmitChunk(int x,int y,int z, std::unique_ptr<Chunk> chunk) {
		if (!chunk) return;

		int ox = x >> core::config::OCTREE_BIT1,
			oy = y >> core::config::OCTREE_BIT1,
			oz = z >> core::config::OCTREE_BIT1;
		int cx = x & (core::config::OCTREE_SIZE - 1),
			cy = y & (core::config::OCTREE_SIZE - 1),
			cz = z & (core::config::OCTREE_SIZE - 1);

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
		int ox = x >> core::config::OCTREE_BIT1, 
			oy = y >> core::config::OCTREE_BIT1, 
			oz = z >> core::config::OCTREE_BIT1;
		int cx = x & (core::config::OCTREE_SIZE - 1), 
			cy = y & (core::config::OCTREE_SIZE - 1),
			cz = z & (core::config::OCTREE_SIZE - 1);

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

		const int CHUNK_BIT1 = core::config::CHUNK_BIT1;

		std::array<Chunk*, 6> results{};

		for (int i = 0;i < 6;i++) {
			std::array<int, 3> nei = NeighBours[i];

			results[i] = FindChunk((nei[0] << CHUNK_BIT1) + x, (nei[1] << CHUNK_BIT1) + y, (nei[2] << CHUNK_BIT1) + z);
		}

		return results;
	}

	size_t ChunkData::Size() const {
		size_t size = 0;
		for (const auto& Oct : chunkMap_) {
			size += Oct.second.Size();
		}
		return size;
	}

	//把所有的数据清空，并且释放对象池中的所有块
	void ChunkData::Clear() {
		chunkMap_.clear();
		chunkPool_.Shrink();
	}
}