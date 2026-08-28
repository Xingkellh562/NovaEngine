#include "shader_library.h"
#include <log/logger.h>

namespace nova_engine::render::opengl {
	ShaderLibrary::ShaderLibrary() {
		AddShader("BaseShader", { {"shaders/vert.glsl",GL_VERTEX_SHADER},{"shaders/frag.glsl",GL_FRAGMENT_SHADER} });
	}

	const Shader* ShaderLibrary::GetShader(const std::string& name) const {
		auto it = library_.find(name);
		if (it != library_.end()) {
			return (it->second.get());
		}
		else {
			NE_CORE_ERROR("RenderSystem","Failed to find shader with name: {}",name);
		}
		return nullptr;
	}

	void ShaderLibrary::AddShader(const std::string& name,const std::initializer_list<ShaderFile>& list) {
		library_.try_emplace(name, std::make_unique<Shader>(list));
		NE_CORE_INFO("RenderSystem","Add shader with name:{}", name);
	}

	void ShaderLibrary::Cleanup() {
		library_.clear();
	}
}