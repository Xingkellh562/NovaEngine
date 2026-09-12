#include "semi_transparent_renderer.h"
#include <log/logger.h>

namespace nova_engine::render::opengl {
	SemiTransparentRenderer::SemiTransparentRenderer() {
		glGenTextures(1, &accumTex_);
		glGenTextures(1, &revealTex_);
		glGenFramebuffers(1, &semiTranFbo_);

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
		

		glBindTexture(GL_TEXTURE_2D,accumTex_);
		glTexImage2D(GL_TEXTURE_2D,0, GL_RGBA16F,width_,height_,0,GL_RGBA,GL_FLOAT,nullptr);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glBindTexture(GL_TEXTURE_2D,revealTex_);
		glTexImage2D(GL_TEXTURE_2D,0,GL_R16F,width_,height_,0,GL_RED,GL_FLOAT,nullptr);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glBindFramebuffer(GL_FRAMEBUFFER, semiTranFbo_);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, accumTex_, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,GL_TEXTURE_2D, revealTex_, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D, mainDepthTex_, 0);

		GLenum attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		glDrawBuffers(2, attachments);
		
		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			NE_CORE_DEBUG("RenderSystem", "SemiTran FBO incomplete: {}", status);
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
		glBindFramebuffer(GL_FRAMEBUFFER, mainFbo_);
			
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_TRUE);        // 写深度
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);  // 不写颜色

		glDisable(GL_BLEND);         // 深度占位不需要混合
		glEnable(GL_CULL_FACE);      // 与正式绘制保持一致
		glCullFace(GL_BACK);

		//mvp
		if (camera_)
			view_ = camera_->GetViewMatrix();
		glm::mat4 model = mesh.GetModelMatrix();

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

		glBindFramebuffer(GL_FRAMEBUFFER, semiTranFbo_);

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