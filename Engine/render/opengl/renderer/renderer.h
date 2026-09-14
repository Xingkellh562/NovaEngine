#pragma once

#include <render/opengl/glmesh.h>
#include <render/opengl/context/glcontext.h>

namespace nova_engine::render::opengl {
	class IRenderer {
	public:
		virtual ~IRenderer() = default;
		virtual void Render(const GLMesh& mesh) = 0;
		virtual void OnResize(int w, int h) = 0;

		IRenderer(const IRenderer&) = delete;
		IRenderer& operator=(const IRenderer&) = delete;
	protected:
		IRenderer() = default;
	};
}