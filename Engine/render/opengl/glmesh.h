#pragma once

#include <glad/glad.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <render/mesh_data.h>

namespace nova_engine::render::opengl {
	class GLMesh {
	public:
		glm::vec3 position = glm::vec3(0, 0, 0);
		glm::vec3 rotation = glm::vec3(0, 0, 1);

		GLMesh();
		~GLMesh();

		GLMesh(const GLMesh&) = delete;
		GLMesh& operator=(const GLMesh&) = delete;

		GLMesh(GLMesh&&) noexcept;
		GLMesh& operator=(GLMesh&&) noexcept;

		void GetOrUpdateData(const MeshData& data);

		void Bind() const;
		int GetElementLength() const { return elementLength_; }
		const glm::mat4 GetModelMatrix()const;
		void Sleep() { elementLength_ = 0; }
	private:
		GLuint vao_ = 0;
		GLuint vbo_ = 0;
		GLuint ebo_ = 0;

		int elementLength_;
		

		void Cleanup();
	};
}