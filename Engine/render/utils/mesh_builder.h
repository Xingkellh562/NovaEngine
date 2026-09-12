#pragma once 

#include "render/mesh_data.h"
#include <voxel/chunk/chunk.h>
#include <voxel/block/block_library.h>

namespace nova_engine::render::utils {
	class MeshBuilder {
	public:
		//支持邻居,传入的邻居顺序必须严格按照映射表
		static std::array<std::optional<MeshData>, 2> BuildMeshFromChunk(const voxel::chunk::Chunk& chunk,const std::array<voxel::chunk::Chunk*,6> neighbours,const voxel::block::BlockLibrary& library);

	private:
		static const std::array<std::array<int, 3>, 6> neighbours_;
		static const std::array<std::array<std::array<int, 3>, 12>, 6> aos_;

		static inline void AddQuadMesh(const voxel::block::QuadFace& face,
			std::vector<float>& vertices,
			std::vector<float>& uvws,
			std::vector<float>& aos,
			std::vector<int>& indices,
			int& vertexCount,
			const std::array<float, 4>& aoArray,
			int x, int y, int z);

		static inline const std::array<float,4>& GetAOValueForMesh(int idx,int dir);
	};
}