#include "gltexture.h"
#include <log/logger.h>

namespace nova_engine::render::opengl {
	GLTexture::GLTexture(const TextureData& data) :
		texture_(0), width_(data.width()), height_(data.height()) {

		glGenTextures(1, &texture_);
		glBindTexture(GL_TEXTURE_2D, texture_);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, data.width(), data.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
		glGenerateMipmap(GL_TEXTURE_2D);

		NE_CORE_INFO("RenderSystem","Bind texture to id:{}", texture_);
	}

	GLTexture::~GLTexture() {
		Cleanup();
	}

	GLTexture::GLTexture(GLTexture&& other) noexcept :
		texture_(other.texture_), width_(other.width_), height_(other.height_)
	{
		other.texture_ = 0;
		other.width_ = -1;
		other.height_ = -1;
	}

	GLTexture& GLTexture::operator=(GLTexture&& other) noexcept {
		if (this != &other)
		{
			Cleanup();
			texture_ = other.texture_;
			width_ = other.width_, height_ = other.height_;
			other.texture_ = 0;
			other.width_ = -1;
			other.height_ = -1;
		}
		return *this;
	}

	void GLTexture::Cleanup() {
		if (texture_ != 0) {
			glDeleteTextures(1, &texture_);
			width_ = -1, height_ = -1;
		}
	}
}