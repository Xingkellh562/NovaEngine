#pragma once

#include <glad/glad.h>
#include <render/mesh_data.h>

namespace nova_engine::render::opengl {
	class GLMesh {
	public:
		GLMesh(const MeshData& data);
		~GLMesh();

		GLMesh(const GLMesh&) = delete;
		GLMesh& operator=(const GLMesh&) = delete;

		GLMesh(GLMesh&&) noexcept;
		GLMesh& operator=(GLMesh&&) noexcept;

		void Bind() const;
		int GetElementLength() const { return elementLength_; }
	private:
		GLuint vao_;
		GLuint vbo_;
		GLuint ebo_;

		int elementLength_;

		void Cleanup();
	};
}