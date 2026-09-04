#pragma once 

#include "render/mesh_data.h"
#include <voxel/chunk/chunk.h>
#include <voxel/block/block_library.h>

namespace nova_engine::render::utils {
	class MeshBuilder {
	public:
		static MeshData BuildMeshFromChunk(const voxel::chunk::Chunk& chunk,const voxel::block::BlockLibrary& library);

		//重载，支持邻居,传入的邻居顺序必须严格按照映射表
		static MeshData BuildMeshFromChunk(const voxel::chunk::Chunk& chunk,const std::array<voxel::chunk::Chunk*,6> neighbours,const voxel::block::BlockLibrary& library);

	private:
		static const std::array<std::array<int, 3>, 6> neighbours_;

		static inline void AddQuadMesh(const voxel::block::QuadFace& face,
			std::vector<float>& vertices,
			std::vector<int>& indices,
			int& vertexCount, int x, int y, int z);

		static void GetNeighTable();
	};
}