#pragma once

#include "renderer.h"
#include <render/opengl/library/shader_library.h>
#include <render/opengl/library/camera_library.h>
#include <glm/mat4x4.hpp>

namespace nova_engine::render::opengl {
	class ParallelLightRenderer : public IRenderer {
	public:
		void Render(const GLMesh& mesh) override;

		void OnResize(int w, int h) override;

		ParallelLightRenderer();
	private:
		const Shader* shader_;
		const Camera* camera_;

		unsigned int width_, height_ ;

		glm::mat4 lightProjection_;
		glm::mat4 lightView_;
	};

}