#pragma once

#include <render/core/enums.h>
#include <render/core/handles.h>
#include <render/core/pipeline_state.h>
#include <render/core/shader_param.h>

#include <cstdint>
#include <glm/mat4x4.hpp>
#include <string_view>

namespace nova_engine::render::device {
	struct Rect {
		int x = 0, y = 0;
		int width = 0, height = 0;
	};

	struct ClearValue {
		static constexpr uint32_t kMaxColorAttachments = 4;
		float color[kMaxColorAttachments][4] = {
			{ 0.0f,0.0f,0.0f,1.0f },
			{ 1.0f,0.0f,0.0f,0.0f },
			{ 0.0f,0.0f,0.0f,1.0f },
			{ 0.0f,0.0f,0.0f,1.0f },
		};

		float depth = 1.0f;
		int stencil = 0;
	};

	class ICommandList {
	public:
		virtual ~ICommandList() = default;

		virtual void BeginRenderTarget(
			core::RenderTargetHandle,
			core::LoadOp colorLoad, 
			core::LoadOp depthLoad, 
			const ClearValue&) = 0;

		virtual void EndRenderTarget() = 0;

		virtual void SetViewport(const Rect&) = 0;
		//设置完整管线状态
		virtual void SetPipelineState(const core::PipelineState&) = 0;
		//设置uniform. name是着色器代码中的变量名
		virtual void SetShaderParam(std::string_view name, const core::ShaderParam&) = 0;
		//把纹理绑到槽位unit，并让sampler这个uniform指向该槽位
		virtual void BindTexture(uint32_t unit, std::string_view sampler, 
			core::TextureHandle, const core::SamplerState* samplerState = nullptr) = 0;

		//用model变换整个网格。利用当前管线状态绘制Mesh
		virtual void DrawMesh(core::MeshHandle, const glm::mat4& model) = 0;
	};
}