#pragma once

#include "mesh_instance.h"

#include <render/core/mesh_data.h>
#include <core/config/config.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <unordered_set>
#include <memory>

namespace nova_engine::render::device { class IRenderDevice; }
namespace nova_engine::render::scene {
	enum class MeshLayer : uint32_t {
		Default = 0,
		Lucency = 1,
		Liquid = 2,
		Count = 3,
	};

	//缓存并维护所有Mesh对象
	class ChunkMeshRegistry {
	public:
		explicit ChunkMeshRegistry(device::IRenderDevice& device)
			:device_(device) {}

		~ChunkMeshRegistry();

		ChunkMeshRegistry(const ChunkMeshRegistry&) = delete;
		ChunkMeshRegistry& operator=(const ChunkMeshRegistry&) = delete;
		//通过坐标哈希只读查找mesh是否存在，用于一些外部逻辑
		bool Exist(int x, int y, int z) const;
		//只读查找Mesh对象
		const MeshInstance* FindMesh(int x, int y, int z) const;
		//失活对应mesh，索引移入池，如果不存在则不做任何操作
		void FreeMesh(int x, int y, int z);
		//清空所有数据
		void Clear();
		//直接提交顶点数据，内部自己判断该更新还是添加
		void SubmitMesh(int x, int y, int z, const core::MeshData& data);

		void ReleasePool();
		//获取Mesh数组用于渲染遍历
		const std::vector<MeshInstance>& GetAllMesh() const { return meshs_; }

		void RequireMeshUpdate(int x, int y, int z);

		bool GetUpdateRequirement(glm::ivec3& out);

		inline int PoolLength() const { return pool_.size(); }
		inline int MeshCount() const { return meshs_.size(); }
	private:
		device::IRenderDevice& device_;

		std::unordered_map<glm::vec3, size_t> indexMap_;
		std::vector<MeshInstance> meshs_;
		std::vector<glm::ivec3> needUpdate_;
		std::unordered_set<glm::ivec3> buildSet_;
		std::vector<size_t> pool_;

		//获取索引
		size_t AcquireSlotInternal();
	};

	class ChunkMeshSet {
	public:
		explicit ChunkMeshSet(device::IRenderDevice& device, uint32_t layerCount = static_cast<uint32_t>(MeshLayer::Count));

		ChunkMeshRegistry* Layer(MeshLayer layer);
		const ChunkMeshRegistry* Layer(MeshLayer layer)const;

	private:
		std::vector<std::unique_ptr<ChunkMeshRegistry>> layers_;
	};
}