#include "parallel_light_renderer.h"

namespace nova_engine::render::opengl {
	ParallelLightRenderer::ParallelLightRenderer() {
		//创建深度纹理
		glGenTextures(1, &depthMap_);
		glBindTexture(GL_TEXTURE_2D, depthMap_);
		glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT,2048, 2048,0,GL_DEPTH_COMPONENT,GL_FLOAT,NULL);
		//设置纹理参数
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		float borderColor[] = {1.0f,1.0f ,1.0f ,1.0f };
		glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,borderColor);
		//绑定到FBO
		glGenFramebuffers(1, &depthMapFBO_);
		glBindFramebuffer(GL_FRAMEBUFFER,depthMapFBO_);
		glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,depthMap_,0);
		//不渲染颜色
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
		glBindFramebuffer(GL_FRAMEBUFFER,0);

		//用到的渲染程序
		shader_ = ShaderLibrary::GetInstance().GetShader("ShadowShader");
		//存储位置与方向
		camera_ = CameraLibrary::GetInstance().GetCamera("ShadowCamera");

		if (camera_) {
			lightProjection_ = camera_->GetOrthoMatrix(128);
		}
	}

	void ParallelLightRenderer::OnResize(int w, int h) {
		width_ = w, height_ = h;
	}

	void ParallelLightRenderer::Render(const GLMesh& mesh) {
		//渲染
		if (camera_ && shader_) {
			GLint viewport[4];
			glGetIntegerv(GL_VIEWPORT, viewport);
			glViewport(0, 0, 2048, 2048);
			glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO_);

			lightView_ = camera_->GetViewMatrix();
			shader_->Use();
			glm::mat4 lightSpaceMatrix = lightProjection_ * lightView_;
			glm::mat4 modelMatrix = mesh.GetModelMatrix();
			shader_->SetMat4("lightSpaceMatrix", lightSpaceMatrix);
			shader_->SetMat4("model", modelMatrix);
			mesh.Bind(Shadow);
			glDrawElements(GL_TRIANGLES, mesh.GetElementLength(), GL_UNSIGNED_INT, 0);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
		}
	}
}