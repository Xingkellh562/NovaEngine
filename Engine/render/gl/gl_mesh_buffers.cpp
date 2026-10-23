#include "gl_mesh_buffers.h"

#include <utility>

namespace nova_engine::render::gl {

	GLMeshBuffers::GLMeshBuffers(GLMeshBuffers&& o) noexcept
		: vbos_(o.vbos_), ebo_(o.ebo_),
		indexCount_(o.indexCount_), screenQuad_(o.screenQuad_) {
		o.vbos_.fill(0);
		o.ebo_ = 0;
		o.indexCount_ = 0;
	}
	GLMeshBuffers& GLMeshBuffers::operator=(GLMeshBuffers&& o) noexcept {
		if (this != &o) {
			Destroy();
			vbos_ = o.vbos_;
			ebo_ = o.ebo_;
			indexCount_ = o.indexCount_;
			screenQuad_ = o.screenQuad_;
			o.vbos_.fill(0);
			o.ebo_ = 0;
			o.indexCount_ = 0;
		}
		return *this;
	}

	void GLMeshBuffers::UploadStream(GLuint vbo, const void* data, std::size_t bytes) {
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		// 对应改动前 glmesh.cpp:120-121 的 glBufferData(NULL) + glBufferSubData 两步；
		// 单次 glBufferData 少一次驱动分配
		glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(bytes), data, GL_STATIC_DRAW);
	}

	void GLMeshBuffers::UploadVoxel(const core::MeshData& data) {
		screenQuad_ = false;
		indexCount_ = static_cast<uint32_t>(data.indices.size());

		if (vbos_[0] == 0) glGenBuffers(static_cast<GLsizei>(vbos_.size()), vbos_.data());
		if (ebo_ == 0)     glGenBuffers(1, &ebo_);

		// 空网格：释放显存（对应改动前 glmesh.cpp:98-115）
		if (!data.IsValid()) {
			for (GLuint& vbo : vbos_) {
				glBindBuffer(GL_ARRAY_BUFFER, vbo);
				glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
			}
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
			indexCount_ = 0;
			return;
		}

		UploadStream(vbos_[0], data.vertices.data(), data.vertices.size() * sizeof(float));
		UploadStream(vbos_[1], data.uvws.data(), data.uvws.size() * sizeof(float));
		UploadStream(vbos_[2], data.aos.data(), data.aos.size() * sizeof(float));
		UploadStream(vbos_[3], data.normals.data(), data.normals.size() * sizeof(float));
		UploadStream(vbos_[4], data.colors.data(), data.colors.size() * sizeof(float));

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER,
			static_cast<GLsizeiptr>(data.indices.size() * sizeof(uint32_t)),
			data.indices.data(), GL_STATIC_DRAW);
	}

	void GLMeshBuffers::UploadScreenQuad(const core::MeshData& data) {
		screenQuad_ = true;
		indexCount_ = static_cast<uint32_t>(data.indices.size());

		if (vbos_[0] == 0) glGenBuffers(4, vbos_.data());
		if (ebo_ == 0)     glGenBuffers(1, &ebo_);

		// ScreenQuad 布局：vertices 解释为 vec2、uvws 解释为 vec2
		UploadStream(vbos_[0], data.vertices.data(), data.vertices.size() * sizeof(float));
		UploadStream(vbos_[1], data.uvws.data(), data.uvws.size() * sizeof(float));

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER,
			static_cast<GLsizeiptr>(data.indices.size() * sizeof(uint32_t)),
			data.indices.data(), GL_STATIC_DRAW);
	}

	void GLMeshBuffers::Destroy() {
		for (GLuint& vbo : vbos_) {
			if (vbo != 0) { glDeleteBuffers(1, &vbo); vbo = 0; }
		}
		if (ebo_ != 0) { glDeleteBuffers(1, &ebo_); ebo_ = 0; }
		indexCount_ = 0;
	}

}