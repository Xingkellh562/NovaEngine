#pragma once 

#include <render/core/mesh_data.h>
#include <voxel/chunk/chunk.h>
#include <voxel/block/block_library.h>
#include <voxel/lod/lod_voxel_data.h>

namespace nova_engine::render::utils {
	class MeshBuilder {
	public:
		//支持邻居,传入的邻居顺序必须严格按照映射表
		static std::array<std::optional<core::MeshData>, 2> BuildMeshFromChunk(const voxel::chunk::Chunk<int>& chunk,const std::array<voxel::chunk::Chunk<int>*,6> neighbours,const voxel::block::BlockLibrary& library);

		static core::MeshData BuildLodMesh(const voxel::chunk::Chunk<int>& chunk, size_t scale,const voxel::block::BlockLibrary& library);

		static core::MeshData BuildLodMesh(const voxel::lod::LodVoxelData& lodData, size_t scale, const voxel::block::BlockLibrary& library);
	private:
		static const std::array<std::array<int, 3>, 6> neighbours_;
		static const std::array<std::array<std::array<int, 3>, 12>, 6> aos_;

		static inline void AddQuadMesh(const voxel::block::QuadFace& face,
			std::vector<float>& vertices,
			std::vector<float>& uvws,
			std::vector<float>& aos,
			std::vector<float>& normals,
			std::vector<uint32_t>& indices,
			int& vertexCount,
			const std::array<float, 4>& aoArray,
			int x, int y, int z,int nx, int ny, int nz,int scale = 1);

		static inline const std::array<float,4>& GetAOValueForMesh(int idx,int dir);
	};
}