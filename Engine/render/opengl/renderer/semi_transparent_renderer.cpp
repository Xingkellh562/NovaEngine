#include "semi_transparent_renderer.h"
#include <log/logger.h>
#include <render/opengl/library/frame_buffer_library.h>

namespace nova_engine::render::opengl {
	SemiTransparentRenderer::SemiTransparentRenderer() {
		depthShader_ = ShaderLibrary::GetInstance().GetShader("DepthPrepassShader");
		accumShader_ = ShaderLibrary::GetInstance().GetShader("AccumPassShader");
		camera_ = CameraLibrary::GetInstance().GetCamera("MainCamera");

		const GLTextureArray* textures = TextureArrayLibrary::GetInstance().GetTextureArray("BlockTextures");
		if (textures != nullptr && depthShader_ != nullptr && accumShader_ != nullptr) {
			depthShader_->Use();
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D_ARRAY, textures->get());
			depthShader_->SetTextureUnit("uTextureArray", 0);
			accumShader_->SetTextureUnit("uTextureArray", 0);
		}
	}


	void SemiTransparentRenderer::CreateSemiTranFbo() {
		if (opengl::FrameBufferLibrary::GetInstance().FindFrameBuffer("OITFrame"))
			opengl::FrameBufferLibrary::GetInstance().FindFrameBuffer("OITFrame")->Bind();
		auto mainFbo = FrameBufferLibrary::GetInstance().FindFrameBuffer("MainFrame");
		if (mainFbo) {
			GLuint depthTex = mainFbo->FindTexture("DepthTex");
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex, 0);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void SemiTransparentRenderer::Render(const GLMesh& mesh) {
		if (needResizeFBO_) {
			CreateSemiTranFbo();
			if (camera_) {
				projection_ = camera_->GetTransformMatrix(width_, height_);
				
			}

		    needResizeFBO_ = false;
		}

		//直接渲染
		//glBindFramebuffer(GL_FRAMEBUFFER, mainFbo_);
		if (opengl::FrameBufferLibrary::GetInstance().FindFrameBuffer("MainFrame"))
			opengl::FrameBufferLibrary::GetInstance().FindFrameBuffer("MainFrame")->Bind();

		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_TRUE);        // 写深度
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);  // 不写颜色

		glDisable(GL_BLEND);         // 深度占位不需要混合
		glEnable(GL_CULL_FACE);      // 与正式绘制保持一致
		glCullFace(GL_BACK);

		//mvp
		glm::mat4 model;
		if (camera_) {
			view_ = camera_->GetRelativeViewMatrix();
			model = mesh.GetModelMatrix(camera_->position);
		}
		else {
			model = mesh.GetModelMatrix();
		}
			

		if (depthShader_) {
			depthShader_->Use();

			depthShader_->SetMat4("uProj", projection_);
			depthShader_->SetMat4("uView", view_);
			depthShader_->SetMat4("uModel", model);

			mesh.Bind(Main);
			glDrawElements(GL_TRIANGLES, mesh.GetElementLength(), GL_UNSIGNED_INT, 0);
		}

		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDepthMask(GL_FALSE);

		glBlendFunci(0, GL_ONE, GL_ONE);              // accum: additive
		glBlendFunci(1, GL_ZERO, GL_ONE_MINUS_SRC_COLOR); // revealage: multiplicative

		//glBindFramebuffer(GL_FRAMEBUFFER, semiTranFbo_);
		if (opengl::FrameBufferLibrary::GetInstance().FindFrameBuffer("OITFrame"))
			opengl::FrameBufferLibrary::GetInstance().FindFrameBuffer("OITFrame")->Bind();

		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_FALSE);       // ★ 不写深度
		glEnable(GL_BLEND);
		
		glBlendFunc(GL_ONE, GL_ONE); // ★ 加法混合：颜色+权重都累加

		// Revealage 需要单独的乘性混合（双源混合或分 MRT 索引控制）
		glBlendFunci(0, GL_ONE, GL_ONE);              // accum: additive
		glBlendFunci(1, GL_ZERO, GL_ONE_MINUS_SRC_COLOR); // revealage: multiplicative

		if (accumShader_) {
			accumShader_->Use();

			accumShader_->SetMat4("uProj", projection_);
			accumShader_->SetMat4("uView", view_);
			accumShader_->SetMat4("uModel", model);

			mesh.Bind(Main);
			glDrawElements(GL_TRIANGLES, mesh.GetElementLength(), GL_UNSIGNED_INT, 0);
		}
		
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
		glEnable(GL_CULL_FACE);
	}
}