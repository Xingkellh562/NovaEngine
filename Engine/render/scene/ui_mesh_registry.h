#pragma once

#include <render/core/mesh_data.h>

#include <unordered_map>
#include <vector>

namespace nova_engine::render::scene {
	//缓存所有的UI四面体网格
	//将所有ui四边形存储到一个MeshData上，渲染时可以一次DrawCall批量提交
	class UIMeshRegistry {
	public:
		UIMeshRegistry(device::IRenderDevice& device) : device_(device) {
			data_.vertices = {385,298,415,298,415,302,385,302,
							  398,285,402,285,402,315,398,315 };
			data_.indices = { 0,1,2,0,2,3,4,5,6,4,6,7 };
			data_.uvws = {0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,1 };
			data_.colors = {
				0.8f,0.8f,0.8f,1.0f,
				0.8f,0.8f,0.8f,1.0f,
				0.8f,0.8f,0.8f,1.0f,
				0.8f,0.8f,0.8f,1.0f,
				0.8f,0.8f,0.8f,1.0f,
				0.8f,0.8f,0.8f,1.0f,
				0.8f,0.8f,0.8f,1.0f,
				0.8f,0.8f,0.8f,1.0f,
			};

			mesh_.owner = &device;
			mesh_.mesh = device_.CreateMesh(data_);
			mesh_.rotation = glm::vec3(0, 0, -1);
		}
		~UIMeshRegistry() = default;

		void CreateOrUpdateQuad(size_t id) {}
		void ReleaseQuad(size_t id) {}
		void Clear() {}

		const scene::MeshInstance& GetMeshData() const{
			if (mesh_.mesh.Valid()) {
				device_.UpdateMesh(mesh_.mesh, data_);
			}
			return mesh_;
		}
	private:
		device::IRenderDevice& device_;
		//维护四边形对象ID->index索引的映射
		std::unordered_map<size_t, size_t> map_;
		//回收释放的索引，避免数据数组中产生空洞
		std::vector<size_t> idxPool_;

		core::MeshData data_;
		scene::MeshInstance mesh_;
	};
}