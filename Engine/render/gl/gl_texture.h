#pragma once

#include <render/core/texture_desc.h>

#include <glad/glad.h>

#include <string>
#include <unordered_map>

namespace nova_engine::render::gl {

	class GLTexture {
	public:
		GLTexture() = default;
		~GLTexture() { Destroy(); }

		GLTexture(const GLTexture&) = delete;
		GLTexture& operator=(const GLTexture&) = delete;
		GLTexture(GLTexture&& o) noexcept;
		GLTexture& operator=(GLTexture&& o) noexcept;

		bool Create2D(const core::TextureDesc& desc, const void* pixels);
		bool CreateArray(const core::TextureArrayDesc& desc,
			const std::vector<const void*>& layerPixels);

		bool ReallocateStorage(const core::TextureDesc& desc);

		void Destroy();

		GLuint  id() const { return texture_; }
		GLenum  target() const { return target_; }
		bool    valid() const { return texture_ != 0; }
		int     FindLayerIndex(const std::string& name) const;

	private:
		GLuint texture_ = 0;
		GLenum target_ = GL_TEXTURE_2D;
		int    width_ = 0, height_ = 0;
		std::unordered_map<std::string, int> layerIndex_;
	};

}