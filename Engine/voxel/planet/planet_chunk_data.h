#pragma once
#include <voxel/chunk/chunk.h>
#include <collection/data_structure/octree.h>
#include <collection/data_structure/object_pool.h>
#include <memory>
#include <vector>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

namespace nova_engine::voxel::planet {
	class PlanetChunkData {
	public:
		PlanetChunkData(size_t size);
		~PlanetChunkData();

		PlanetChunkData(const PlanetChunkData&) = delete;
		PlanetChunkData& operator=(const PlanetChunkData&) = delete;

		void SubmitChunk(int x,int y,int z, std::unique_ptr<chunk::Chunk>);
		void RemoveChunk(int x, int y, int z);

		const chunk::Chunk* FindChunk(int x,int y,int z) const;

		void Clear();

		const std::array<chunk::Chunk*, 6> GetChunkNeighbours(int x, int y, int z) const;
		//从对象池中获取一个块，如果对象池中没有，则创建一个新的块
		std::unique_ptr<chunk::Chunk> AcquireChunk();
	private:
		std::vector<collection::data_structure::Octree<chunk::Chunk, 2048, 32>> chunks_;
		collection::data_structure::ObjectPool<chunk::Chunk, 1024> pool_;
		size_t planetSize_, chunkCount_,poolLength_;

		//球面立方体，f [0，5] 表示正方体的面编号,uv表示八叉树根节点在每个平面的偏移，原点为左下角，始终为正
		size_t MapPlanetPosToAddress(int f, int u, int v) const {
			return f * planetSize_ * planetSize_ + u * planetSize_ + v;
		}
		//将球面的经纬转换为八叉树存储坐标
		glm::ivec3 PlanetCooTrans(float x, float z) const;

		glm::vec2 TransToLonLatCoo(int x, int z) const;

		size_t GetIndex(float x, float y) const {
			auto fuv = PlanetCooTrans(x, y);
			return MapPlanetPosToAddress(fuv.x, fuv.y, fuv.z);
		}
	};
}