#pragma once

#include <unordered_map>
#include <string>
#include <memory>
#include <render/opengl/shader.h>

namespace nova_engine::render::opengl {
	class ShaderLibrary {
	public:
		static ShaderLibrary& GetInstance() {
			static ShaderLibrary instance;
			return instance;
		}

		const Shader* GetShader(const std::string& name) const;
		void AddShader(const std::string& name, const std::initializer_list<ShaderFile>& list);

		ShaderLibrary(const ShaderLibrary&) = delete;
		ShaderLibrary& operator=(const ShaderLibrary&) = delete;
	private:
		std::unordered_map<std::string, std::unique_ptr<Shader>> library_;

		ShaderLibrary();
		~ShaderLibrary();
		void Cleanup();
	};
}