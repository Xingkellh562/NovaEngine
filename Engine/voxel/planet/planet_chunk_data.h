#pragma once
#include <voxel/chunk/chunk.h>
#include <core/config/config.h>
#include <collection/data_structure/octree.h>
#include <collection/data_structure/object_pool.h>
#include <memory>
#include <vector>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>


namespace nova_engine::voxel::planet {
	

	class PlanetChunkData{
	public:
		PlanetChunkData() = default;
		~PlanetChunkData() = default;

		PlanetChunkData(const PlanetChunkData&) = delete;
		PlanetChunkData& operator=(const PlanetChunkData&) = delete;

		//查找指定坐标的块，如果不存在则返回nullptr
		chunk::Chunk<float>* FindChunk(int x, int y, int z) const;
		//查询指定坐标体素的密度值，默认外部(失败返回1.0)，适合少量查找
		float GetVoxelDensity(int x, int y, int z) const;
		//转移区块所有权到ChunkData中，ChunkData将负责管理该区块的生命周期
		void SubmitChunk(int x, int y, int z, std::unique_ptr<chunk::Chunk<float>> chunk);
		//删除指定坐标的块，回收到对象池中，如果不存在则不做任何操作
		void RemoveChunk(int x, int y, int z);
		//返回指定坐标区块的七个邻居的临时指针，若邻居不存在则填充nullptr
		const std::array<chunk::Chunk<float>*, 7> GetChunkNeighbours(int x, int y, int z) const;

		bool NeighboursExist(int x, int y, int z) const;
		//清空所有块数据
		void Clear();
		//从对象池中获取一个块，如果对象池中没有，则创建一个新的块
		std::unique_ptr<chunk::Chunk<float>> AcquireChunk();

		void RecycleChunk(std::unique_ptr<chunk::Chunk<float>>);
		//获取容器区块数量O(n)时间查找
		size_t Size() const;
		size_t PoolSize() const {
			return chunkPool_.Size();
		}

		void SignDirty(glm::ivec3 pos);
		bool GetDirtyChunks(glm::ivec3& pos);
	private:
		std::unordered_map<glm::ivec3, std::unique_ptr<chunk::Chunk<float>>> map_;
		collection::data_structure::ObjectPool<chunk::Chunk<float>, core::config::POOL_SIZE> chunkPool_;

		std::vector<glm::ivec3> dirtyChunks_;
		size_t size_ = 0;

		glm::ivec3 GetKeyWithCoord(int x, int y, int z) const {
			return {
				x >> core::config::CHUNK_BIT1,
				y >> core::config::CHUNK_BIT1,
				z >> core::config::CHUNK_BIT1
			};
		}

		using const_iterator = std::unordered_map<glm::ivec3, std::unique_ptr<chunk::Chunk<float>>>::const_iterator;

	public:
		const_iterator begin() const noexcept { return map_.begin(); }
		const_iterator end() const noexcept { return map_.end(); }

		const_iterator cbegin() const noexcept { return map_.cbegin(); }
		const_iterator cend() const noexcept { return map_.cend(); }
	};
}