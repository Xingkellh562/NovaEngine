#pragma once

namespace nova_engine::ui {

	// UI 后端接口。
	//
	// 存在的意义：让 ui/ 目录只依赖 ImGui 核心，不依赖任何 imgui_impl_* 后端。
	// 实现见 render/gl/imgui_gl_backend.h。
	class IUiBackend {
	public:
		virtual ~IUiBackend() = default;

		// nativeWindow 是平台窗口句柄（GLFWwindow* 等），由具体后端解释
		virtual bool Init() = 0;
		virtual void Shutdown() = 0;

		// 每帧开始前的后端准备（对应 ImGui_ImplOpenGL3_NewFrame + ImGui_ImplGlfw_NewFrame）
		virtual void NewFrame() = 0;

		// ImGui::Render() 之后提交绘制数据
		virtual void RenderDrawData() = 0;
	};

}