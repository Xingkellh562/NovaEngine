#include "chunk_data.h"
#include <array>

namespace nova_engine::voxel::chunk {
	//这里统一传入绝对世界坐标
	Chunk<int>* ChunkData::FindChunk(int x, int y, int z) const{
		auto key = GetKeyWithCoord(x,y,z);

		auto it = map_.find(key);
		if (it != map_.end()) {
			return it->second.get();
		}
		return nullptr;
	}

	std::unique_ptr<Chunk<int>> ChunkData::AcquireChunk() {
		return chunkPool_.Acquire(32);
	}

	void ChunkData::SubmitChunk(int x,int y,int z, std::unique_ptr<Chunk<int>> chunk) {
		if (!chunk) return;

		//chunk-> std::unique_ptr<>
		auto key = GetKeyWithCoord(x, y, z);
		auto it = map_.find(key);
		if (it != map_.end()) {
			//原数据入池
			chunkPool_.Recycle(std::move(it->second));
			it->second = std::move(chunk);
		}
		else {
			map_.emplace(key, std::move(chunk));
		}
		size_++;
	}

	void ChunkData::RemoveChunk(int x, int y, int z) {
		auto key = GetKeyWithCoord(x, y, z);
		auto it = map_.find(key);
		if (it != map_.end()) {
			//数据入池
			chunkPool_.Recycle(std::move(it->second));
			size_--;
		}
	}

	const std::array<Chunk<int>*, 6> ChunkData::GetChunkNeighbours(int x, int y, int z) const {
		static const std::array<std::array<int, 3>, 6> NeighBours = {
		{{-1,0,0},{1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}
		};

		const int CHUNK_BIT1 = core::config::CHUNK_BIT1;

		std::array<Chunk<int>*, 6> results{};

		for (int i = 0;i < 6;i++) {
			std::array<int, 3> nei = NeighBours[i];

			results[i] = FindChunk((nei[0] << CHUNK_BIT1) + x, (nei[1] << CHUNK_BIT1) + y, (nei[2] << CHUNK_BIT1) + z);
		}

		return results;
	}

	size_t ChunkData::Size() const {
		return size_;
	}

	//把所有的数据清空，并且释放对象池中的所有块
	void ChunkData::Clear() {
		map_.clear();
		chunkPool_.Shrink();
	}
}