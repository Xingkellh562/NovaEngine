#pragma once

#include "chunk_mesh_cache.h"

namespace nova_engine::render::opengl::context {
	struct RenderContext {
		ChunkMeshCache chunkMeshCache;
		BlockRenderer blockRenderer;
	};
}