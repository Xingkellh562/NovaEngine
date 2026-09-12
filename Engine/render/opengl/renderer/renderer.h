#pragma once

#include <render/opengl/glmesh.h>

namespace nova_engine::render::opengl {
	class IRenderer {
	public:
		virtual ~IRenderer() = default;
		virtual void Render(const GLMesh& mesh) = 0;
		virtual void OnResize(int w, int h) = 0;
		virtual const GLuint GetFBO() const = 0;

		IRenderer(const IRenderer&) = delete;
		IRenderer& operator=(const IRenderer&) = delete;
	protected:
		IRenderer() = default;
	};
}