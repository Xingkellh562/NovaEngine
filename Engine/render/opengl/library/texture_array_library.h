#pragma once

#include <memory>
#include <unordered_map>
#include <string>
#include <render/opengl/texture/gltexture_array.h>


namespace nova_engine::render::opengl {
	class TextureArrayLibrary {
	public:
		static TextureArrayLibrary& GetInstance() {
			static TextureArrayLibrary instance;
			return instance;
		}

		const GLTextureArray* GetTextureArray(const std::string& name) const;

		void AddTextureArray(const std::string& name, const std::string& path);

		TextureArrayLibrary(const TextureArrayLibrary&) = delete;
		TextureArrayLibrary& operator=(const TextureArrayLibrary&) = delete;

	private:
		std::unordered_map<std::string, std::unique_ptr<GLTextureArray>> textures_;

		void Cleanup();

		TextureArrayLibrary();
		~TextureArrayLibrary();
	};

}