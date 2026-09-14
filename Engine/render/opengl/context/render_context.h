#pragma once

#include "static_mesh.h"
#include "glcontext.h"
#include <render/opengl/renderer/post_process_renderer.h>
#include <render/opengl/renderer/parallel_light_renderer.h>
#include <render/opengl/renderer/semi_transparent_renderer.h>

namespace nova_engine::render::opengl::context {
	struct RenderContext {
		StaticMesh chunkMesh;
		BlockRenderer blockRenderer;
		PostProcessRenderer postProcessRenderer;
		ParallelLightRenderer parallelLightRenderer;
		SemiTransparentRenderer semiTranRenderer;
		GLContext glContext;

		bool useShadow = true,lucencyMesh = true;
	};
}