#include "gl_shader.h"

#include "gl_enums.h"

#include <log/logger.h>

#include <glm/gtc/type_ptr.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace nova_engine::render::gl {

	GLShader::GLShader(GLShader&& o) noexcept
		: program_(o.program_), locations_(std::move(o.locations_)) {
		o.program_ = 0;
		o.locations_.clear();
	}
	GLShader& GLShader::operator=(GLShader&& o) noexcept {
		if (this != &o) {
			Destroy();
			program_ = o.program_;
			locations_ = std::move(o.locations_);
			o.program_ = 0;
			o.locations_.clear();
		}
		return *this;
	}

	bool GLShader::CompileStage(const std::string& path, GLenum type, GLuint& out) {
		const std::string src = LoadSource(path);
		if (src.empty()) {
			NE_CORE_ERROR("RenderGL", "Shader source not found: {}", path);
			return false;
		}

		const GLuint shader = glCreateShader(type);
		const char* raw = src.c_str();
		glShaderSource(shader, 1, &raw, nullptr);
		glCompileShader(shader);

		GLint ok = GL_FALSE;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
		if (!ok) {
			char log[1024] = {};
			glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
			NE_CORE_ERROR("RenderGL", "Compile failed: {} ({})", path, log);
			glDeleteShader(shader);
			return false;
		}
		out = shader;
		return true;
	}

	std::string GLShader::LoadSource(const std::string& path) {
		// 与改动前 shader.cpp:168-190 的 LoadShaderSrc 等价
		std::ifstream file(path);
		if (!file.is_open()) return {};
		std::stringstream buf;
		buf << file.rdbuf();
		return buf.str();
	}

	bool GLShader::Create(const core::ShaderDesc& desc) {
		std::vector<GLuint> stages;
		for (const core::ShaderStageDesc& s : desc.stages) {
			GLuint id = 0;
			if (!CompileStage(s.path, ToGLShaderStage(s.stage), id)) {
				for (GLuint s2 : stages) glDeleteShader(s2);
				return false;
			}
			stages.push_back(id);
		}

		program_ = glCreateProgram();
		for (GLuint s : stages) glAttachShader(program_, s);
		glLinkProgram(program_);

		GLint ok = GL_FALSE;
		glGetProgramiv(program_, GL_LINK_STATUS, &ok);
		if (!ok) {
			char log[1024] = {};
			glGetProgramInfoLog(program_, sizeof(log), nullptr, log);
			NE_CORE_ERROR("RenderGL", "Link failed: {}", log);
			for (GLuint s : stages) glDeleteShader(s);
			glDeleteProgram(program_);
			program_ = 0;
			return false;
		}

		for (GLuint s : stages) glDeleteShader(s);

		CacheUniformLocations();
		return true;
	}

	// 修掉改动前 shader.cpp:13-17 的每次查询：
	// link 后遍历一次全部活动 uniform，之后只查表。
	void GLShader::CacheUniformLocations() {
		GLint count = 0;
		glGetProgramiv(program_, GL_ACTIVE_UNIFORMS, &count);

		GLint maxName = 0;
		glGetProgramiv(program_, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxName);

		std::string name(static_cast<std::size_t>(maxName ? maxName : 1), '\0');

		for (GLint i = 0; i < count; ++i) {
			GLsizei length = 0;
			GLint   size = 0;
			GLenum  type = 0;
			glGetActiveUniform(program_, static_cast<GLuint>(i),
				static_cast<GLsizei>(name.size()), &length,
				&size, &type, name.data());
			name.resize(static_cast<std::size_t>(length));

			// 数组 uniform 的名字形如 "foo[0]"，同时缓存去掉下标的版本
			locations_.emplace(name, glGetUniformLocation(program_, name.c_str()));
			if (const auto bracket = name.find('['); bracket != std::string::npos) {
				locations_.emplace(name.substr(0, bracket),
					glGetUniformLocation(program_, name.c_str()));
			}
			name.resize(static_cast<std::size_t>(maxName ? maxName : 1));
		}
	}

	GLint GLShader::Location(std::string_view name) const {
		const auto it = locations_.find(std::string(name));
		return it == locations_.end() ? -1 : it->second;
	}

	void GLShader::SetParam(std::string_view name, const core::ShaderParam& value) const {
		const GLint loc = Location(name);
		if (loc < 0) return;   // uniform 未使用或被优化掉，静默跳过

		std::visit([loc](const auto& v) {
			using T = std::decay_t<decltype(v)>;
			if constexpr (std::is_same_v<T, int>)          glUniform1i(loc, v);
			else if constexpr (std::is_same_v<T, float>)   glUniform1f(loc, v);
			else if constexpr (std::is_same_v<T, glm::vec2>) glUniform2fv(loc, 1, glm::value_ptr(v));
			else if constexpr (std::is_same_v<T, glm::vec3>) glUniform3fv(loc, 1, glm::value_ptr(v));
			else if constexpr (std::is_same_v<T, glm::vec4>) glUniform4fv(loc, 1, glm::value_ptr(v));
			else if constexpr (std::is_same_v<T, glm::mat3>) glUniformMatrix3fv(loc, 1, GL_FALSE, glm::value_ptr(v));
			else if constexpr (std::is_same_v<T, glm::mat4>) glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(v));
			}, value);
	}

	void GLShader::Destroy() {
		if (program_ != 0) {
			glDeleteProgram(program_);
			program_ = 0;
		}
		locations_.clear();
	}

}