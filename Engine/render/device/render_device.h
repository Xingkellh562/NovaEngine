#pragma once

#include <render/core/handles.h>
#include <render/core/mesh_data.h>
#include <render/core/render_target_desc.h>
#include <render/core/shader_desc.h>
#include <render/core/texture_desc.h>

#include <cstdint>
#include <span>
#include <string_view>

namespace nova_engine::render::device {
	class ICommandList;

	struct FrameInfo {
		uint32_t backBufferWidth = 0;
		uint32_t backBufferHeight = 0;
		double deltaSeconds = 0.0;
	};

	/*渲染设备：唯一的图形API收口
	1.所有Craete返回的句柄在Destroy之前一直有效
	2.OnResize 不改变任何句柄，只重新分配背后的存储
	3.所有方法只能在渲染线程调用*/
	class IRenderDevice {
	public:
		virtual ~IRenderDevice() = default;

		//创建一个网格并分配句柄
		virtual core::MeshHandle CreateMesh(const core::MeshData&) = 0;
		//用新数据覆盖已有网格；传空MeshData表示释放显存但保留句柄
		virtual void UpdateMesh(core::MeshHandle, const core::MeshData&) = 0;
		//删除网格句柄并释放显存
		virtual void DestroyMesh(core::MeshHandle) = 0;

		//pixels的排布必须与的desc.format一致;pixels为空表示只分配句柄不进行初始化
		virtual core::TextureHandle CreateTexture(const core::TextureDesc&,
			std::span<const std::byte> pixels) = 0;

		//按desc.sources的路径逐个加载并上传
		virtual core::TextureHandle CreateTextureArray(const core::TextureArrayDesc&) = 0;
		virtual void DestroyTexture(core::TextureHandle) = 0;

		virtual core::ShaderHandle CreateShader(const core::ShaderDesc&) = 0;
		virtual void DestroyShader(core::ShaderHandle) = 0;

		virtual core::RenderTargetHandle CreateRenderTarget(const core::RenderTargetDesc&) = 0;
		virtual void DestroyRenderTarget(core::RenderTargetHandle) = 0;
		
		//查询给定渲染目标的附件纹理句柄
		virtual core::TextureHandle GetAttachment(core::RenderTargetHandle,std::string_view attachmentName)const = 0;

		//返回全屏四边形，顶点布局为VertexLayoutId::ScreenQuad
		virtual core::MeshHandle FullscreenQuad() const = 0;

		virtual void BeginFrame(const FrameInfo&) = 0;
		virtual ICommandList& Commands() = 0;
		virtual void Present() = 0;

		virtual void OnResize(uint32_t width, uint32_t height) = 0;
		virtual core::RenderTargetHandle BackBuffer() const = 0;

		virtual const char* Name() const = 0;
	};
}