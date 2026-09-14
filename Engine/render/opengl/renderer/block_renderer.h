#pragma once

#include "renderer.h"
#include <render/opengl/library/shader_library.h>
#include <render/opengl/library/camera_library.h>
#include <render/opengl/library/texture_array_library.h>
#include <glm/mat4x4.hpp>

namespace nova_engine::render::opengl {
	class BlockRenderer : public IRenderer {
	public:
		void Render(const GLMesh& mesh) override;

		void OnResize(int w, int h) override;

		BlockRenderer();
	private:
		const Shader* shader_;
		const Camera* camera_;

		int textureUnit_ = GL_TEXTURE0;
		glm::mat4 projection_{};
		glm::mat4 view_{};

		bool needResizeFBO_ = false;
		int width_ = 800, height_ = 600;

		void CreateMainFBO(int width, int height)const;
	};
}