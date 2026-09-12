#include "shader_library.h"
#include <log/logger.h>

namespace nova_engine::render::opengl {
	ShaderLibrary::ShaderLibrary() {
		AddShader("BaseShader", { 
			{"shaders/vert.glsl",GL_VERTEX_SHADER},
			{"shaders/frag.glsl",GL_FRAGMENT_SHADER} });

		AddShader("ShadowShader",{ 
			{"shaders/para_vert.glsl",GL_VERTEX_SHADER},
			{"shaders/para_frag.glsl",GL_FRAGMENT_SHADER} });

		AddShader("PostProcessShader", { 
			{"shaders/post_process_vert.glsl",GL_VERTEX_SHADER},
			{"shaders/post_process_frag.glsl",GL_FRAGMENT_SHADER} });

		AddShader("DepthPrepassShader",{ 
			{"shaders/depth_prepass_vert.glsl", GL_VERTEX_SHADER}, 
			{"shaders/depth_prepass_frag.glsl",GL_FRAGMENT_SHADER}});

		AddShader("AccumPassShader", {
			{"shaders/accum_pass_vert.glsl", GL_VERTEX_SHADER},
			{"shaders/accum_pass_frag.glsl",GL_FRAGMENT_SHADER}});
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
		NE_CORE_INFO("RenderSystem", "Delete All shaders.");
	}

	ShaderLibrary::~ShaderLibrary() {
		Cleanup();
	}
}