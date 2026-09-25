#include "parallel_light_renderer.h"
#include <render/opengl/library/frame_buffer_library.h>

namespace nova_engine::render::opengl {
	ParallelLightRenderer::ParallelLightRenderer() {
		if (opengl::FrameBufferLibrary::GetInstance().FindFrameBuffer("ShadowFrame"))
			opengl::FrameBufferLibrary::GetInstance().FindFrameBuffer("ShadowFrame")->Bind();
		//不渲染颜色
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
		glBindFramebuffer(GL_FRAMEBUFFER,0);

		//用到的渲染程序
		shader_ = ShaderLibrary::GetInstance().GetShader("ShadowShader");
		//存储位置与方向
		camera_ = CameraLibrary::GetInstance().GetCamera("ShadowCamera");

		if (camera_) {
			lightProjection_ = camera_->GetOrthoMatrix(32);
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
			//glCullFace(GL_FRONT);
			//glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO_);

			lightView_ = camera_->GetRelativeViewMatrix();
			shader_->Use();
			glm::mat4 lightSpaceMatrix = lightProjection_ * lightView_;
			glm::mat4 modelMatrix = mesh.GetModelMatrix(camera_->position);
			shader_->SetMat4("lightSpaceMatrix", lightSpaceMatrix);
			shader_->SetMat4("model", modelMatrix);
			mesh.Bind(Shadow);
			
			glDrawElements(GL_TRIANGLES, mesh.GetElementLength(), GL_UNSIGNED_INT, 0);

			//glBindFramebuffer(GL_FRAMEBUFFER, 0);
			//glCullFace(GL_BACK);
			glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
		}
	}
}