#pragma once

#include <render/opengl/renderer/block_renderer.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>


namespace nova_engine::render::opengl::context {
	//缓存并维护所有 GLMesh对象
	class ChunkMeshCache {
	public:
		ChunkMeshCache() = default;
		~ChunkMeshCache() = default;

		ChunkMeshCache(const ChunkMeshCache&) = delete;
		ChunkMeshCache& operator=(const ChunkMeshCache&) = delete;
		//通过坐标哈希只读查找mesh是否存在，用于一些外部逻辑
		bool Exist(int x, int y, int z) const;
		//只读查找Mesh对象
		const GLMesh* FindMesh(int x, int y, int z) const;
		//失活对应mesh，索引移入池，如果不存在则不做任何操作
		void FreeMesh(int x, int y, int z);
		//清空所有数据
		void Clear();
		//直接提交顶点数据，内部自己判断该更新还是添加
		void SubmitMesh(int x, int y, int z,const MeshData& data);

		void ReleasePool();
		//获取Mesh数组用于渲染遍历
		const std::vector<GLMesh>& GetAllMesh() const {return meshs_;}

		inline int PoolLength() const { return pool_.size(); }
		inline int MeshCount() const { return meshs_.size(); }
	private:
		std::unordered_map<glm::vec3, size_t> indexMap_;
		std::vector<GLMesh> meshs_;
		std::vector<size_t> pool_;

		//获取索引
		size_t AcquireSlotInternal();
	};
}