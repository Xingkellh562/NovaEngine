#include "block_renderer.h"

namespace nova_engine::render::opengl {
	BlockRenderer::BlockRenderer() : IRenderer() {
		shader_ = ShaderLibrary::GetInstance().GetShader("BaseShader");
		camera_ = CameraLibrary::GetInstance().GetCamera("MainCamera");

		const GLTextureArray* textures = TextureArrayLibrary::GetInstance().GetTextureArray("BlockTextures");
		if (textures != nullptr && shader_ != nullptr) {
			shader_->Use();
			GLint textureID = 0;
			glGetIntegeri_v(GL_TEXTURE_BINDING_2D_ARRAY, textureUnit_, &textureID);
			while (textureID == 0 && textureUnit_ < GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS)
			{
				glGetIntegeri_v(GL_TEXTURE_BINDING_2D_ARRAY, ++textureUnit_, &textureID);
			}
			glActiveTexture(textureUnit_);
			glBindTexture(GL_TEXTURE_2D_ARRAY, textures->get());
			shader_->SetTextureUnit("uTextures", textureUnit_);
		}
	}

	void BlockRenderer::Render(const GLMesh& mesh) {
		if (shader_ == nullptr || camera_ == nullptr)
			return;
		view_ = camera_->GetViewMatrix();
		shader_->Use();
		shader_->SetMat4("view", view_);

		mesh.Bind();
		glDrawElements(GL_TRIANGLES, mesh.GetElementLength(), GL_UNSIGNED_INT, 0);
	}

	void BlockRenderer::OnResize(int w, int h) {
		if (camera_ != nullptr)
			projection_ = camera_->GetTransformMatrix(w, h);
		if (shader_ != nullptr)
			shader_->SetMat4("projection", projection_);
	}

	
}