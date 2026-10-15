#pragma once

#include <render/device/command_list.h>

#include "slot_table.h"

#include <glad/glad.h>

#include <cstdint>
#include <unordered_map>

namespace nova_engine::render::gl {

	class GLDevice;

	class GLCommandList final : public device::ICommandList {
	public:
		explicit GLCommandList(GLDevice& device) : device_(device) {}
		~GLCommandList() override;

		void BeginRenderTarget(core::RenderTargetHandle, core::LoadOp colorLoad,
			core::LoadOp depthLoad,
			const device::ClearValue&) override;
		void EndRenderTarget() override;

		void SetViewport(const device::Rect&) override;

		void SetPipelineState(const core::PipelineState&) override;
		void SetShaderParam(std::string_view name, const core::ShaderParam&) override;
		void BindTexture(uint32_t unit, std::string_view sampler, core::TextureHandle, const core::SamplerState* samplerState = nullptr) override;

		void DrawMesh(core::MeshHandle, const glm::mat4& model) override;

		void InvalidateMesh(uint32_t meshId);
		// GLDevice 析构时调用
		void ReleaseCaches();

	private:
		GLDevice& device_;

		core::RenderTargetHandle boundTarget_{};
		core::PipelineState      currentPipeline_{};
		bool               hasPipeline_ = false;

		std::unordered_map<uint64_t, GLuint> vaoCache_;      // (meshId << 8) | layout
		std::unordered_map<uint64_t, GLuint> samplerCache_;  // SamplerState 的哈希

		GLuint GetOrCreateVao(uint32_t meshId, core::VertexLayoutId layout);
		GLuint GetOrCreateSampler(const core::SamplerState&);

		void ApplyPipelineState(const core::PipelineState&);
	};

}