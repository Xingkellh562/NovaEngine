#include "glmesh.h"
#include <log/logger.h>
#include <glm/gtc/matrix_transform.hpp>


namespace nova_engine::render::opengl {
	GLMesh::GLMesh():position(0,0,0),rotation(0,0,1){
		elementLength_ = 0;
		//在这里绑定所有的gl对象
		glGenVertexArrays(1, &vao_);
		glGenVertexArrays(1, &shadowVao_);
		glGenBuffers(1, &verVbo_);
		glGenBuffers(1, &uvVbo_);
		glGenBuffers(1, &aoVbo_);
		glGenBuffers(1, &ebo_);
	}

	GLMesh::~GLMesh() {
		Cleanup();
	}

	GLMesh::GLMesh(GLMesh&& other) noexcept {
		vao_ = other.vao_;
		other.vao_ = 0;
		shadowVao_ = other.shadowVao_;
		other.shadowVao_ = 0;

		verVbo_ = other.verVbo_;
		uvVbo_ = other.uvVbo_;
		aoVbo_ = other.aoVbo_;
		other.verVbo_ = 0;
		other.uvVbo_ = 0;
		other.aoVbo_ = 0;

		ebo_ = other.ebo_;
		other.ebo_ = 0;
		position = other.position;
		other.position = { 0, 0, 0 };
		rotation = other.rotation;
		other.rotation = { 0,0,1 };
		elementLength_ = other.elementLength_;
		other.elementLength_ = 0;
	}

	GLMesh& GLMesh::operator=(GLMesh&& other) noexcept {
		if (this != &other) {
			Cleanup();
			vao_ = other.vao_;
			other.vao_ = 0;
			shadowVao_ = other.shadowVao_;
			other.shadowVao_ = 0;

			verVbo_ = other.verVbo_;
			uvVbo_ = other.uvVbo_;
			aoVbo_ = other.aoVbo_;
			other.verVbo_ = 0;
			other.uvVbo_ = 0;
			other.aoVbo_ = 0;

			ebo_ = other.ebo_;
			other.ebo_ = 0;
			position = other.position;
			other.position = { 0, 0, 0 };
			rotation = other.rotation;
			other.rotation = { 0,0,1 };
			elementLength_ = other.elementLength_;
			other.elementLength_ = 0;
		}
		return *this;
	}

	void GLMesh::GetOrUpdateData(const MeshData& data) {
		elementLength_ = data.indices.size();

		if (!vao_ || !shadowVao_) {
			glGenVertexArrays(1, &vao_);
			glGenVertexArrays(1, &shadowVao_);
			glGenBuffers(1, &verVbo_);
			glGenBuffers(1, &uvVbo_);
			glGenBuffers(1, &aoVbo_);
			glGenBuffers(1, &ebo_);
		}

		if (data.vertices.empty() || data.indices.empty()) {
			// 释放显存：如果之前分配过大缓冲区，现在归还给驱动
			// 调用 glBufferData 并传入 size=0 会释放当前缓冲区对象持有的显存
			glBindBuffer(GL_ARRAY_BUFFER, verVbo_);
			glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);

			glBindBuffer(GL_ARRAY_BUFFER, uvVbo_);
			glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);

			glBindBuffer(GL_ARRAY_BUFFER, aoVbo_);
			glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);

			elementLength_ = 0;
			return;
		}

		glBindVertexArray(vao_);
		//把顶点数组复制到缓冲中供OpenGL使用
		glBindBuffer(GL_ARRAY_BUFFER, verVbo_);
		glBufferData(GL_ARRAY_BUFFER, data.vertices.size() * sizeof(float), data.vertices.data(), GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);

		glBindBuffer(GL_ARRAY_BUFFER, uvVbo_);
		glBufferData(GL_ARRAY_BUFFER, data.uvws.size() * sizeof(float), data.uvws.data(), GL_STATIC_DRAW);

		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);

		glBindBuffer(GL_ARRAY_BUFFER, aoVbo_);
		glBufferData(GL_ARRAY_BUFFER, data.aos.size() * sizeof(float), data.aos.data(), GL_STATIC_DRAW);

		glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 1 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(2);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, data.indices.size() * sizeof(int), data.indices.data(), GL_STATIC_DRAW);
		//设置shadowVao
		glBindVertexArray(shadowVao_);
		glBindBuffer(GL_ARRAY_BUFFER, verVbo_);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);

		if (vao_ == 0 || verVbo_ == 0 || uvVbo_ == 0 || aoVbo_ == 0 || ebo_ == 0) {
			NE_CORE_ERROR("RenderSystem", "Mesh load failed");
		}
	}

	const glm::mat4 GLMesh::GetModelMatrix() const {
		glm::mat4 view = glm::lookAt(position,position+rotation,glm::vec3(0,1,0));
		glm::mat4 model = glm::inverse(view);
		return model;
	}

	void GLMesh::Bind(VaoType v) const {
		//绑定顶点数组对象VAO
		switch (v){
		case(Main):glBindVertexArray(vao_);
			break;
		case(Shadow):glBindVertexArray(shadowVao_);
			break;
		default:glBindVertexArray(vao_);
			break;
		}
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
	}

	void GLMesh::Cleanup() {
		if (vao_ != 0)
			glDeleteVertexArrays(1, &vao_);
		if(shadowVao_ != 0)
			glDeleteVertexArrays(1, &shadowVao_);
		if (verVbo_ != 0)
			glDeleteBuffers(1, &verVbo_);
		if (uvVbo_ != 0)
			glDeleteBuffers(1, &uvVbo_);
		if (aoVbo_ != 0)
			glDeleteBuffers(1, &aoVbo_);
		if (ebo_ != 0)
			glDeleteBuffers(1, &ebo_);
	}
}