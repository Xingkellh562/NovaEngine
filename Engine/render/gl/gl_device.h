#pragma once

#include <render/device/render_device.h>

#include "gl_command_list.h"
#include "gl_mesh_buffers.h"
#include "gl_render_target.h"
#include "gl_shader.h"
#include "gl_texture.h"
#include "slot_table.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace nova_engine::render::gl {

	class GLDevice final : public device::IRenderDevice {
	public:
		GLDevice();
		~GLDevice() override;

		// ---------------- 网格 ----------------
		core::MeshHandle CreateMesh(const core::MeshData&) override;
		void       UpdateMesh(core::MeshHandle, const core::MeshData&) override;
		void       DestroyMesh(core::MeshHandle) override;

		// ---------------- 纹理 ----------------
		core::TextureHandle CreateTexture(const core::TextureDesc&,
			std::span<const std::byte> pixels) override;
		core::TextureHandle CreateTextureArray(const core::TextureArrayDesc&) override;
		void          DestroyTexture(core::TextureHandle) override;

		// ---------------- 着色器 ----------------
		core::ShaderHandle CreateShader(const core::ShaderDesc&) override;
		void         DestroyShader(core::ShaderHandle) override;

		// ---------------- 渲染目标 ----------------
		core::RenderTargetHandle CreateRenderTarget(const core::RenderTargetDesc&) override;
		void               DestroyRenderTarget(core::RenderTargetHandle) override;
		core::TextureHandle      GetAttachment(core::RenderTargetHandle,
			std::string_view name) const override;

		// ---------------- 内建资源 ----------------
		core::MeshHandle FullscreenQuad() const override { return fullscreenQuad_; }

		// ---------------- 帧 ----------------
		void          BeginFrame(const device::FrameInfo& info) override;
		device::ICommandList& Commands() override { return commands_; }
		void          Present() override;

		void               OnResize(uint32_t width, uint32_t height) override;
		// 空句柄表示 GL 的默认帧缓冲（0）
		core::RenderTargetHandle BackBuffer() const override { return backBuffer_; }

		const char* Name() const override { return "OpenGL"; }

		// ---- 供 GLCommandList 使用的内部访问 ----
		GLShader* ShaderOf(core::ShaderHandle h) { return shaders_.Get(h.id); }
		GLTexture* TextureOf(core::TextureHandle h) { return textures_.Get(h.id); }
		GLMeshBuffers* MeshOf(core::MeshHandle h) { return meshes_.Get(h.id); }
		GLuint         FramebufferOf(core::RenderTargetHandle h) const;
		bool           HasColorAttachment(core::RenderTargetHandle, uint32_t index) const;

	private:
		// 渲染目标的附件登记：名字 -> {纹理句柄, 是否深度, 颜色序号, 固定尺寸}
		struct AttachmentRecord {
			std::string   name;
			core::TextureHandle handle{};
			core::TextureDesc   desc{};
			bool          isDepth = false;
			bool          owned = true;
			uint32_t      colorIndex = 0;
			uint32_t      fixedWidth = 0;
			uint32_t      fixedHeight = 0;
		};
		struct TargetRecord {
			std::vector<AttachmentRecord> attachments;
			uint32_t colorAttachmentCount = 0;
			bool isBackBuffer = false;
		};

		SlotTable<GLShader>       shaders_;
		SlotTable<GLTexture>      textures_;
		SlotTable<GLMeshBuffers>  meshes_;
		SlotTable<GLRenderTarget> targets_;

		std::unordered_map<uint32_t, TargetRecord> targetRecords_;

		GLCommandList commands_;
		uint32_t      width_ = 0, height_ = 0;

		core::MeshHandle fullscreenQuad_{};
		core::RenderTargetHandle backBuffer_{};

		// 把 TargetRecord 转成 GLRenderTarget 需要的挂接列表
		std::vector<AttachmentBinding> MakeBindings(const TargetRecord&) const;
		// 按固定尺寸/窗口尺寸重新分配某个渲染目标的全部附件存储（句柄不变）
		void ReallocateTarget(TargetRecord&, uint32_t width, uint32_t height);
		void CreateFullscreenQuad();
	};

}