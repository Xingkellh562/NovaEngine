#include "block_renderer.h"
#include <log/logger.h>

namespace nova_engine::render::opengl {
	BlockRenderer::BlockRenderer() : IRenderer() {
		shader_ = ShaderLibrary::GetInstance().GetShader("BaseShader");
		camera_ = CameraLibrary::GetInstance().GetCamera("MainCamera");

		const GLTextureArray* textures = TextureArrayLibrary::GetInstance().GetTextureArray("BlockTextures");
		if (textures != nullptr && shader_ != nullptr) {
			shader_->Use();
			/*GLint textureID = 0;
			glGetIntegeri_v(GL_TEXTURE_BINDING_2D_ARRAY, textureUnit_, &textureID);
			while (textureID == 0 && textureUnit_ < GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS)
			{
				glGetIntegeri_v(GL_TEXTURE_BINDING_2D_ARRAY, ++textureUnit_, &textureID);
			}*/
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D_ARRAY, textures->get());
			shader_->SetTextureUnit("uTextures", 0);
		}

		glGenTextures(1, &mainColorTex_);
		glGenTextures(1, &mainDepthTex_);
		glGenFramebuffers(1, &mainFBO_);
	}

	void BlockRenderer::CreateMainFBO(int width, int height) const {

		// 颜色纹理
		glBindTexture(GL_TEXTURE_2D, mainColorTex_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		// 深度纹理
		glBindTexture(GL_TEXTURE_2D, mainDepthTex_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		// 创建 FBO
		glBindFramebuffer(GL_FRAMEBUFFER, mainFBO_);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mainColorTex_, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, mainDepthTex_, 0);

		// 检查完整性
		//GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		//if (status != GL_FRAMEBUFFER_COMPLETE) {
		//	// 输出具体错误
		//	NE_CORE_ERROR("RenderSystem", "FBO incomplete:{},{} ", std::hex ,status);
		//}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void BlockRenderer::Render(const GLMesh& mesh){
		if (needResizeFBO_) {
			glViewport(0,0,width_, height_);
			CreateMainFBO(width_ , height_);
			needResizeFBO_ = false;
		}

		if (shader_ == nullptr || camera_ == nullptr)
			return;

		view_ = camera_->GetViewMatrix();
		shader_->Use();
		shader_->SetMat4("view", view_);
		shader_->SetMat4("model", mesh.GetModelMatrix());

		projection_ = camera_->GetTransformMatrix(width_, height_);
		shader_->SetMat4("projection", projection_);
		mesh.Bind(Main);
		glDrawElements(GL_TRIANGLES, mesh.GetElementLength(), GL_UNSIGNED_INT, 0);
		//glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void BlockRenderer::OnResize(int w, int h) {
		width_ = w, height_ = h;
		needResizeFBO_ = true;
	}

	
}