#pragma once

#include <glad/glad.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <render/mesh_data.h>

namespace nova_engine::render::opengl {
	enum VaoType {
		Main = 0,Shadow = 1
	};

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

		void Bind(VaoType v) const;
		int GetElementLength() const { return elementLength_; }
		glm::mat4 GetModelMatrix()const;
		glm::mat4 GetModelMatrix(const glm::vec3& camerapos)const;
		void Sleep() { 
			//elementLength_ = 0;
			awake_ = false;
		}
		void Wake() { awake_ = true; }

		bool Awake() const { return awake_; }
		void Cleanup();
	private:
		GLuint vao_ = 0, shadowVao_ = 0;
		GLuint verVbo_ = 0,uvVbo_ = 0,aoVbo_ = 0;
		GLuint ebo_ = 0;

		int elementLength_;
		bool awake_ = true;
	};
}