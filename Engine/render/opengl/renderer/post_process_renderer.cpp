#include "post_process_renderer.h"
#include <log/logger.h>

namespace nova_engine::render::opengl {
	PostProcessRenderer::PostProcessRenderer() {
		// 顶点数据（6个顶点，每个4个float：x,y,u,v）
		float vertices[] = {
			-1.0f,  1.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 0.0f,
			 1.0f, -1.0f, 1.0f, 0.0f,
			-1.0f,  1.0f, 0.0f, 1.0f,
			 1.0f, -1.0f, 1.0f, 0.0f,
			 1.0f,  1.0f, 1.0f, 1.0f
		};

		// 生成 VAO 和 VBO
		glGenVertexArrays(1, &quadVao_);
		glGenBuffers(1, &quadVbo_);

		glBindVertexArray(quadVao_);
		glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

		// 位置属性 (location = 0)
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);

		// UV 属性 (location = 1)
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
		glEnableVertexAttribArray(1);

		// 解绑
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		shader_ = ShaderLibrary::GetInstance().GetShader("PostProcessShader");
		mainCamera_ = CameraLibrary::GetInstance().GetCamera("MainCamera");
		shadowCamera_ = CameraLibrary::GetInstance().GetCamera("ShadowCamera");
	}

	PostProcessRenderer::~PostProcessRenderer() {
		glDeleteVertexArrays(1, &quadVao_);
		glDeleteBuffers(1, &quadVbo_);
	}

	void PostProcessRenderer::Render(const GLMesh& mesh) {
		
		if (shader_) {
			
			if (!mainCamera_) return;
			if (!shadowCamera_) return;

			glm::mat4 view = mainCamera_->GetViewMatrix();
			glm::mat4 projection = mainCamera_->GetTransformMatrix(width_, height_);
			glm::mat4 invViewProj = glm::inverse(projection * view);
			
			glm::mat4 lightView = shadowCamera_->GetViewMatrix();
			glm::mat4 lightProjection = shadowCamera_->GetOrthoMatrix(128);
			glm::mat4 lightSpaceMatrix = lightProjection * lightView;

			if (setTex_) {
				shader_->Use();
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, mainColorTex_);
				shader_->SetTextureUnit("colorTex", 1);

				glActiveTexture(GL_TEXTURE2);
				glBindTexture(GL_TEXTURE_2D, mainDepthTex_);
				shader_->SetInt("depthTex", 2);

				glActiveTexture(GL_TEXTURE3);
				glBindTexture(GL_TEXTURE_2D, shadowMap_);
				shader_->SetTextureUnit("shadowMap",3);

				glActiveTexture(GL_TEXTURE4);
				glBindTexture(GL_TEXTURE_2D, accumTex_);
				shader_->SetTextureUnit("accumTex", 4);

				glActiveTexture(GL_TEXTURE5);
				glBindTexture(GL_TEXTURE_2D, revealTex_);
				shader_->SetTextureUnit("revealTex", 5);
			}

			shader_->SetMat4("invViewProj", invViewProj);
			shader_->SetMat4("lightSpaceMatrix", lightSpaceMatrix);
			shader_->SetVec2("screenSize", glm::vec2(width_, height_));

			glBindVertexArray(quadVao_);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			//NE_CORE_DEBUG("RenderSystem", "PostProcessRenderer called");
			glBindVertexArray(0);
		}

	}
}