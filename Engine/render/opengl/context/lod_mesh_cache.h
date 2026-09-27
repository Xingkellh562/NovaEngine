#pragma once 

#include <unordered_map>
#include <collection/data_structure/adaptive_lod_octree.h>
#include <render/opengl/glmesh.h>
#include <core/config/config.h>
#include <array>
#include <ranges>

#include <glm/vec3.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

namespace nova_engine::render::opengl::context {
	//以八叉树结构组织Lod网格
	class LodMeshCache {
	public:
		LodMeshCache() = default;
		~LodMeshCache() = default;

		LodMeshCache(const LodMeshCache&) = delete;
		LodMeshCache& operator=(const LodMeshCache&) = delete;

		GLMesh* Find(int x,int y,int z,int* depth = nullptr) const;

		bool TryEmplace(int x,int y,int z,const MeshData& data);

		bool TrySubdivide(int x, int y, int z, int depth, std::array<MeshData*, 8>);

		bool TryCombine(int x, int y, int z, int depth, const MeshData* data);

		bool TryDeleteOctree(int x, int y, int z);

		auto AllData() const {
			return datas_
				| std::views::values
				| std::views::transform(
					[](const collection::data_structure::AdaptiveLodOctree<
						GLMesh, core::config::OCTREE_SIZE, core::config::CHUNK_SIZE>& octree)
					-> const auto& {
						return octree.Data();
					})
				| std::views::join
				| std::views::filter([](const std::unique_ptr<GLMesh>& p) {
				return p != nullptr;
					})
				| std::views::transform([](const std::unique_ptr<GLMesh>& p)
					-> const GLMesh& {
						return *p;
					});
		}

		const auto& AllOctree() const {
			return datas_;
		}
	private: 
		std::unordered_map < glm::ivec3,collection::data_structure::AdaptiveLodOctree
			<GLMesh,core::config::OCTREE_SIZE, core::config::CHUNK_SIZE> > datas_;

		glm::ivec3 GetKeyWithCoo(int x, int y, int z) const {
			const int bit = core::config::OCTREE_BIT1;
			return{ x >> bit,y >> bit,z >> bit };
		}
	};

}