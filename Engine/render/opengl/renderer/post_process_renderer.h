#pragma once

#include "renderer.h"
#include <render/opengl/library/shader_library.h>
#include <render/opengl/library/camera_library.h>
#include <array>

namespace nova_engine::render::opengl {
	class PostProcessRenderer : public IRenderer {
	public:
		void Render(const GLMesh& mesh) override;

		void OnResize(int w, int h) override {
			width_ = w, height_ = h;
		}

		PostProcessRenderer();
		~PostProcessRenderer();
	private:
		std::array<float, 24> quadVertices_;
		int width_, height_;
		GLuint textureUnit_ = GL_TEXTURE0;
		GLuint quadVao_=0,quadVbo_ = 0;

		const Shader* shader_;

		const Camera* mainCamera_;
		const Camera* shadowCamera_;


	};

}