#pragma once

#include <render/core/handles.h>
#include <render/core/pipeline_state.h>

#include <cstdint>

namespace nova_engine::render::device { class IRenderDevice; }
namespace nova_engine::render::framework {
	//一帧渲染所需要的所有资源句柄
	struct RenderResources {
		core::RenderTargetHandle mainFrame{};
		core::RenderTargetHandle lightFrame{};
		core::RenderTargetHandle shadowFrame{};
		core::RenderTargetHandle oitFrame{};
		core::RenderTargetHandle backBuffer{};

		core::TextureHandle mainColor{};
		core::TextureHandle mainDepth{};
		core::TextureHandle lightTex{};
		core::TextureHandle shadowMap{};
		core::TextureHandle accumTex{};
		core::TextureHandle revealTex{};

		core::TextureHandle blockTextures{};

		core::ShaderHandle baseShader{};
		core::ShaderHandle lodShader{};
		core::ShaderHandle shadowShader{};
		core::ShaderHandle lightShader{};
		core::ShaderHandle lodLightShader{};
		core::ShaderHandle depthPrepassShader{};
		core::ShaderHandle accumPassShader{};
		core::ShaderHandle postProcessShader{};

		core::PipelineState shadowPipeline{};
		core::PipelineState lodPipeline{};
		core::PipelineState opaquePipeline{};
		core::PipelineState lightPipeline{};
		core::PipelineState lodLightPipeline{};
		core::PipelineState wboitDepthPipeline{};
		core::PipelineState wboitAccumPipeline{};
		core::PipelineState postProcessPipeline{};

		core::MeshHandle fullscreenQuad{};

		core::SamplerState blockSampler{}, linearSampler{}, shadowSampler{};

		uint32_t shadowResolution = 2048;
	};

	RenderResources CreateRenderResources(device::IRenderDevice& device,
		uint32_t width, uint32_t height);

	void DestroyRenderResources(device::IRenderDevice& device, RenderResources& des);
}