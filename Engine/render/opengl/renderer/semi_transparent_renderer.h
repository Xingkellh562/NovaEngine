#pragma once

#include "renderer.h"
#include <render/opengl/library/shader_library.h>
#include <render/opengl/library/camera_library.h>
#include <render/opengl/library/texture_array_library.h>
#include <array>
#include <log/logger.h>

namespace nova_engine::render::opengl {
	class SemiTransparentRenderer : public IRenderer {
	public:
		SemiTransparentRenderer();

		void Render(const GLMesh& mesh) override;

		void OnResize(int w, int h) override {
			width_ = w, height_ = h;
			needResizeFBO_ = true;
		}
	private:
		const Shader* depthShader_,*accumShader_;
		const Camera* camera_;

		unsigned int width_ = 800, height_ = 600;
		bool needResizeFBO_ = false;

		void CreateSemiTranFbo();

		glm::mat4 projection_{};
		glm::mat4 view_{};
	};


}