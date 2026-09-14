#include "frame_buffer.h"
#include <log/logger.h>

namespace nova_engine::render::opengl {
	FrameBuffer::FrameBuffer(){
		glGenFramebuffers(1,&fbo_);
	}

	FrameBuffer::~FrameBuffer() {
		Cleanup();
	}

	FrameBuffer::FrameBuffer(FrameBuffer&& other) noexcept
		: fbo_(other.fbo_),
		textures_(std::move(other.textures_)),
		texArgs_(std::move(other.texArgs_)),
		colorAttachmentOrder_(std::move(other.colorAttachmentOrder_)) {
		other.fbo_ = 0;
		other.textures_.clear();
		other.texArgs_.clear();
		other.colorAttachmentOrder_.clear();
	}

	FrameBuffer& FrameBuffer::operator=(FrameBuffer&& other) noexcept {
		if (this != &other) {
			Cleanup();

			fbo_ = other.fbo_;
			textures_ = std::move(other.textures_);
			texArgs_ = std::move(other.texArgs_);
			colorAttachmentOrder_ = std::move(other.colorAttachmentOrder_);

			other.fbo_ = 0;
			other.textures_.clear();
			other.texArgs_.clear();
			other.colorAttachmentOrder_.clear();
		}
		return *this;

	}

	void FrameBuffer::AddTexture(const std::string& texName, FrameBufferTexArgs args) {
		bool isColor =
			args.attachment >= GL_COLOR_ATTACHMENT0 &&
			args.attachment <= GL_COLOR_ATTACHMENT0 + 15;

		auto it = textures_.find(texName);

		// 如果已存在，先删除旧纹理，避免泄漏
		if (it != textures_.end()) {
			if (it->second != 0) {
				glDeleteTextures(1, &it->second);
				it->second = 0;
			}

			colorAttachmentOrder_.erase(
				std::remove(colorAttachmentOrder_.begin(),
					colorAttachmentOrder_.end(),
					texName),
				colorAttachmentOrder_.end());
		}

		GLuint tex = 0;
		glGenTextures(1, &tex);

		textures_[texName] = tex;
		texArgs_[texName] = args;

		if (isColor) {
			colorAttachmentOrder_.push_back(texName);
		}
	}

	void FrameBuffer::CreateTexture(const std::string& texName, int width, int height) {
		
		//找到纹理
		auto it = textures_.find(texName);
		auto it2 = texArgs_.find(texName);
		if (it == textures_.end() || it2 == texArgs_.end()) return;

		const auto& args = it2->second;

		int w = width, h = height;
		if (args.width)
			w = args.width;
		if (args.height)
			h = args.height;

		if(!it->second) glGenTextures(1, &textures_[texName]);
		//设置纹理参数
		glBindTexture(GL_TEXTURE_2D,it->second);

		glTexImage2D(GL_TEXTURE_2D, 0, args.internalFormat, w, h, 0, args.format, args.type, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glBindTexture(GL_TEXTURE_2D, 0);

		GLint prevFbo = 0;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);

		glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
		glFramebufferTexture2D(GL_FRAMEBUFFER, args.attachment, GL_TEXTURE_2D, it->second, 0);

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			NE_CORE_ERROR("RenderSystem","Failed to bind frame buffer,name: {}",texName);
		}

		RebuildDrawBuffers();

		glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
	}

	void FrameBuffer::Resize(int width, int height) {
		for (auto& tex : textures_) {
			CreateTexture(tex.first, width, height);
		}
	}

	GLuint FrameBuffer::FindTexture(const std::string& texName) const {
		auto it = textures_.find(texName);
		if (it != textures_.end()) {
			return it->second;
		}
		NE_CORE_ERROR("RenderSystem", "Failed to find frame buffer texture with name: {}", texName);
		return 0;
	}

	void FrameBuffer::Cleanup() {
		if (fbo_) glDeleteFramebuffers(1, &fbo_);
		fbo_ = 0;

		for (auto& tex : textures_) {
			if (tex.second) {
				glDeleteTextures(1, &tex.second);
				tex.second = 0;
			}
				
		}

		textures_.clear();
		texArgs_.clear();
		colorAttachmentOrder_.clear();
	}

	void FrameBuffer::RebuildDrawBuffers() {
		// 必须在 fbo_ 绑定时调用
		std::vector<GLenum> drawBuffers;
		drawBuffers.reserve(colorAttachmentOrder_.size());

		for (const auto& name : colorAttachmentOrder_) {
			auto argIt = texArgs_.find(name);
			auto texIt = textures_.find(name);
			if (argIt == texArgs_.end() || texIt == textures_.end()) continue;
			if (texIt->second == 0) continue;
			drawBuffers.push_back(argIt->second.attachment);
		}

		if (drawBuffers.empty()) {
			// 纯深度/无颜色附件
			GLenum none = GL_NONE;
			glDrawBuffers(1, &none);
		}
		else {
			glDrawBuffers(static_cast<GLsizei>(drawBuffers.size()),
				drawBuffers.data());
		}
	}
}