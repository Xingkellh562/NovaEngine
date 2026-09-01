#pragma once

#include <glad/glad.h>
#include "render/texture.h"

namespace nova_engine::render::opengl {
	class GLTexture {
	public:
		GLTexture(const TextureData& data);
		~GLTexture();

		//禁用拷贝，允许移动
		GLTexture(const GLTexture&) = delete;
		GLTexture& operator=(const GLTexture&) = delete;
		GLTexture(GLTexture&&) noexcept;
		GLTexture& operator=(GLTexture&&) noexcept;

		GLuint get() const { return texture_; }
		int width() const { return width_; }
		int height() const { return height_; }
	private:
		GLuint texture_;
		int width_, height_;
		void Cleanup();
	};
}