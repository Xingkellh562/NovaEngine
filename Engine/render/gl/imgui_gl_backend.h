#pragma once

#include <ui/ui_backend.h>

struct GLFWwindow;

namespace nova_engine::render::gl {

	// ImGui 的 OpenGL3 + GLFW 后端。
	// 这是全项目唯一 include imgui_impl_* 的地方。
	class ImguiGLBackend final : public ui::IUiBackend {
	public:
		explicit ImguiGLBackend(GLFWwindow* window) : window_(window) {}
		~ImguiGLBackend() override { Shutdown(); }

		bool Init() override;
		void Shutdown() override;
		void NewFrame() override;
		void RenderDrawData() override;

	private:
		GLFWwindow* window_ = nullptr;
		bool        initialized_ = false;
	};

}