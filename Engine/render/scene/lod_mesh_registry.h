#pragma once

#include <render/core/handles.h>
#include <render/core/mesh_data.h>

#include "mesh_instance.h"

#include <collection/data_structure/adaptive_lod_octree.h>
#include <core/config/config.h>

#include <array>
#include <memory>
#include <ranges>
#include <unordered_map>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/vec3.hpp>

namespace nova_engine::render::device { class IRenderDevice; }

namespace nova_engine::render::scene {

	// LOD 网格账本：以八叉树组织，支持细分（Subdivide）与合并（Combine）。
	//
	// 与改动前 LodMeshCache 的唯一区别：节点数据从 GLMesh 变为 MeshInstance。
	// 八叉树本身是模板，改动量为 0。
	class LodMeshRegistry {
	public:
		explicit LodMeshRegistry(device::IRenderDevice& device) : device_(device) {}
		~LodMeshRegistry() = default;

		LodMeshRegistry(const LodMeshRegistry&) = delete;
		LodMeshRegistry& operator=(const LodMeshRegistry&) = delete;

		MeshInstance* Find(int x, int y, int z, int* depth = nullptr) const;

		bool TryEmplace(int x, int y, int z, const core::MeshData& data);
		bool TrySubdivide(int x, int y, int z, int depth, std::array<core::MeshData*, 8> datas);
		bool TryCombine(int x, int y, int z, int depth, const core::MeshData* data);
		bool TryDeleteOctree(int x, int y, int z);

		// 扁平遍历视图：给渲染 Pass 用。与改动前 LodMeshCache::AllData() 等价。
		auto AllData() const {
			return datas_
				| std::views::values
				| std::views::transform(
					[](const Octree& octree) -> const auto& { return octree.Data(); })
				| std::views::join
				| std::views::filter([](const std::unique_ptr<MeshInstance>& p) {
				return p != nullptr; })
				| std::views::transform([](const std::unique_ptr<MeshInstance>& p)
					-> const MeshInstance& { return *p; });
		}

		const auto& AllOctree() const { return datas_; }

	private:
		using Octree = collection::data_structure::AdaptiveLodOctree<
			MeshInstance, nova_engine::core::config::OCTREE_SIZE, nova_engine::core::config::CHUNK_SIZE>;

		device::IRenderDevice& device_;
		std::unordered_map<glm::ivec3, Octree> datas_;

		glm::ivec3 GetKeyWithCoo(int x, int y, int z) const {
			const int bit = nova_engine::core::config::OCTREE_BIT1;
			return { x >> bit, y >> bit, z >> bit };
		}

		// 统一的实例构造：分配 MeshInstance + 上传 GPU 网格 + 设置位置朝向。
		// 改动前这段逻辑在三处重复（lod_mesh_cache.cpp:25-27, 66-73, 93-100）。
		std::unique_ptr<MeshInstance> MakeInstance(const core::MeshData& data,
			const glm::vec3& position);
	};

}