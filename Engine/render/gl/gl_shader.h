#pragma once

#include <render/core/shader_desc.h>
#include <render/core/shader_param.h>

#include <glad/glad.h>

#include <string>
#include <string_view>
#include <unordered_map>

namespace nova_engine::render::gl {

	class GLShader {
	public:
		GLShader() = default;
		~GLShader() { Destroy(); }

		GLShader(const GLShader&) = delete;
		GLShader& operator=(const GLShader&) = delete;
		GLShader(GLShader&& o) noexcept;
		GLShader& operator=(GLShader&& o) noexcept;

		bool Create(const core::ShaderDesc& desc);
		void Destroy();

		void Use() const { glUseProgram(program_); }

		// 查表获取 uniform 位置；未找到返回 -1（调用方静默跳过）
		GLint Location(std::string_view name) const;

		void SetParam(std::string_view name, const core::ShaderParam& value) const;

		GLuint id() const { return program_; }

	private:
		GLuint program_ = 0;
		std::unordered_map<std::string, GLint> locations_;

		void CacheUniformLocations();
		static bool CompileStage(const std::string& path, GLenum type, GLuint& out);
		static std::string LoadSource(const std::string& path);
	};

}