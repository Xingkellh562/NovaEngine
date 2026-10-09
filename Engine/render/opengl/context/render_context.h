#pragma once

#include "static_mesh.h"
#include "lod_mesh_cache.h"
#include <render/opengl/renderer/post_process_renderer.h>
#include <render/opengl/renderer/parallel_light_renderer.h>
#include <render/opengl/renderer/semi_transparent_renderer.h>
#include <render/opengl/renderer/light_renderer.h>

namespace nova_engine::render::opengl::context {
	struct RenderContext {
		StaticMesh chunkMesh = StaticMesh(4);
		LodMeshCache lodMeshCache;

		BlockRenderer blockRenderer;
		PostProcessRenderer postProcessRenderer;
		ParallelLightRenderer parallelLightRenderer;
		SemiTransparentRenderer semiTranRenderer;
		LightRenderer lightRenderer;

		bool useShadow = true,lucencyMesh = true,lodMesh = true;
	};
}