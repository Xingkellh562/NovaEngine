#include "gl_render_target.h"

#include <log/logger.h>

#include <algorithm>

namespace nova_engine::render::gl {

	// ===========================================================================
	// 移动语义
	//
	// GLRenderTarget 不拥有任何纹理，只拥有一个 FBO。
	// 因此移动时只需搬运 fbo_ 与挂接记录，不做任何 glDelete*。
	// 这与改动前 FrameBuffer 的移动构造（frame_buffer.cpp:13-40）形成对比 ——
	// 那里要一起搬 textures_ 这张 GLuint 表，因为纹理归它所有。
	// ===========================================================================

	GLRenderTarget::GLRenderTarget(GLRenderTarget&& other) noexcept
		: fbo_(other.fbo_), bindings_(std::move(other.bindings_)) {
		other.fbo_ = 0;
	}

	GLRenderTarget& GLRenderTarget::operator=(GLRenderTarget&& other) noexcept {
		if (this != &other) {
			Destroy();
			fbo_ = other.fbo_;
			bindings_ = std::move(other.bindings_);
			other.fbo_ = 0;
		}
		return *this;
	}

	// ===========================================================================
	// 创建与重建
	// ===========================================================================

	// 对应改动前 frame_buffer.cpp:5-6 的 glGenFramebuffers
	bool GLRenderTarget::Create(const std::vector<AttachmentBinding>& bindings) {
		if (defaultFramebuffer_) return true;

		Destroy();

		glGenFramebuffers(1, &fbo_);
		if (fbo_ == 0) {
			NE_CORE_ERROR("RenderGL", "glGenFramebuffers failed");
			return false;
		}

		Apply(bindings);
		return true;
	}

	// resize 之后重新挂接。
	//
	// 前提：bindings 里的 texture（GL id）必须仍然有效。
	// 这由 GLTexture::ReallocateStorage 保证
	void GLRenderTarget::Rebuild(const std::vector<AttachmentBinding>& bindings) {
		if (defaultFramebuffer_) return;
		if (fbo_ == 0) {
			Create(bindings);
			return;
		}
		Apply(bindings);
	}

	// 挂接 + draw buffers 的公共流程。
	//
	// 保存并恢复当前 FBO 绑定——
	// 这个函数会在渲染循环之外被调用（初始化、resize），不能假设当前绑定是 0。
	void GLRenderTarget::Apply(const std::vector<AttachmentBinding>& bindings) {
		bindings_ = bindings;

		GLint prevFbo = 0;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);

		glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
		AttachAll();
		RebuildDrawBuffers();
		glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
	}

	void GLRenderTarget::AttachAll() {
		for (const AttachmentBinding& b : bindings_) {
			if (b.texture == 0) continue;   // 未初始化，跳过（不挂接也不进 draw buffers）

			const GLenum attachment = b.isDepth
				? GL_DEPTH_ATTACHMENT
				: static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + b.colorIndex);

			glFramebufferTexture2D(GL_FRAMEBUFFER, attachment,
				GL_TEXTURE_2D, b.texture, 0);
		}

		// 对应改动前 frame_buffer.cpp:107-110
		const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			NE_CORE_ERROR("RenderGL", "Framebuffer incomplete (status = 0x{:X})",
				static_cast<unsigned>(status));
		}
	}

	//   glDrawBuffers 的第 i 个元素对应**片元着色器的输出 location i**，
	//   而不是「颜色附件 i」。因此数组必须按 colorIndex 升序排列：
	//
	//     colorBuffers[0] = GL_COLOR_ATTACHMENT0  →  out0 (outAccum)     写入 accum
	//     colorBuffers[1] = GL_COLOR_ATTACHMENT1  →  out1 (outRevealage) 写入 reveal
	//
	//   这与 accum_pass_frag.glsl 的
	//     layout(location = 0) out vec4  outAccum;
	//     layout(location = 1) out float outRevealage;
	//   一一对应。
	//
	// 改动前是按添加顺序（colorAttachmentOrder_）压入的。对现有的四个渲染目标，
	// 添加顺序与 colorIndex 顺序恰好一致，所以两者结果完全相同。这里显式排序，
	// 是为了让正确性不依赖「谁先调用 AddTexture」这个隐含约定。
	void GLRenderTarget::RebuildDrawBuffers() {
		std::vector<const AttachmentBinding*> colors;
		colors.reserve(bindings_.size());
		for (const AttachmentBinding& b : bindings_) {
			if (!b.isDepth && b.texture != 0) colors.push_back(&b);
		}

		std::sort(colors.begin(), colors.end(),
			[](const AttachmentBinding* a, const AttachmentBinding* b) {
				return a->colorIndex < b->colorIndex;
			});

		if (colors.empty()) {
			// 纯深度 / 无颜色附件（ShadowFrame），对应 frame_buffer.cpp:162-166。
			// 不写颜色，只写深度。
			GLenum none = GL_NONE;
			glDrawBuffers(1, &none);
			return;
		}

		std::vector<GLenum> buffers;
		buffers.reserve(colors.size());
		for (const AttachmentBinding* b : colors) {
			buffers.push_back(static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + b->colorIndex));
		}

		glDrawBuffers(static_cast<GLsizei>(buffers.size()), buffers.data());
	}

	void GLRenderTarget::Destroy() {
		bindings_.clear();

		// 只删 FBO。纹理归 GLDevice 所有，这里绝不碰。
		if (fbo_ != 0) {
			glDeleteFramebuffers(1, &fbo_);
			fbo_ = 0;
		}
	}

}