#pragma once

#include <glad/glad.h>

#include <unordered_map>
#include <string>

namespace nova_engine::render::opengl {
	class GLTextureArray {
	public:
		GLTextureArray(const char* path);
		~GLTextureArray();

		//禁用拷贝，允许移动
		GLTextureArray(const GLTextureArray&) = delete;
		GLTextureArray& operator=(const GLTextureArray&) = delete;
		GLTextureArray(GLTextureArray&&) noexcept;
		GLTextureArray& operator=(GLTextureArray&&) noexcept;

		GLuint get() const { return textureArray_; }
		int FindIndex(const std::string& name) const{
			auto it = indexMap_.find(name);
			if (it != indexMap_.end())
				return it->second;
			else
				return 0;
		}
		int width() const { return width_; }
		int height() const { return height_; }
	private:
		GLuint textureArray_;
		int width_, height_;
		std::unordered_map<std::string, int> indexMap_;
		void Cleanup();
	};
}