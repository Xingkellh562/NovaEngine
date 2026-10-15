#include "lod_mesh_registry.h"

#include <render/device/render_device.h>

namespace nova_engine::render::scene {

	std::unique_ptr<MeshInstance> LodMeshRegistry::MakeInstance(const core::MeshData& data,
		const glm::vec3& position) {
		auto inst = std::make_unique<MeshInstance>(device_);
		inst->mesh = device_.CreateMesh(data);
		inst->position = position;
		inst->rotation = glm::vec3(0.0f, 0.0f, -1.0f);
		return inst;
	}

	MeshInstance* LodMeshRegistry::Find(int x, int y, int z, int* depth) const {
		const int mask = nova_engine::core::config::OCTREE_SIZE - 1;
		const glm::ivec3 key = GetKeyWithCoo(x, y, z);

		auto it = datas_.find(key);
		if (it == datas_.end()) {
			if (depth) *depth = 0;
			return nullptr;
		}
		return it->second.Find(x & mask, y & mask, z & mask, depth);
	}

	bool LodMeshRegistry::TryEmplace(int x, int y, int z, const core::MeshData& data) {
		const glm::ivec3 key = GetKeyWithCoo(x, y, z);

		auto it = datas_.find(key);
		if (it == datas_.end()) return false;

		const int mask = nova_engine::core::config::OCTREE_SIZE - 1;
		const glm::vec3 pos = glm::vec3(x & ~mask, y & ~mask, z & ~mask);

		if (data.vertices.empty()) {
			return it->second.TryEmplace(x & mask, y & mask, z & mask, nullptr);
		}
		return it->second.TryEmplace(x & mask, y & mask, z & mask,
			MakeInstance(data, pos));
	}

	bool LodMeshRegistry::TrySubdivide(int x, int y, int z, int depth,
		std::array<core::MeshData*, 8> datas) {
		static const std::array<glm::ivec3, 8> kChildOffset = { {
			{ 0,0,0 },{ 1,0,0 },{ 0,1,0 },{ 1,1,0 },
			{ 0,0,1 },{ 1,0,1 },{ 0,1,1 },{ 1,1,1 }
		} };

		const int size = nova_engine::core::config::OCTREE_SIZE >> depth;
		const int mask = nova_engine::core::config::OCTREE_SIZE - 1;
		const glm::ivec3 key = GetKeyWithCoo(x, y, z);

		const int px = x & mask, py = y & mask, pz = z & mask;
		const int ox = x & ~mask, oy = y & ~mask, oz = z & ~mask;

		std::array<std::unique_ptr<MeshInstance>, 8> instances = {};
		for (int i = 0; i < 8; ++i) {
			if (!datas[i]) continue;
			// 与改动前 lod_mesh_cache.cpp:69-73 的位置计算完全一致（注意运算符优先级）
			const glm::vec3 pos = glm::vec3(
				(px + kChildOffset[i].x * size + ox) & ~(size - 1),
				(py + kChildOffset[i].y * size + oy) & ~(size - 1),
				(pz + kChildOffset[i].z * size + oz) & ~(size - 1));
			instances[i] = MakeInstance(*datas[i], pos);
		}

		auto [it, inserted] = datas_.try_emplace(key);
		return it->second.TrySubdivide(px, py, pz, std::move(instances));
	}

	bool LodMeshRegistry::TryCombine(int x, int y, int z, int depth, const core::MeshData* data) {
		const int mask = nova_engine::core::config::OCTREE_SIZE - 1;
		const glm::ivec3 key = GetKeyWithCoo(x, y, z);

		std::unique_ptr<MeshInstance> instance;
		if (data && !data->vertices.empty()) {
			instance = MakeInstance(*data, glm::vec3(x, y, z));
		}

		auto [it, inserted] = datas_.try_emplace(key);
		return it->second.TryCombine(x & mask, y & mask, z & mask, std::move(instance));
	}

	bool LodMeshRegistry::TryDeleteOctree(int x, int y, int z) {
		const glm::ivec3 key = GetKeyWithCoo(x, y, z);
		auto it = datas_.find(key);
		if (it == datas_.end()) return false;
		datas_.erase(key);
		return true;
	}

}