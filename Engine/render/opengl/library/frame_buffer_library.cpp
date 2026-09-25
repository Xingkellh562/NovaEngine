#include "frame_buffer_library.h"
#include <log/logger.h>

namespace nova_engine::render::opengl {
	FrameBufferLibrary::FrameBufferLibrary() {
		AddFrameBuffer("MainFrame");
		FrameBuffer* mainFrame = FindFrameBuffer("MainFrame");
		if (mainFrame) {
			mainFrame->AddTexture("ColorTex", { GL_RGBA,GL_RGBA,GL_UNSIGNED_BYTE,GL_COLOR_ATTACHMENT0 ,0,0 });
			mainFrame->AddTexture("DepthTex", { GL_DEPTH_COMPONENT32F,GL_DEPTH_COMPONENT,GL_FLOAT,GL_DEPTH_ATTACHMENT ,0,0});
		}

		AddFrameBuffer("ShadowFrame");
		FrameBuffer* shadowFrame = FindFrameBuffer("ShadowFrame");
		if (shadowFrame) {
			shadowFrame->AddTexture("ShadowMap", { GL_DEPTH_COMPONENT32F,GL_DEPTH_COMPONENT,GL_FLOAT,GL_DEPTH_ATTACHMENT,2048,2048 });
			auto tex = shadowFrame->FindTexture("ShadowMap");
			if (tex) {
				glBindTexture(GL_TEXTURE_2D, tex);
				float borderColor[] = { 1.0f,1.0f ,1.0f ,1.0f };
				glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
				glBindTexture(GL_TEXTURE_2D, 0);
			}
		}

		AddFrameBuffer("OITFrame");
		FrameBuffer* oitFrame = FindFrameBuffer("OITFrame");
		if (oitFrame) {
			oitFrame->AddTexture("AccumTex", { GL_RGBA16F ,GL_RGBA,GL_FLOAT,GL_COLOR_ATTACHMENT0 ,0,0 });
			oitFrame->AddTexture("RevealTex", { GL_R16F ,GL_RED,GL_FLOAT,GL_COLOR_ATTACHMENT1 ,0,0 });
		}
	}

	FrameBufferLibrary::~FrameBufferLibrary() {
		Cleanup();
	}

	void FrameBufferLibrary::Cleanup() {
		buffers_.clear();
	}

	FrameBuffer* FrameBufferLibrary::FindFrameBuffer(std::string name) const {
		auto it = buffers_.find(name);
		if (it != buffers_.end()) {
			return (it->second.get());
		}
		else {
			NE_CORE_ERROR("RenderSystem", "Failed to find frame buffer with name: {}", name);
		}
		return nullptr;
	}

	void FrameBufferLibrary::AddFrameBuffer(std::string name) {
		buffers_.try_emplace(name,std::make_unique<FrameBuffer>());
		NE_CORE_INFO("RenderSystem", "Add frame buffer with name:{}", name);
	}

	void FrameBufferLibrary::OnResize(int w, int h)
	{
		for (auto& buffer : buffers_) {
			if(buffer.second)
				buffer.second->Resize(w,h);
		}
	}
}