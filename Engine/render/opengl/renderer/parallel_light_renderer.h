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

		const GLuint GetFBO() const override {
			return depthMapFBO_;
		}

		const GLuint GetDepthMap() const {
			return depthMap_;
		}

		ParallelLightRenderer();
	private:
		GLuint depthMapFBO_;
		GLuint depthMap_;

		const Shader* shader_;
		const Camera* camera_;

		unsigned int width_, height_ ;

		glm::mat4 lightProjection_;
		glm::mat4 lightView_;
	};

}