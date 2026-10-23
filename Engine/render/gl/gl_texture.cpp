#include "gl_texture.h"

#include "gl_enums.h"

#include <log/logger.h>

namespace nova_engine::render::gl {

	GLTexture::GLTexture(GLTexture&& o) noexcept
		: texture_(o.texture_), target_(o.target_),
		width_(o.width_), height_(o.height_),
		layerIndex_(std::move(o.layerIndex_)) {
		o.texture_ = 0;
	}
	GLTexture& GLTexture::operator=(GLTexture&& o) noexcept {
		if (this != &o) {
			Destroy();
			texture_ = o.texture_;
			target_ = o.target_;
			width_ = o.width_;
			height_ = o.height_;
			layerIndex_ = std::move(o.layerIndex_);
			o.texture_ = 0;
		}
		return *this;
	}

	namespace {
		// 对应改动前 frame_buffer.cpp:91-99 的纹理参数设置
		void ApplySampler(GLenum target, const core::SamplerState& s) {
			glTexParameteri(target, GL_TEXTURE_MIN_FILTER, ToGLMinFilter(s.minFilter));
			glTexParameteri(target, GL_TEXTURE_MAG_FILTER, ToGLMagFilter(s.magFilter));
			glTexParameteri(target, GL_TEXTURE_WRAP_S, ToGLWrap(s.wrap));
			glTexParameteri(target, GL_TEXTURE_WRAP_T, ToGLWrap(s.wrap));
		}
	} // namespace

	bool GLTexture::Create2D(const core::TextureDesc& desc, const void* pixels) {
		Destroy();

		const GLTexFormat fmt = ToGLTexFormat(desc.format);

		glGenTextures(1, &texture_);
		target_ = GL_TEXTURE_2D;
		glBindTexture(target_, texture_);

		glTexImage2D(target_, 0, fmt.internalFormat,
			static_cast<GLsizei>(desc.width),
			static_cast<GLsizei>(desc.height),
			0, fmt.format, fmt.type, pixels);

		ApplySampler(target_, desc.defaultSampler);

		// 阴影贴图的深度比较模式，对应 frame_buffer_library.cpp:28-31
		if (desc.depthCompareMode) {
			glTexParameterfv(target_, GL_TEXTURE_BORDER_COLOR, desc.borderColor);
			glTexParameteri(target_, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
			glTexParameteri(target_, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
		}

		if (desc.generateMipmaps) glGenerateMipmap(target_);

		glBindTexture(target_, 0);

		width_ = static_cast<int>(desc.width);
		height_ = static_cast<int>(desc.height);
		return texture_ != 0;
	}

	bool GLTexture::CreateArray(const core::TextureArrayDesc& desc,
		std::span<const std::byte* const> layerPixels) {
		Destroy();

		if (desc.tileSize == 0) return false;   // 已由上层校验，这里只是防御

		const GLTexFormat fmt = ToGLTexFormat(desc.format);
		const GLsizei size = static_cast<GLsizei>(desc.tileSize);

		glGenTextures(1, &texture_);
		target_ = GL_TEXTURE_2D_ARRAY;
		glBindTexture(target_, texture_);

		glTexImage3D(target_, 0, fmt.internalFormat, size, size,
			static_cast<GLsizei>(desc.layers), 0,
			fmt.format, fmt.type, nullptr);

		for (std::size_t i = 0; i < layerPixels.size() && i < desc.layers; ++i) {
			if (layerPixels[i] == nullptr) continue;
			glTexSubImage3D(target_, 0, 0, 0, static_cast<GLint>(i),
				size, size, 1, fmt.format, fmt.type, layerPixels[i]);
		}

		ApplySampler(target_, desc.defaultSampler);
		if (desc.generateMipmaps) glGenerateMipmap(target_);
		glBindTexture(target_, 0);

		width_ = height_ = static_cast<int>(size);

		// 层名字 → 索引的表保留在这里也没坏处，但**不要再对外暴露 FindLayerIndex** ——
		// 查询统一走 scene::TextureSet。
		for (const core::TextureArrayLayer& layer : desc.sources) {
			layerIndex_[layer.name] = layer.index;
		}
		return texture_ != 0;
	}

	bool GLTexture::ReallocateStorage(const core::TextureDesc& desc) {
		// 数组纹理的层数在创建时确定，窗口 resize 不改变它。
		// 这里拒绝而不是静默改掉 target_ —— 后者会让纹理对象与 target_ 记录不一致。
		if (texture_ != 0 && target_ != GL_TEXTURE_2D) {
			NE_CORE_ERROR("RenderGL", "ReallocateStorage only supports GL_TEXTURE_2D");
			return false;
		}

		if (texture_ == 0) {
			glGenTextures(1, &texture_);
			if (texture_ == 0) {
				NE_CORE_ERROR("RenderGL", "glGenTextures failed");
				return false;
			}
		}

		target_ = GL_TEXTURE_2D;
		const GLTexFormat fmt = ToGLTexFormat(desc.format);

		glBindTexture(target_, texture_);

		glTexImage2D(target_, 0, static_cast<GLint>(fmt.internalFormat),
			static_cast<GLsizei>(desc.width),
			static_cast<GLsizei>(desc.height),
			0, fmt.format, fmt.type, nullptr);

		// 纹理参数（filter / wrap / compare）属于纹理对象状态，
		// glTexImage2D **不会**重置它们。这里重设一遍只是让本函数自包含，
		// 不依赖「创建时已经设过」这个前提。
		ApplySampler(target_, desc.defaultSampler);

		// 但 COMPARE_MODE 必须双向显式设置：
		// ApplySampler 不碰它，若上一轮是 GL_COMPARE_REF_TO_TEXTURE
		// 而这一轮 desc.depthCompareMode == false，不关掉就会残留。
		if (desc.depthCompareMode) {
			glTexParameterfv(target_, GL_TEXTURE_BORDER_COLOR, desc.borderColor);
			glTexParameteri(target_, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
			glTexParameteri(target_, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
		}
		else {
			glTexParameteri(target_, GL_TEXTURE_COMPARE_MODE, GL_NONE);
		}

		// 尺寸变了，旧 mip 链已失效；需要时重建
		if (desc.generateMipmaps) glGenerateMipmap(target_);

		glBindTexture(target_, 0);

		width_ = static_cast<int>(desc.width);
		height_ = static_cast<int>(desc.height);
		return true;
	}

	int GLTexture::FindLayerIndex(const std::string& name) const {
		const auto it = layerIndex_.find(name);
		return it == layerIndex_.end() ? 0 : it->second;
	}

	void GLTexture::Destroy() {
		if (texture_ != 0) {
			glDeleteTextures(1, &texture_);
			texture_ = 0;
		}
		layerIndex_.clear();
	}

}