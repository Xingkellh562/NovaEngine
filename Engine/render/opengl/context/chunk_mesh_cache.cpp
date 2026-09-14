#include "chunk_mesh_cache.h"

namespace nova_engine::render::opengl::context {
	bool ChunkMeshCache::Exist(int x, int y, int z)  const {
		int fx = x >> 5, fy = y >> 5, fz = z >> 5;

		return indexMap_.find(glm::vec3(fx, fy, fz)) != indexMap_.end();
	}

	void ChunkMeshCache::SubmitMesh(int x,int y,int z,const MeshData& data) {
		int fx = x >> 5, fy = y >> 5, fz = z >> 5;

		glm::vec3 key(fx, fy, fz);

		auto it = indexMap_.find(key);
		size_t idx;
		if (it == indexMap_.end()) {
			// 新区块：分配槽位
			idx = AcquireSlotInternal();
			indexMap_.emplace(key, idx);
		}
		else {
			// 已存在：复用现有槽位
			idx = it->second;
		}
		int mx = x & ~31, my = y & ~31, mz = z & ~31;
		meshs_[idx].position.x = mx;
		meshs_[idx].position.y = my;
		meshs_[idx].position.z = mz;

		meshs_[idx].rotation.x = 0;
		meshs_[idx].rotation.y = 0;
		meshs_[idx].rotation.z = -1;
		//填充数据时自动设置索引数量
		meshs_[idx].GetOrUpdateData(data);
	}

	void ChunkMeshCache::FreeMesh(int x, int y, int z) {
		int fx = x >> 5, fy = y >> 5, fz = z >> 5;
		glm::vec3 key(fx, fy, fz);
		auto it = indexMap_.find(key);
		if (it == indexMap_.end()) {return;} 
		size_t idx = it->second;
		//边界检查(保险起见)
		if (idx >= meshs_.size()) return;
		//失活(把索引数量归零，这样外部渲染器不会渲染它)
		meshs_[idx].Sleep();
		//入池
		pool_.push_back(idx);
		indexMap_.erase(key);
	}

	void ChunkMeshCache::Clear() {
		indexMap_.clear();
		pool_.clear();
		for (size_t i = 0;i < meshs_.size();i++) {
			pool_.push_back(i);
			meshs_[i].Sleep();
		}
	}

	void ChunkMeshCache::ReleasePool() {
		for (size_t i = 0;i < pool_.size();i++) {
			int poolIdx = pool_[i];

		}
	}

	size_t  ChunkMeshCache::AcquireSlotInternal() {
		if (pool_.empty()) {
			meshs_.emplace_back();
			return meshs_.size() - 1;
		}
			
		size_t idx = pool_.back();
		pool_.pop_back();
		meshs_[idx].Sleep();
		return idx;
	}
}