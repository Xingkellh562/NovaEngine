#include "shader.h"
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>
namespace fs = std::filesystem;
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <log/logger.h>

namespace nova_engine::render::opengl {

	GLint Shader::GetVarLocation(const char* name) const {
		GLint loc = glGetUniformLocation(id_, name);
		assert(loc != -1 && "Could not find uniform variable in shader");
		return loc;
	}

	Shader::Shader(std::initializer_list<ShaderFile> shaders) {
		std::vector<GLuint> shaderIDs;
		NE_CORE_INFO("RenderSystem", "Creating shader program with {} shaders", shaders.size());
		shaderIDs.reserve(shaderIDs.size());

		//获取所有的着色器程序并编译
		for (const auto& shader : shaders) {
			NE_CORE_INFO(
				"RenderSystem",
				"Compiling shader: path='{}', type={}",
				shader.shaderPath,
				shader.GetTypeToString()
			);
			shaderIDs.push_back(CompileShader(shader.shaderPath, shader.shaderType));
		}
			

		id_ = glCreateProgram();
		//添加并链接所有着色器程序
		for (const auto& shaderID : shaderIDs) {
			glAttachShader(id_, shaderID);
			NE_CORE_TRACE(
				"RenderSystem",
				"Attached shader ID {} to program {}",
				shaderID,
				id_
			);
		}
			

		glLinkProgram(id_);
		NE_CORE_INFO(
			"RenderSystem",
			"Shader program (ID={}) linked successfully",
			id_
		);

		int success;
		char infoLog[512];
		glGetProgramiv(id_, GL_LINK_STATUS, &success);
		if (!success) {
			glGetProgramInfoLog(id_, 512, NULL, infoLog);
			NE_CORE_ERROR(
				"RenderSystem",
				"Shader program link failed (program ID {}): {}",
				id_,
				infoLog
			);
		}
		//链接完成，释放用完的程序句柄
		for (const auto& shaderID : shaderIDs)
			glDeleteShader(shaderID);
	}

	Shader::~Shader() {
		Cleanup();
	}

	Shader::Shader(Shader&& other) noexcept
	{
		id_ = other.id_;
		other.id_ = 0;
	}

	Shader& Shader::operator=(Shader&& other) noexcept
	{
		if (this != &other)
		{
			Cleanup();
			id_ = other.id_;
			other.id_ = 0;
		}
		return *this;
	}

	void Shader::Use() const{
		glUseProgram(id_);
	}

	void Shader::SetInt(const char* name, int value) const {
		glUniform1i(GetVarLocation(name), value);
	}

	void Shader::SetBool(const char* name, bool value) const {
		glUniform1i(GetVarLocation(name), (int)value);
	}

	void Shader::SetFloat(const char* name, float value) const {
		glUniform1f(GetVarLocation(name), value);
	}

	void Shader::SetUint(const char* name, unsigned int value) const {
		glUniform1ui(GetVarLocation(name), value);
	}

	void Shader::SetVec2(const char* name, const glm::vec2& value) const {
		glUniform2fv(GetVarLocation(name), 1, glm::value_ptr(value));
	}

	void Shader::SetVec3(const char* name, const glm::vec3& value) const {
		glUniform3fv(GetVarLocation(name), 1, glm::value_ptr(value));
	}

	void Shader::SetIVec3(const char* name, const glm::ivec3& value) const {
		glUniform3iv(GetVarLocation(name), 1, glm::value_ptr(value));
	}

	void Shader::SetVec4(const char* name, const glm::vec4& value) const {
		glUniform4fv(GetVarLocation(name), 1, glm::value_ptr(value));
	}

	void Shader::SetMat4(const char* name, const glm::mat4& value) const {
		glUniformMatrix4fv(GetVarLocation(name), 1, GL_FALSE, glm::value_ptr(value));
	}

	void Shader::SetTextureUnit(const char* name, int unit) const {
		glUniform1i(GetVarLocation(name), unit);
	}

	unsigned int Shader::CompileShader(const char* path, int shaderType) {
		unsigned int shader;
		shader = glCreateShader(shaderType);

		std::string src = LoadShaderSrc(path);
		const GLchar* shaderStr = src.c_str();

		glShaderSource(shader, 1, &shaderStr, NULL);
		glCompileShader(shader);

		int success;
		char infoLog[512];
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (!success) {
			glGetShaderInfoLog(shader, 512, NULL, infoLog);
			NE_CORE_ERROR(
				"RenderSystem",
				"Shader program compile failed (program ID {}): {}",
				id_,
				infoLog
			);
		}

		return shader;
	}

	std::string Shader::LoadShaderSrc(const char* path) {
		if (!fs::exists(path)) {
			return {};
		}

		std::fstream file;
		std::stringstream buf;

		std::string ret = "";

		file.open(path);

		if (file.is_open()) {
			buf << file.rdbuf();
			ret = buf.str();
		}
		else {
			return {};
		}

		file.close();
		return ret;
	}

	std::string Shader::StripGLSLComments(const std::string& shaderSrc) {
		std::string out;
		out.reserve(shaderSrc.size());

		enum class State {
			Normal,
			LineComment,
			BlockComment,
			String,
			Char
		};

		State state = State::Normal;

		for (size_t i = 0; i < shaderSrc.size(); ++i)
		{
			char c = shaderSrc[i];
			char next = (i + 1 < shaderSrc.size()) ? shaderSrc[i + 1] : '\0';

			switch (state)
			{
			case State::Normal:
				if (c == '/' && next == '/') {
					state = State::LineComment;
					++i;
				}
				else if (c == '/' && next == '*') {
					state = State::BlockComment;
					++i;
				}
				else if (c == '"') {
					state = State::String;
					out += c;
				}
				else if (c == '\'') {
					state = State::Char;
					out += c;
				}
				else {
					out += c;
				}
				break;

			case State::LineComment:
				if (c == '\n') {
					state = State::Normal;
					out += c;
				}
				break;

			case State::BlockComment:
				if (c == '*' && next == '/') {
					state = State::Normal;
					++i;
				}
				break;

			case State::String:
				out += c;
				if (c == '\\' && next != '\0') {
					// Escape sequence: copy next char verbatim
					out += next;
					++i;
				}
				else if (c == '"') {
					state = State::Normal;
				}
				break;

			case State::Char:
				out += c;
				if (c == '\\' && next != '\0') {
					out += next;
					++i;
				}
				else if (c == '\'') {
					state = State::Normal;
				}
				break;
			}
		}

		return out;
	}

	void Shader::Cleanup() {
		if (id_ != 0) {
			glDeleteShader(id_);
			
		}
	}
}