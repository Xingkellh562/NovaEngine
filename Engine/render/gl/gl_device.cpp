#include "gl_device.h"

#include "gl_enums.h"

#include <log/logger.h>

#include <stdexcept>

namespace nova_engine::render::gl {

	using namespace nova_engine::render;

	GLDevice::GLDevice() : commands_(*this) {
		CreateFullscreenQuad();
		const uint32_t id = targets_.Add(GLRenderTarget{true});
		TargetRecord rec; rec.isBackBuffer = true;
		targetRecords_[id] = std::move(rec);
		backBuffer_ = core::RenderTargetHandle{ id };
	}

	GLDevice::~GLDevice() {
		// 顺序：先释放依赖设备的缓存，再释放资源
		commands_.ReleaseCaches();

		for (uint32_t id = 1; id <= meshes_.Capacity(); ++id) {
			if (GLMeshBuffers* m = meshes_.Get(id)) m->Destroy();
		}
		for (uint32_t id = 1; id <= textures_.Capacity(); ++id) {
			if (GLTexture* t = textures_.Get(id)) t->Destroy();
		}
		for (uint32_t id = 1; id <= targets_.Capacity(); ++id) {
			if (GLRenderTarget* rt = targets_.Get(id)) rt->Destroy();
		}
		for (uint32_t id = 1; id <= shaders_.Capacity(); ++id) {
			if (GLShader* s = shaders_.Get(id)) s->Destroy();
		}
	}

	core::MeshHandle GLDevice::CreateMesh(const core::MeshData& data) {
		GLMeshBuffers buffers;
		buffers.UploadVoxel(data);
		return core::MeshHandle{ meshes_.Add(std::move(buffers)) };
	}

	void GLDevice::UpdateMesh(core::MeshHandle handle, const core::MeshData& data) {
		if (GLMeshBuffers* m = meshes_.Get(handle.id)) m->UploadVoxel(data);
	}

	void GLDevice::DestroyMesh(core::MeshHandle handle) {
		if (GLMeshBuffers* m = meshes_.Get(handle.id)) m->Destroy();
		commands_.InvalidateMesh(handle.id);
		meshes_.Remove(handle.id);
	}

	core::TextureHandle GLDevice::CreateTexture(const core::TextureDesc& desc,
		std::span<const std::byte> pixels) {
		GLTexture texture;
		if (!texture.Create2D(desc, pixels.empty() ? nullptr : pixels.data())) {
			NE_CORE_ERROR("RenderGL", "CreateTexture failed ({}x{})", desc.width, desc.height);
			return {};
		}
		return core::TextureHandle{ textures_.Add(std::move(texture)) };
	}

	core::TextureHandle GLDevice::CreateTextureArray(
		const core::TextureArrayDesc& desc,
		std::span<const std::byte* const> layerPixels) {

		// ★ 断言而不是兜底：这些字段现在由 assets::LoadTextureArray 保证非零。
		//   如果这里为 0，说明调用方绕过了加载器。
		if (desc.tileSize == 0 || desc.layers == 0) {
			NE_CORE_ERROR("RenderGL",
				"CreateTextureArray: tileSize={} layers={} — "
				"desc 未经 assets::LoadTextureArray 填充？",
				desc.tileSize, desc.layers);
			return {};
		}
		if (layerPixels.size() != desc.layers) {
			NE_CORE_ERROR("RenderGL",
				"CreateTextureArray: got {} layer pointers, desc says {}",
				layerPixels.size(), desc.layers);
			return {};
		}

		GLTexture texture;
		if (!texture.CreateArray(desc, layerPixels)) {
			NE_CORE_ERROR("RenderGL", "CreateTextureArray failed");
			return {};
		}
		return core::TextureHandle{ textures_.Add(std::move(texture)) };
	}

	void GLDevice::DestroyTexture(core::TextureHandle handle) {
		if (GLTexture* t = textures_.Get(handle.id)) t->Destroy();
		textures_.Remove(handle.id);
	}

	core::ShaderHandle GLDevice::CreateShader(const core::ShaderDesc& desc) {
		GLShader shader;
		if (!shader.Create(desc)) return {};
		return core::ShaderHandle{ shaders_.Add(std::move(shader)) };
	}

	void GLDevice::DestroyShader(core::ShaderHandle handle) {
		if (GLShader* s = shaders_.Get(handle.id)) s->Destroy();
		shaders_.Remove(handle.id);
	}

	core::RenderTargetHandle GLDevice::CreateRenderTarget(const core::RenderTargetDesc& desc) {
		TargetRecord record;

		for (const core::RenderTargetAttachment& a : desc.attachments) {
			AttachmentRecord att;
			att.name = a.name;        // ★
			att.isDepth = a.isDepth;
			att.colorIndex = a.index;
			att.fixedWidth = a.fixedWidth;
			att.fixedHeight = a.fixedHeight;

			if (a.imported.Valid()) {
				if (textures_.Get(a.imported.id) == nullptr) {
					NE_CORE_ERROR("RenderGL",
						"CreateRenderTarget: imported attachment '{}' has an invalid handle", a.name);
					return {};
				}
				att.handle = a.imported;
				att.owned = false;
			}
			else {
				core::TextureDesc td;
				td.width = a.fixedWidth ? a.fixedWidth : width_;
				td.height = a.fixedHeight ? a.fixedHeight : height_;
				td.format = a.format;
				td.defaultSampler = { core::TextureFilter::Linear, core::TextureFilter::Linear,
									  core::TextureWrap::ClampToEdge };
				td.depthCompareMode = (a.isDepth && a.fixedWidth != 0);

				// ★ 提前拦截：0 尺寸纹理挂上去就是 0x8CD6，但错误会在几层之外才浮现
				if (td.width == 0 || td.height == 0) {
					NE_CORE_ERROR("RenderGL",
						"CreateRenderTarget: attachment '{}' has zero size "
						"(did you forget device.OnResize before creating resources?)", a.name);
					return {};
				}

				att.desc = td;
				att.handle = CreateTexture(td, {});
				att.owned = true;

				if (!att.handle.Valid()) {
					NE_CORE_ERROR("RenderGL",
						"CreateRenderTarget: failed to create texture for attachment '{}'", a.name);
					return {};
				}
			}

			if (!a.isDepth) ++record.colorAttachmentCount;
			record.attachments.push_back(std::move(att));
		}

		GLRenderTarget target;
		if (!target.Create(MakeBindings(record))) {
			NE_CORE_ERROR("RenderGL", "CreateRenderTarget failed");
			return {};
		}

		const uint32_t id = targets_.Add(std::move(target));
		targetRecords_[id] = std::move(record);
		return core::RenderTargetHandle{ id };
	}

	void GLDevice::DestroyRenderTarget(core::RenderTargetHandle handle) {
		if (!handle.Valid()) return;
		const auto it = targetRecords_.find(handle.id);
		if (it != targetRecords_.end()) {
			for (const AttachmentRecord& att : it->second.attachments) {
				// 只销毁自有的附件；imported 的附件属于别的渲染目标
				if (att.desc.width != 0) DestroyTexture(att.handle);
			}
			targetRecords_.erase(it);
		}
		if (GLRenderTarget* rt = targets_.Get(handle.id)) rt->Destroy();
		targets_.Remove(handle.id);
	}

	core::TextureHandle GLDevice::GetAttachment(core::RenderTargetHandle handle,
		std::string_view name) const {
		const auto it = targetRecords_.find(handle.id);
		if (it == targetRecords_.end()) {
			NE_CORE_ERROR("RenderGL", "GetAttachment: render target not found");
			return {};
		}

		for (const AttachmentRecord& att : it->second.attachments) {
			if (att.name == name) return att.handle;
		}

		NE_CORE_ERROR("RenderGL", "GetAttachment: attachment '{}' not found", name);
		return {};
	}

	std::vector<AttachmentBinding> GLDevice::MakeBindings(const TargetRecord& rec) const {
		std::vector<AttachmentBinding> bindings;
		bindings.reserve(rec.attachments.size());
		for (const AttachmentRecord& a : rec.attachments) {
			GLuint id = 0;
			if (const GLTexture* t = textures_.Get(a.handle.id)) id = t->id();
			bindings.push_back({ {}, id, a.isDepth, a.colorIndex });
		}
		return bindings;
	}

	GLuint GLDevice::FramebufferOf(core::RenderTargetHandle handle) const {
		if (!handle.Valid()) return 0;                       // 默认帧缓冲
		const GLRenderTarget* rt = targets_.Get(handle.id);
		if (rt == nullptr) return 0;
		const auto it = targetRecords_.find(handle.id);
		if (it != targetRecords_.end() && it->second.isBackBuffer) {
			if (rt->fbo() != 0)
				NE_CORE_ERROR("RenderGL", "backbuffer != 0!", rt->fbo());
			return 0;                                  // 永远回到默认帧缓冲
		}
		return rt->fbo();
	}

	bool GLDevice::HasColorAttachment(core::RenderTargetHandle handle, uint32_t index) const {
		const auto it = targetRecords_.find(handle.id);
		if (it == targetRecords_.end()) return false;
		if (it->second.isBackBuffer) return index == 0;
		for (const AttachmentRecord& a : it->second.attachments) {
			if (!a.isDepth && a.colorIndex == index) return true;
		}
		return false;
	}

	// ===========================================================================
	// 帧
	// ===========================================================================

	void GLDevice::BeginFrame(const device::FrameInfo& info) {
		width_ = info.backBufferWidth;
		height_ = info.backBufferHeight;
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void GLDevice::Present() {
		// 交换缓冲由平台层（GLFW）负责，见第 6.6 节的 Window 参考实现。
		// 之所以不在这里 swap：设备不应该知道窗口的存在。
	}

	// 见上方「与 resize 的配合」——必须是两阶段，且必须用 ReallocateStorage。
	void GLDevice::OnResize(uint32_t width, uint32_t height) {
		width_ = width;
		height_ = height;

		// 阶段 1：重新分配所有自有附件的存储。
		// GL id 不变，所以 GLRenderTarget 里已捕获的挂接记录依然有效。
		for (auto& [id, record] : targetRecords_) {
			if (record.isBackBuffer) continue;
			ReallocateTarget(record, width, height);
		}

		// 阶段 2：全部纹理就位后再统一重新挂接，
		// 避免 unordered_map 遍历顺序导致「挂到旧纹理」。
		for (auto& [id, record] : targetRecords_) {
			if (record.isBackBuffer) continue;
			if (GLRenderTarget* rt = targets_.Get(id)) {
				rt->Rebuild(MakeBindings(record));
			}
		}
	}

	void GLDevice::ReallocateTarget(TargetRecord& record, uint32_t width, uint32_t height) {
		for (AttachmentRecord& att : record.attachments) {
			if (att.desc.width == 0) continue;   // imported，不归本目标管理

			// 先把最终尺寸写回 desc，ReallocateStorage 与 MakeBindings 都以它为准
			att.desc.width = att.fixedWidth ? att.fixedWidth : width;
			att.desc.height = att.fixedHeight ? att.fixedHeight : height;

			if (GLTexture* t = textures_.Get(att.handle.id)) {
				t->ReallocateStorage(att.desc);
			}
		}
	}

	// ===========================================================================
	// 内建全屏四边形
	// ===========================================================================

	void GLDevice::CreateFullscreenQuad() {
		// 布局 VertexLayoutId::ScreenQuad: slot0 = pos2, slot1 = uv2
		// 对应改动前 post_process_renderer.cpp:8-15 的 6 顶点四边形（改为 4 顶点 + 索引）
		core::MeshData quad;
		quad.vertices = {
			-1.0f,-1.0f,
			-1.0f,1.0f,
			1.0f,1.0f,
			1.0f,-1.0f,
		};
		quad.uvws = {
			0.0f, 0.0f,
			0.0f, 1.0f,
			1.0f, 1.0f,
			1.0f, 0.0f,
		};
		quad.indices = { 0, 1, 2, 0, 2, 3 };

		GLMeshBuffers buffers;
		buffers.UploadScreenQuad(quad);
		fullscreenQuad_ = core::MeshHandle{ meshes_.Add(std::move(buffers)) };
	}

}