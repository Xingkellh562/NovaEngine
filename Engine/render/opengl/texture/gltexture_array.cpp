#include "gltexture_array.h"
#include "render/texture.h"
#include <filesystem>
#include <vector>
#include <stdexcept>
#include <log/logger.h>

namespace nova_engine::render::opengl {
	GLTextureArray::GLTextureArray(const char* path):textureArray_(0) {
		namespace fs = std::filesystem;

		std::vector<fs::path> filePaths;
		for (const auto& entry : fs::directory_iterator(path)) {
			if (entry.is_regular_file() && entry.path().extension() == ".png") {
				filePaths.push_back(entry.path());
			}
		}
		if (filePaths.empty()) {
			NE_CORE_ERROR("RenderSystem","Create texture array failed,because the directory is empty.");
		}

		TextureData first = LoadTexture(filePaths[0].string().c_str());
		if (!first.data()) {
			NE_CORE_ERROR("RenderSystem", "Create texture array failed.Cannot get the texture data.");
		}

		width_ = first.width();
		height_ = first.height();

		GLenum format = (first.channels() == 4) ? GL_RGBA : ((first.channels() == 3) ? GL_RGB : GL_RED);
		GLenum internalFormat = (first.channels() == 4) ? GL_RGBA8 : ((first.channels() == 3) ? GL_RGB8 : GL_R8);

		glGenTextures(1, &textureArray_);
		glBindTexture(GL_TEXTURE_2D_ARRAY, textureArray_);

		glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, internalFormat,
			first.width(), first.height(), (GLsizei)filePaths.size(),
			0,format,GL_UNSIGNED_BYTE,nullptr);

		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		try
		{
			for (size_t i = 0; i < filePaths.size();i++) {
				TextureData current = (i == 0)? first : LoadTexture(filePaths[i].string().c_str());
				if(!current.data())
					NE_CORE_ERROR("RenderSystem", "Create texture array failed.Cannot get the texture data.");
				if (current.width() != first.width() || current.height() != first.height())
					break;
				if (current.channels() != first.channels())
					break;

				glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0,
					0, 0, i, current.width(), current.height(), 1,
					format, GL_UNSIGNED_BYTE, current.data());

				std::string key = filePaths[i].stem().string();
				indexMap_[key] = (int)i;

				if (i != 0)
					current.Clear();
			}
			first.Clear();
			glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
		}
		
		catch (const std::exception&)
		{
			NE_CORE_ERROR("RenderSystem", "Create texture array failed.");
		}

		
		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	}


	GLTextureArray::~GLTextureArray() {
		Cleanup();
	}

	GLTextureArray::GLTextureArray(GLTextureArray&& other) noexcept :
		textureArray_(other.textureArray_), 
		width_(other.width_),
		height_(other.height_), 
		indexMap_(std::move(other.indexMap_))
	{
		other.textureArray_ = 0;
		other.width_ = -1;
		other.height_ = -1;
		other.indexMap_.clear();
	}

	GLTextureArray& GLTextureArray::operator=(GLTextureArray&& other) noexcept 
		 {
		if (this != &other)
		{
			Cleanup();
			textureArray_ = other.textureArray_;
			width_ = other.width_, height_ = other.height_;
			other.textureArray_ = 0;
			other.width_ = -1;
			other.height_ = -1;
			other.indexMap_.clear();
		}
		return *this;
	}

	void GLTextureArray::Cleanup() {
		if (textureArray_ != 0) {
			glDeleteTextures(1, &textureArray_);
			width_ = -1, height_ = -1;
		}
	}
}

		