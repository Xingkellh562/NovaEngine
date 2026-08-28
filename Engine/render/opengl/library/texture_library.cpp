#include "texture_library.h"
#include <render/texture.h>
#include <log/logger.h>

namespace nova_engine::render::opengl {
	TextureLibrary::TextureLibrary() {
		AddTexture("Grass", "assets/grass.png");
	}

	const GLTexture* TextureLibrary::GetTexture(const std::string& name) const {
		auto it = textures_.find(name);
		if (it != textures_.end()) {
			return (it->second.get());
		}
		else {
			NE_CORE_ERROR("RenderSystem", "Failed to find texture with name: {}", name);
		}
		return nullptr;
	}

	void TextureLibrary::AddTexture(const std::string& name, const std::string& path) {
		textures_.try_emplace(name,std::make_unique<GLTexture>(LoadTexture(path.c_str())));
		NE_CORE_INFO("RenderSystem", "Add texture with name:{}", name);
	}
}