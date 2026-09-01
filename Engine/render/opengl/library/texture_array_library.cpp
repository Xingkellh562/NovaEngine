#include "texture_array_library.h"
#include <log/logger.h>

namespace nova_engine::render::opengl {
	TextureArrayLibrary::TextureArrayLibrary() {
		AddTextureArray("BlockTextures", "assets/block_textures");
	}

	const GLTextureArray* TextureArrayLibrary::GetTextureArray(const std::string& name) const {
		auto it = textures_.find(name);
		if (it != textures_.end()) {
			return (it->second.get());
		}
		else {
			NE_CORE_ERROR("RenderSystem", "Failed to find texture array with name: {}", name);
			
		}
		return nullptr;
	}

	void TextureArrayLibrary::AddTextureArray(const std::string& name, const std::string& path) {
		textures_.try_emplace(name, std::make_unique<GLTextureArray>(path.c_str()));
		NE_CORE_INFO("RenderSystem", "Add texture array with name:{}", name);
	}

	void TextureArrayLibrary::Cleanup() {
		NE_CORE_INFO("RenderSystem", "Delete All textures.");
	}

	TextureArrayLibrary::~TextureArrayLibrary() {
		Cleanup();
	}
}