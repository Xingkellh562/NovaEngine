#include "glmesh.h"
#include <log/logger.h>


namespace nova_engine::render::opengl {
	GLMesh::GLMesh(const MeshData& data) {
		elementLength_ = data.indices.size();
		//在这里绑定所有的gl对象
		glGenVertexArrays(1, &vao_);
		glGenBuffers(1, &vbo_);
		glGenBuffers(1, &ebo_);

		glBindVertexArray(vao_);
		//把顶点数组复制到缓冲中供OpenGL使用
		glBindBuffer(GL_ARRAY_BUFFER, vbo_);
		glBufferData(GL_ARRAY_BUFFER, data.vertices.size() * sizeof(float), data.vertices.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, data.indices.size() * sizeof(int), data.indices.data(), GL_STATIC_DRAW);
		//设置顶点属性指针
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);

		if (vao_ == 0 || vbo_ == 0 || ebo_ == 0) {
			NE_CORE_ERROR("RenderSystem","Mesh load failed");
		}
	}

	GLMesh::~GLMesh() {
		Cleanup();
	}

	GLMesh::GLMesh(GLMesh&& other) noexcept {
		vao_ = other.vao_;
		other.vao_ = 0;
		vbo_ = other.vbo_;
		other.vbo_ = 0;
		ebo_ = other.ebo_;
		other.ebo_ = 0;
		elementLength_ = other.elementLength_;
		other.elementLength_ = 0;
	}

	GLMesh& GLMesh::operator=(GLMesh&& other) noexcept {
		if (this != &other) {
			Cleanup();
			vao_ = other.vao_;
			other.vao_ = 0;
			vbo_ = other.vbo_;
			other.vbo_ = 0;
			ebo_ = other.ebo_;
			other.ebo_ = 0;
			elementLength_ = other.elementLength_;
			other.elementLength_ = 0;
		}
		return *this;
	}

	void GLMesh::Bind() const {
		glBindVertexArray(vao_);      //绑定顶点数组对象VAO
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
	}

	void GLMesh::Cleanup() {
		if (vao_ != 0)
			glDeleteVertexArrays(1, &vao_);
		if (vbo_ != 0)
			glDeleteBuffers(1, &vbo_);
		if (ebo_ != 0)
			glDeleteBuffers(1, &ebo_);
	}
}