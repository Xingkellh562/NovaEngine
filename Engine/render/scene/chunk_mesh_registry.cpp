#include "chunk_mesh_registry.h"
#include <render/device/render_device.h>
#include <core/config/config.h>
#include <log/logger.h>

namespace nova_engine::render::scene {
	namespace {
		inline glm::vec3 MakeKey(int x, int y, int z) {
			const int bit = nova_engine::core::config::CHUNK_BIT1;
			return{ static_cast<float>(x >> bit),
				    static_cast<float>(y >> bit),
				    static_cast<float>(z >> bit) };
		}

	}
	ChunkMeshRegistry::~ChunkMeshRegistry() {
		meshs_.clear();
	}


	bool ChunkMeshRegistry::Exist(int x, int y, int z)  const {
		return  indexMap_.find(MakeKey(x, y, z)) != indexMap_.end();
	}

	const MeshInstance* ChunkMeshRegistry::FindMesh(int x, int y, int z) const {
		auto it = indexMap_.find(MakeKey(x,y,z));
		if (it != indexMap_.end()) {
			return &meshs_[it->second];
		}
		else return nullptr;
	}

	void ChunkMeshRegistry::SubmitMesh(int x, int y, int z, const core::MeshData& data) {
		const int CHUNK_BIT = nova_engine::core::config::CHUNK_BIT1;
		const int CHUNK_MASK = nova_engine::core::config::CHUNK_SIZE - 1;

		glm::vec3 key = MakeKey(x,y,z);

		auto it = indexMap_.find(key);
		size_t idx = 0;
		if (it == indexMap_.end()) {
			// 新区块：分配槽位
			idx = AcquireSlotInternal();
			indexMap_.emplace(key, idx);
		}
		else {
			// 已存在：复用现有槽位
			idx = it->second;
		}
		
		if (idx >= meshs_.size()) {
			NE_CORE_ERROR("RenderSystem", "ChunkMeshCache::SubmitMesh: Index {} out of bounds (size: {})", idx, meshs_.size());
			return;
		} //边界检查(保险起见)

		MeshInstance& inst = meshs_[idx];
		inst.position = glm::vec3( static_cast<float>(x & ~CHUNK_MASK),
								   static_cast<float>(y & ~CHUNK_MASK),
								   static_cast<float>(z & ~CHUNK_MASK));

		inst.rotation = glm::vec3(0.0f, 0.0f, -1.0f);
		//填充数据时自动设置索引数量
		if (!data.IsValid()) {
			if (inst.mesh.Valid()) device_.UpdateMesh(inst.mesh, core::MeshData{});
			inst.Sleep();
			return;
		}

		if (!inst.mesh.Valid())
			inst.mesh = device_.CreateMesh(data);
		else
			device_.UpdateMesh(inst.mesh, data);
		inst.Wake();
	}

	void ChunkMeshRegistry::FreeMesh(int x, int y, int z) {
		glm::vec3 key = MakeKey(x,y,z);

		auto it = indexMap_.find(key);
		if (it == indexMap_.end()) return;

		const size_t idx = it->second;
		//边界检查(保险起见)
		if (idx >= meshs_.size()) return;
		//失活(把索引数量归零，这样外部渲染器不会渲染它)
		meshs_[idx].Sleep();
		//入池
		pool_.push_back(idx);
		indexMap_.erase(it);
	}

	void ChunkMeshRegistry::Clear() {
		indexMap_.clear();
		pool_.clear();
		for (size_t i = 0;i < meshs_.size();i++) {
			pool_.push_back(i);
			meshs_[i].Sleep();
		}
	}

	void ChunkMeshRegistry::ReleasePool() {
		for (size_t i = 0;i < pool_.size();i++) {
			size_t poolIdx = pool_[i];
			if (poolIdx < meshs_.size()) {
				device_.DestroyMesh(meshs_[poolIdx].mesh);
				meshs_[poolIdx].mesh = {};
			}
		}
	}

	void ChunkMeshRegistry::RequireMeshUpdate(int x, int y, int z) {
		const int CHUNK_BIT = nova_engine::core::config::CHUNK_BIT1;
		int fx = (x >> CHUNK_BIT) << CHUNK_BIT,
			fy = (y >> CHUNK_BIT) << CHUNK_BIT,
			fz = (z >> CHUNK_BIT) << CHUNK_BIT;

		glm::ivec3 pos = { fx,fy,fz };

		if (buildSet_.insert(pos).second) {
			needUpdate_.push_back(pos);
		}
	}

	bool ChunkMeshRegistry::GetUpdateRequirement(glm::ivec3& out) {
		if (needUpdate_.empty()) return false;
		out = needUpdate_.back();
		needUpdate_.pop_back();
		buildSet_.erase(out);
		return true;
	}

	size_t  ChunkMeshRegistry::AcquireSlotInternal() {
		if (pool_.empty()) {
			meshs_.emplace_back();
			return meshs_.size() - 1;
		}

		size_t idx = pool_.back();
		pool_.pop_back();
		meshs_[idx].Sleep();
		return idx;
	}

	ChunkMeshSet::ChunkMeshSet(device::IRenderDevice& device, uint32_t layerCount) {
		for (uint32_t i = 0;i < layerCount;i++) {
			layers_.push_back(std::make_unique<ChunkMeshRegistry>(device));
		}
	}

	ChunkMeshRegistry* ChunkMeshSet::Layer(MeshLayer layer) {
		const auto i = static_cast<size_t>(layer);
		return i < layers_.size() ? layers_[i].get() : nullptr;
	}

	const ChunkMeshRegistry* ChunkMeshSet::Layer(MeshLayer layer) const {
		const auto i = static_cast<size_t>(layer);
		return i < layers_.size() ? layers_[i].get() : nullptr;
	}
}