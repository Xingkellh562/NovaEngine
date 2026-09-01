#pragma once

#include <memory>
#include <unordered_map>
#include <string>
#include <render/opengl/texture/gltexture.h>


namespace nova_engine::render::opengl {
	class TextureLibrary {
	public:
		static TextureLibrary& GetInstance() {
			static TextureLibrary instance;
			return instance;
		}

		const GLTexture* GetTexture(const std::string& name) const;

		void AddTexture(const std::string& name, const std::string& path);

		TextureLibrary(const TextureLibrary&) = delete;
		TextureLibrary& operator=(const TextureLibrary&) = delete;

	private:
		std::unordered_map<std::string, std::unique_ptr<GLTexture>> textures_;

		TextureLibrary();
		~TextureLibrary() = default;
	};

}