#pragma once

#include "chunk.h"
#include <collection/data_structure/octree.h>
#include <collection/data_structure/object_pool.h>
#include <memory>
#include <unordered_map>
#include <core/config/config.h>

#include <glm/vec3.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>


namespace nova_engine::voxel::chunk {
	//世界中的所有块数据都存储在ChunkData中，ChunkData使用八叉树来管理所有的块数据
	class ChunkData {
	public:
		ChunkData() = default;
		~ChunkData() = default;

		ChunkData(const ChunkData&) = delete;
		ChunkData& operator=(const ChunkData&) = delete;

		//查找指定坐标的块，如果不存在则返回nullptr
		Chunk* FindChunk(int x, int y, int z) const;

		//转移区块所有权到ChunkData中，ChunkData将负责管理该区块的生命周期
		void SubmitChunk(int x, int y, int z, std::unique_ptr<Chunk> chunk);
		//删除指定坐标的块，回收到对象池中，如果不存在则不做任何操作
		void RemoveChunk(int x, int y, int z);
		//返回指定坐标区块的六个邻居的临时指针，若邻居不存在则填充nullptr
		const std::array<Chunk*, 6> GetChunkNeighbours(int x, int y, int z) const;
		//清空所有块数据
		void Clear();
		//从对象池中获取一个块，如果对象池中没有，则创建一个新的块
		std::unique_ptr<Chunk> AcquireChunk();
		//获取容器区块数量O(n)时间查找
		size_t Size() const;
		size_t PoolSize() const {
			return chunkPool_.Size();
		}
	private:
		std::unordered_map<glm::ivec3, std::unique_ptr<Chunk>> map_;
		collection::data_structure::ObjectPool<Chunk, core::config::POOL_SIZE> chunkPool_;
		size_t size_ = 0;

		glm::ivec3 GetKeyWithCoord(int x, int y, int z) const {
			return {
				x >> core::config::CHUNK_BIT1,
				y >> core::config::CHUNK_BIT1,
				z >> core::config::CHUNK_BIT1
			};
		}
	};
}