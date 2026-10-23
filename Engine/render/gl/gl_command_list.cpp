#include "gl_command_list.h"

#include "gl_device.h"
#include "gl_enums.h"
#include "gl_mesh_buffers.h"
#include "gl_render_target.h"
#include "gl_shader.h"
#include "gl_texture.h"

#include <log/logger.h>

#include <functional>

namespace nova_engine::render::gl {

	namespace {
		uint64_t HashSampler(const core::SamplerState& s) {
			return (static_cast<uint64_t>(s.minFilter) << 16)
				| (static_cast<uint64_t>(s.magFilter) << 8)
				| static_cast<uint64_t>(s.wrap)
				| (static_cast<uint64_t>(s.depthCompareMode) << 24);
		}
	} // namespace

	GLCommandList::~GLCommandList() = default;

	// ---------------------------------------------------------------------------
	// 渲染目标
	// ---------------------------------------------------------------------------

	void GLCommandList::BeginRenderTarget(core::RenderTargetHandle handle,
		core::LoadOp colorLoad,
		core::LoadOp depthLoad,
		const device::ClearValue& clear) {
		boundTarget_ = handle;

		const GLuint fbo = device_.FramebufferOf(handle);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);

		// 与改动前不同：这里对每个颜色附件分别清屏，替代 glClearBufferfv
		// （render_system.cpp:106-110）
		GLenum buffers[device::ClearValue::kMaxColorAttachments] = {};
		for (uint32_t i = 0; i < device::ClearValue::kMaxColorAttachments; ++i) {
			buffers[i] = GL_COLOR_ATTACHMENT0 + i;
		}

		if (colorLoad == core::LoadOp::Clear) {
			for (uint32_t i = 0; i < device::ClearValue::kMaxColorAttachments; ++i) {
				if (!device_.HasColorAttachment(handle, i)) continue;
				glClearBufferfv(GL_COLOR, static_cast<GLint>(i), clear.color[i]);
			}
		}

		if (depthLoad == core::LoadOp::Clear) {
			glDepthMask(GL_TRUE);
			glClearBufferfi(GL_DEPTH_STENCIL, 0, clear.depth, clear.stencil);
		}

		hasPipeline_ = false;   // 新的渲染目标后需要重新应用状态
	}

	void GLCommandList::EndRenderTarget() {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		boundTarget_ = {};
		hasPipeline_ = false;
	}

	void GLCommandList::SetViewport(const device::Rect& r) {
		glViewport(r.x, r.y, r.width, r.height);
	}

	// ---------------------------------------------------------------------------
	// 管线状态
	// ---------------------------------------------------------------------------

	void GLCommandList::ApplyPipelineState(const core::PipelineState& ps) {
		// 着色器
		if (GLShader* shader = device_.ShaderOf(ps.shader)) {
			shader->Use();
		}

		// 混合（对应 render_system.cpp:24-26 与 semi_transparent_renderer.cpp:84-100）
		switch (ps.blendMode) {
		case core::BlendMode::Opaque:
			glDisable(GL_BLEND);
			break;

		case core::BlendMode::AlphaBlend:
			glEnable(GL_BLEND);
			glBlendEquation(GL_FUNC_ADD);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			break;

		case core::BlendMode::Additive:
			glEnable(GL_BLEND);
			glBlendEquation(GL_FUNC_ADD);
			glBlendFunc(GL_ONE, GL_ONE);
			break;

		case core::BlendMode::WboitAccum:
			// 对应 semi_transparent_renderer.cpp:99-100：
			//   glBlendFunci(0, GL_ONE, GL_ONE)
			//   glBlendFunci(1, GL_ZERO, GL_ONE_MINUS_SRC_COLOR)
			glEnable(GL_BLEND);
			glBlendEquation(GL_FUNC_ADD);
			glBlendFunci(0, GL_ONE, GL_ONE);
			glBlendFunci(1, GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
			break;
		}

		// 深度
		if (ps.depthTest == core::DepthTest::None) {
			glDisable(GL_DEPTH_TEST);
		}
		else {
			glEnable(GL_DEPTH_TEST);
			glDepthFunc(ToGLDepthFunc(ps.depthTest));
		}
		glDepthMask(ps.depthWrite ? GL_TRUE : GL_FALSE);

		// 颜色写入掩码（对应 semi_transparent_renderer.cpp:53）
		if (ps.colorWrite) {
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		}
		else {
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		}

		// 剔除（对应 render_system.cpp:37, 53, 78 与 semi_transparent_renderer.cpp:56-57）
		if (ps.cullMode == core::CullMode::None) {
			glDisable(GL_CULL_FACE);
		}
		else {
			glEnable(GL_CULL_FACE);
			glCullFace(ToGLCullFace(ps.cullMode));
		}

		// 采样器：绑到所有单元，取代改动前直接改纹理滤波的做法
		//（render_system.cpp:102 / 139 —— 这就是 bug 1 的根治点）
		const GLuint sampler = GetOrCreateSampler(ps.samplerState);
		for (uint32_t unit = 0; unit < 8; ++unit) {
			glBindSampler(unit, sampler);
		}
	}

	void GLCommandList::SetPipelineState(const core::PipelineState& ps) {
		if (hasPipeline_ && currentPipeline_.shader.id == ps.shader.id
			&& currentPipeline_.blendMode == ps.blendMode
			&& currentPipeline_.depthTest == ps.depthTest
			&& currentPipeline_.depthWrite == ps.depthWrite
			&& currentPipeline_.colorWrite == ps.colorWrite
			&& currentPipeline_.cullMode == ps.cullMode
			&& currentPipeline_.samplerState == ps.samplerState) {
			return;   // 同一 Pass 内的重复设置直接跳过
		}

		currentPipeline_ = ps;
		hasPipeline_ = true;
		ApplyPipelineState(ps);
	}

	void GLCommandList::SetShaderParam(std::string_view name, const core::ShaderParam& value) {
		if (GLShader* shader = device_.ShaderOf(currentPipeline_.shader)) {
			shader->SetParam(name, value);
		}
	}

	void GLCommandList::BindTexture(uint32_t unit, std::string_view sampler,
		core::TextureHandle handle,const core::SamplerState* samplerState) {
		GLTexture* texture = device_.TextureOf(handle);
		if (texture == nullptr || !texture->valid()) return;

		glActiveTexture(GL_TEXTURE0 + unit);
		glBindTexture(texture->target(), texture->id());

		const core::SamplerState& state = samplerState ? *samplerState : currentPipeline_.samplerState;
		glBindSampler(unit, GetOrCreateSampler(state));

		// 让 sampler uniform 指向该槽位
		if (GLShader* shader = device_.ShaderOf(currentPipeline_.shader)) {
			shader->SetParam(sampler, static_cast<int>(unit));
		}
	}

	// ---------------------------------------------------------------------------
	// 绘制
	// ---------------------------------------------------------------------------

	GLuint GLCommandList::GetOrCreateVao(uint32_t meshId, core::VertexLayoutId layout) {
		const uint64_t key = (static_cast<uint64_t>(meshId) << 8)
			| static_cast<uint64_t>(layout);
		if (const auto it = vaoCache_.find(key); it != vaoCache_.end()) {
			return it->second;
		}

		GLMeshBuffers* mesh = device_.MeshOf(core::MeshHandle{ meshId });
		if (mesh == nullptr) return 0;

		GLuint vao = 0;
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		// 对应改动前 glmesh.cpp:151-172 的三段 VAO 设置
		switch (layout) {
		case core::VertexLayoutId::VoxelMain:
			// 0 = pos3, 1 = uvw3, 2 = ao1, 3 = normal3
			glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo(0));
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
			glEnableVertexAttribArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo(1));
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
			glEnableVertexAttribArray(1);
			glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo(2));
			glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 1 * sizeof(float), nullptr);
			glEnableVertexAttribArray(2);
			glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo(3));
			glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
			glEnableVertexAttribArray(3);
			break;

		case core::VertexLayoutId::VoxelShadow:
			// 0 = pos3
			glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo(0));
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
			glEnableVertexAttribArray(0);
			break;

		case core::VertexLayoutId::VoxelLight:
			// 0 = pos3, 1 = normal3, 2 = uvw3
			glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo(0));
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
			glEnableVertexAttribArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo(3));
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
			glEnableVertexAttribArray(1);
			glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo(1));
			glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
			glEnableVertexAttribArray(2);
			break;

		case core::VertexLayoutId::ScreenQuad:
			// 0 = pos2, 1 = uv2
			glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo(0));
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
			glEnableVertexAttribArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo(1));
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
			glEnableVertexAttribArray(1);
			break;

		case core::VertexLayoutId::UI:
			// 0 = pos2, 1 = uv2 , 2 = color4
			glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo(0));
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
			glEnableVertexAttribArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo(1));
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
			glEnableVertexAttribArray(1);
			glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo(4));
			glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
			glEnableVertexAttribArray(2);
			break;
		}

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo());
		glBindVertexArray(0);

		vaoCache_.emplace(key, vao);
		return vao;
	}

	GLuint GLCommandList::GetOrCreateSampler(const core::SamplerState& s) {
		const uint64_t key = HashSampler(s);
		if (const auto it = samplerCache_.find(key); it != samplerCache_.end()) {
			return it->second;
		}

		GLuint sampler = 0;
		glGenSamplers(1, &sampler);
		glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, ToGLMinFilter(s.minFilter));
		glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, ToGLMagFilter(s.magFilter));
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, ToGLWrap(s.wrap));
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, ToGLWrap(s.wrap));

		if (s.depthCompareMode) {
			glSamplerParameteri(sampler, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
			glSamplerParameteri(sampler, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
		}
		else {
			glSamplerParameteri(sampler, GL_TEXTURE_COMPARE_MODE, GL_NONE);
		}

		samplerCache_.emplace(key, sampler);
		return sampler;
	}

	void GLCommandList::InvalidateMesh(uint32_t meshId) {
		static_assert(static_cast<uint64_t>(core::VertexLayoutId::ScreenQuad) < 256,
			"layout needs lower eight bits of VAO key");
		for (auto it = vaoCache_.begin(); it != vaoCache_.end(); ) {
			if ((it->first >> 8) == meshId) {
				if (it->second) glDeleteVertexArrays(1, &it->second);
				it = vaoCache_.erase(it);
			}
			else ++it;
		}
	}

	void GLCommandList::DrawMesh(core::MeshHandle handle, const glm::mat4& model) {
		GLMeshBuffers* mesh = device_.MeshOf(handle);
		if (mesh == nullptr || !mesh->valid()) return;

		const GLuint vao = GetOrCreateVao(handle.id, currentPipeline_.vertexLayout);
		if (vao == 0) return;

		SetShaderParam("model", model);

		glBindVertexArray(vao);
		glDrawElements(ToGLTopology(currentPipeline_.topology),
			static_cast<GLsizei>(mesh->indexCount()),
			GL_UNSIGNED_INT, nullptr);
		glBindVertexArray(0);
	}

	void GLCommandList::ReleaseCaches() {
		for (auto& [key, vao] : vaoCache_) {
			if (vao != 0) glDeleteVertexArrays(1, &vao);
		}
		vaoCache_.clear();

		for (auto& [key, sampler] : samplerCache_) {
			if (sampler != 0) glDeleteSamplers(1, &sampler);
		}
		samplerCache_.clear();
	}

}