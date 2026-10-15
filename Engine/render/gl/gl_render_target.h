#pragma once

#include <glad/glad.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nova_engine::render::gl {

	// 一个附件在 FBO 上的挂接描述。
	//
	// 注意：这个结构体只持有纹理的 GL id，**不拥有纹理**。
	// 纹理的创建 / 重建 / 销毁全部由 GLDevice 负责。
	struct AttachmentBinding {
		std::string name;                  // 仅用于日志与调试
		GLuint      texture = 0;
		bool        isDepth = false;
		uint32_t    colorIndex = 0;        // 颜色附件序号；深度附件忽略此字段
	};

	// 只负责 FBO 的挂接与 draw buffers 配置。
	//
	//  创建 / 销毁 FBO，把附件挂上去，配置 glDrawBuffers，做完整性检查
	// 不创建纹理、不保存 TextureDesc、不重建存储、不决定尺寸
	//
	// 这条边界让「谁拥有纹理」这个问题只有一个答案：GLDevice。
	// 于是 GetAttachment() 返回的 TextureHandle 一定能被 BindTexture() 解析。
	class GLRenderTarget {
	public:
		explicit GLRenderTarget(bool defaultFramebuffer = false) : defaultFramebuffer_(defaultFramebuffer) {}
		~GLRenderTarget() { Destroy(); }

		GLRenderTarget(const GLRenderTarget&) = delete;
		GLRenderTarget& operator=(const GLRenderTarget&) = delete;
		GLRenderTarget(GLRenderTarget&& other) noexcept;
		GLRenderTarget& operator=(GLRenderTarget&& other) noexcept;

		// 建 FBO 并完成首次挂接
		bool Create(const std::vector<AttachmentBinding>& bindings);

		// 重新挂接。用于 resize 之后纹理 GL id 变化的情况。
		// 若 FBO 尚未创建，等价于 Create。
		void Rebuild(const std::vector<AttachmentBinding>& bindings);

		void Destroy();

		void Bind() const { glBindFramebuffer(GL_FRAMEBUFFER, fbo_); }

		GLuint fbo() const { return fbo_; }

	private:
		GLuint fbo_ = 0;

		bool defaultFramebuffer_ = false;

		// 保留最近一次的挂接记录。Debug 构建下可用于断言 / 打印，
		// Release 下不产生任何开销 —— 它的存在也让 Rebuild 可以省略参数重载。
		std::vector<AttachmentBinding> bindings_;

		void Apply(const std::vector<AttachmentBinding>& bindings);
		void AttachAll();
		void RebuildDrawBuffers();
	};

}