#pragma once

#include <render/core/mesh_data.h>
#include <render/core/pipeline_state.h>

#include <glad/glad.h>

#include <array>
#include <cstdint>

namespace nova_engine::render::gl {

	class GLMeshBuffers {
	public:
		GLMeshBuffers() = default;
		~GLMeshBuffers() { Destroy(); }

		GLMeshBuffers(const GLMeshBuffers&) = delete;
		GLMeshBuffers& operator=(const GLMeshBuffers&) = delete;
		GLMeshBuffers(GLMeshBuffers&& o) noexcept;
		GLMeshBuffers& operator=(GLMeshBuffers&& o) noexcept;

		// 体素网格：上传 4 条属性流（pos3 / uvw3 / ao1 / normal3）+ 索引
		void UploadVoxel(const core::MeshData& data);

		// 全屏四边形：上传 2 条属性流（pos2 / uv2）+ 索引
		void UploadScreenQuad(const core::MeshData& data);

		void Destroy();

		GLuint   vbo(uint32_t slot) const { return vbos_[slot]; }
		GLuint   ebo() const { return ebo_; }
		uint32_t indexCount() const { return indexCount_; }
		bool     valid() const { return indexCount_ > 0; }
		bool     isScreenQuad() const { return screenQuad_; }

	private:
		std::array<GLuint, 4> vbos_{};
		GLuint   ebo_ = 0;
		uint32_t indexCount_ = 0;
		bool     screenQuad_ = false;

		static void UploadStream(GLuint vbo, const void* data, std::size_t bytes);
	};

}