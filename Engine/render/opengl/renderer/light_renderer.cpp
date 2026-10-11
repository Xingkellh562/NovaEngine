#include "light_renderer.h"

namespace nova_engine::render::opengl {
	void LightRenderer::Render(const GLMesh& mesh) {
		if (shader_ == nullptr || camera_ == nullptr)
			return;

		glm::mat4 view = camera_->GetRelativeViewMatrix();
		glm::mat4 model = mesh.GetModelMatrix(camera_->position);
		glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));

		shader_->Use();
		
		shader_->SetMat4("view", view);
		shader_->SetMat4("model", model);
		shader_->SetMat4("projection", projection_);
		shader_->SetMat3("normalMatrix", normalMatrix);
		shader_->SetVec3("lightDir", glm::normalize(glm::vec3(0.3, 0.8, 0.5)));
		shader_->SetTextureUnit("uTextureArray", 0);

		mesh.Bind(Light);
		glDrawElements(GL_TRIANGLES, mesh.GetElementLength(), GL_UNSIGNED_INT, 0);
	}

	void LightRenderer::OnResize(int w, int h){
		width_ = w, height_ = h;
		if(camera_)
			projection_ = camera_->GetTransformMatrix(width_, height_);
	}

	LightRenderer::LightRenderer() {
		shader_ = ShaderLibrary::GetInstance().GetShader("LightShader");
		camera_ = CameraLibrary::GetInstance().GetCamera("MainCamera");

		width_ = 800, height_ = 600;
	}

}