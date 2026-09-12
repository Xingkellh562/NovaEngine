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

		const GLuint GetFBO() const override {
			return 0;
		}

		void SetColorTex(GLuint colorTex,GLuint depthTex,GLuint shadowMap) {
			mainColorTex_ = colorTex;
			mainDepthTex_ = depthTex;
			shadowMap_ = shadowMap;
			setTex_ = true;
		}
		void SetOitTex(GLuint accumTex, GLuint revealTex) {
			accumTex_ = accumTex;
			revealTex_ = revealTex;
			setTex_ = true;
		}

		PostProcessRenderer();
		~PostProcessRenderer();
	private:
		std::array<float, 24> quadVertices_;
		int width_, height_;
		GLuint textureUnit_ = GL_TEXTURE0;
		GLuint quadVao_=0,quadVbo_ = 0;

		GLuint mainColorTex_ = 0,mainDepthTex_  = 0,shadowMap_ = 0;
		GLuint accumTex_ = 0, revealTex_ = 0;
		bool setTex_ = false;

		const Shader* shader_;

		const Camera* mainCamera_;
		const Camera* shadowCamera_;


	};

}