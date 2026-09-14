#pragma once

#include <glad/glad.h>
#include <unordered_map>
#include <string>
#include <vector>

namespace nova_engine::render::opengl {
	struct FrameBufferTexArgs
	{
		GLenum internalFormat;
		GLenum format;
		GLenum type;
		GLenum attachment;
		int width = 0, height = 0;
	};
	class FrameBuffer {
	public:
		FrameBuffer();
		~FrameBuffer();

		//禁拷贝
		FrameBuffer(const FrameBuffer&) = delete;
		FrameBuffer& operator=(const FrameBuffer&) = delete;

		FrameBuffer(FrameBuffer&&) noexcept;
		FrameBuffer& operator=(FrameBuffer&&) noexcept;

		void AddTexture(const std::string&, FrameBufferTexArgs);
		void CreateTexture(const std::string&,int width,int height);
		GLuint FindTexture(const std::string&) const;

		void Resize(int width, int height);

		GLuint GetFbo() const {return fbo_;}
		void Bind() const {glBindFramebuffer(GL_FRAMEBUFFER,fbo_);}


	private:
		GLuint fbo_;
		std::unordered_map<std::string, GLuint> textures_;
		std::unordered_map<std::string, FrameBufferTexArgs> texArgs_;

		std::vector<std::string> colorAttachmentOrder_;

		void Cleanup();
		void RebuildDrawBuffers();
	};
}