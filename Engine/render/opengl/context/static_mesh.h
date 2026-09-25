#pragma once

#include "chunk_mesh_cache.h"
#include <vector>
#include <memory>

enum MeshLayer
{
	Default = 0,
	Lucency = 1,
	Liquid = 2,
	Lod = 3
};

enum LodDepth {
	Lod0 = 0,
	Lod1 = 1,
	Lod2 = 2,
	Lod3 = 3,
	Lod4 = 4,
	Lod5 = 5,
	Lod6 = 6,
};

namespace nova_engine::render::opengl::context {
	class StaticMesh {
	public:
		StaticMesh(int layerSize = 4) {
			for (size_t i = 0; i < layerSize; i++)
				caches_.push_back(std::make_unique<ChunkMeshCache>());
		};
		~StaticMesh() = default;

		StaticMesh(const StaticMesh&) = delete;
		StaticMesh& operator=(const StaticMesh&) = delete;

		ChunkMeshCache* GetLayer(size_t layer) {
			if (layer >= caches_.size()) return nullptr;
			return caches_[layer].get();
		}
	private:
		std::vector<std::unique_ptr<ChunkMeshCache>> caches_;
	};

}