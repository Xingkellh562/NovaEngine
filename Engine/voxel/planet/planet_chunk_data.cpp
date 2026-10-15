#include "planet_chunk_data.h"
#include <algorithm>
#include <array>

namespace nova_engine::voxel::planet {
	//这里统一传入绝对世界坐标
	chunk::Chunk<float>* PlanetChunkData::FindChunk(int x, int y, int z) const {
		auto key = GetKeyWithCoord(x, y, z);

		auto it = map_.find(key);
		if (it != map_.end()) {
			return it->second.get();
		}
		return nullptr;
	}

	std::unique_ptr<chunk::Chunk<float>> PlanetChunkData::AcquireChunk() {
		return chunkPool_.Acquire(32);
	}

	void PlanetChunkData::SubmitChunk(int x, int y, int z, std::unique_ptr<chunk::Chunk<float>> chunk) {
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

	void PlanetChunkData::RemoveChunk(int x, int y, int z) {
		auto key = GetKeyWithCoord(x, y, z);
		auto it = map_.find(key);
		if (it != map_.end()) {
			//数据入池
			chunkPool_.Recycle(std::move(it->second));
			size_--;
		}
	}

	const std::array<const chunk::Chunk<float>*, 7> PlanetChunkData::GetChunkNeighbours(int x, int y, int z) const {
		static const std::array<std::array<int, 3>, 7> NeighBours = {
		{{1,0,0},{0,1,0},{1,1,0},{0,0,1},{1,0,1},{0,1,1},{1,1,1}}
		};

		const int CHUNK_BIT1 = core::config::CHUNK_BIT1;

		std::array<const chunk::Chunk<float>*, 7> results{};

		for (int i = 0;i < 7;i++) {
			std::array<int, 3> nei = NeighBours[i];

			results[i] = FindChunk((nei[0] << CHUNK_BIT1) + x, (nei[1] << CHUNK_BIT1) + y, (nei[2] << CHUNK_BIT1) + z);
		}

		return results;
	}

	bool PlanetChunkData::NeighboursExist(int x, int y, int z) const {
		const int CHUNK_BIT1 = core::config::CHUNK_BIT1;
		for(int nx = -1;nx <= 1;nx++)
			for (int ny = -1;ny <= 1;ny++)
				for (int nz = -1;nz <= 1;nz++) {
					bool exist = (FindChunk((nx << CHUNK_BIT1) + x, (ny << CHUNK_BIT1) + y, (nz << CHUNK_BIT1) + z) != nullptr);
					if (!exist) { return false; }
				}

		return true;
	}

	void PlanetChunkData::SignDirty(glm::ivec3 pos) {
		glm::ivec3 key = GetKeyWithCoord(pos.x, pos.y, pos.z);
		dirtyChunks_.push_back(key);
	}
	bool PlanetChunkData::GetDirtyChunks(glm::ivec3& pos) {
		bool empty = dirtyChunks_.empty();
		pos = glm::ivec3(0.0);
		if (!empty) {
			auto chunkPos = dirtyChunks_.back();
			pos = {
				chunkPos.x << core::config::CHUNK_BIT1,
				chunkPos.y << core::config::CHUNK_BIT1,
				chunkPos.z << core::config::CHUNK_BIT1
			};
			dirtyChunks_.pop_back();
		}
		return !empty;
	}

	size_t PlanetChunkData::Size() const {
		return size_;
	}

	//把所有的数据清空，并且释放对象池中的所有块
	void PlanetChunkData::Clear() {
		map_.clear();
		chunkPool_.Shrink();
	}
}