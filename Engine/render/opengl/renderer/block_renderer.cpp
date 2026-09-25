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
	}

	void BlockRenderer::CreateMainFBO(int width, int height) const {
	}

	void BlockRenderer::Render(const GLMesh& mesh){
		if (needResizeFBO_) {
			glViewport(0,0,width_, height_);
			//CreateMainFBO(width_ , height_);
			needResizeFBO_ = false;
		}

		if (shader_ == nullptr || camera_ == nullptr)
			return;

		view_ = camera_->GetRelativeViewMatrix();
		shader_->Use();
		shader_->SetMat4("view", view_);
		shader_->SetMat4("model", mesh.GetModelMatrix(camera_->position));

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