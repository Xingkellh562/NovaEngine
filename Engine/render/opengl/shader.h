#pragma once

#include <glad/glad.h>
#include <glm/fwd.hpp>
#include <initializer_list>
#include <string>

namespace nova_engine::render::opengl {

	struct ShaderFile {
		const char* shaderPath;
		GLenum shaderType;

		const char* GetTypeToString() const {
			switch (shaderType)
			{
			case GL_VERTEX_SHADER:
				return "Vertex_Shader";
			case GL_FRAGMENT_SHADER:
				return "Fragment_Shader";
			case GL_TESS_CONTROL_SHADER:
				return "Tessellation_Control_Shader";
			case GL_TESS_EVALUATION_SHADER:
				return "Tessellation_Evaluation_Shader";
			case GL_GEOMETRY_SHADER:
				return "Geometry_Shader";
			default:
				return "Unknown_Shader_Type";
			}
		}
	};

	class Shader {
	public:
		Shader() = default;

		//禁止拷贝
		Shader(const Shader&) = delete;
		Shader& operator=(const Shader&) = delete;
		//允许移动
		Shader(Shader&&) noexcept;
		Shader& operator=(Shader&&) noexcept;

		Shader(std::initializer_list<ShaderFile> shaders);
		~Shader();

		void Use() const;

		GLint GetVarLocation(const char* name) const;

		//设置uniform变量
		void SetInt(const char* name, int value) const;
		void SetFloat(const char* name, float value) const;
		void SetUint(const char* name, unsigned int value) const;
		void SetBool(const char* name, bool value) const;

		void SetVec3(const char* name, const glm::vec3& value) const;
		void SetIVec3(const char* name, const glm::ivec3& value) const;
		void SetVec4(const char* name, const glm::vec4& value) const;
		void SetMat4(const char* name, const glm::mat4& value) const;

		void SetTextureUnit(const char* name, int unit) const;

		inline GLuint GetID() const { return id_; }


	protected:
		GLuint id_{};

		unsigned int CompileShader(const char* path, int shaderType);
		std::string LoadShaderSrc(const char* path);
		std::string StripGLSLComments(const std::string& shaderSrc);

		void Cleanup();
	};
}