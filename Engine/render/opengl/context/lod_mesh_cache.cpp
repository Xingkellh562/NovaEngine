#include "lod_mesh_cache.h"
#include <iostream>

namespace nova_engine::render::opengl::context {
	GLMesh* LodMeshCache::Find(int x, int y, int z, int* depth) const {
		const int mask = core::config::OCTREE_SIZE - 1;
		glm::ivec3 key = GetKeyWithCoo(x, y, z);
		auto it = datas_.find(key);
		if (it != datas_.end()) {
			return it->second.Find(x & mask, y & mask, z & mask, depth);
		}
		else {
			if (depth) *depth = 0;
			return nullptr;
		}

	}

	bool LodMeshCache::TryEmplace(int x, int y, int z, const MeshData& data) {
		glm::ivec3 key = GetKeyWithCoo(x, y, z);
		auto it = datas_.find(key);
		if (it != datas_.end()) {
			const int mask = core::config::OCTREE_SIZE - 1;
			if (data.vertices.size() > 0) {
				auto newPtr = std::make_unique<GLMesh>();
				if (newPtr) {
					newPtr->GetOrUpdateData(data);
					return it->second.TryEmplace(x & mask, y & mask, z & mask, std::move(newPtr));
				}
			}
			else {
				return it->second.TryEmplace(x & mask, y & mask, z & mask, nullptr);
			}
			
			return false;
		}
		else return false;
	}

	bool LodMeshCache::TrySubdivide(
		int x, int y, int z, int depth,
		std::array<MeshData*, 8> datas)
	{
		static const std::array<glm::ivec3, 8> NeiOffCoord = { {
			{0,0,0},{1,0,0},{0,1,0},{1,1,0},
			{0,0,1},{1,0,1},{0,1,1},{1,1,1}
		} };

		int size = core::config::OCTREE_SIZE >> depth; // 子节点大小
		const int mask = core::config::OCTREE_SIZE - 1;

		glm::ivec3 key = GetKeyWithCoo(x, y, z);

		// 父节点在八叉树内的局部原点
		int px = x & mask;
		int py = y & mask;
		int pz = z & mask;

		int ox = x & ~mask;
		int oy = y & ~mask;
		int oz = z & ~mask;

		std::array<std::unique_ptr<GLMesh>, 8> meshs = {};
		for (int i = 0; i < 8; ++i) {
			if (datas[i]) {
				meshs[i] = std::make_unique<GLMesh>();
				meshs[i]->GetOrUpdateData(*datas[i]);

				meshs[i]->position.x = px + NeiOffCoord[i].x * size + ox & ~(size - 1);
				meshs[i]->position.y = py + NeiOffCoord[i].y * size + oy & ~(size - 1);
				meshs[i]->position.z = pz + NeiOffCoord[i].z * size + oz & ~(size - 1);

				meshs[i]->rotation = { 0,0,-1 };
			}
			else {
				meshs[i] = nullptr;
			}
		}

		auto [it, insert] = datas_.try_emplace(key);
		return it->second.TrySubdivide(px, py, pz, std::move(meshs));
	}

	bool LodMeshCache::TryCombine(
		int x, int y, int z, int depth, const MeshData* data)
	{
		const int mask = core::config::OCTREE_SIZE - 1;

		glm::ivec3 key = GetKeyWithCoo(x, y, z);

		std::unique_ptr<GLMesh> mesh = nullptr;
		if (data) {
			mesh = std::make_unique<GLMesh>();
			mesh->GetOrUpdateData(*data);

			mesh->position.x = x;
			mesh->position.y = y;
			mesh->position.z = z;

			mesh->rotation = { 0,0,-1 };
		}
		
		auto [it, insert] = datas_.try_emplace(key);
		return it->second.TryCombine(x & mask, y & mask, z & mask, std::move(mesh));
	}

	bool LodMeshCache::TryDeleteOctree(int x, int y, int z) {
		glm::ivec3 key = GetKeyWithCoo(x, y, z);

		auto it = datas_.find(key);
		if (it == datas_.end()) {
			datas_.erase(key);
			return true;
		}
		return false;
	}

}