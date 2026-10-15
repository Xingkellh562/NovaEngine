#include "imgui_gl_backend.h"

#include <log/logger.h>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_opengl3.h>
#include <imgui/backends/imgui_impl_glfw.h>

namespace nova_engine::render::gl {

	bool ImguiGLBackend::Init() {
		if (initialized_) return true;

		// glsl 版本号不再由上层传入 —— 它是 GL 后端的知识
		if (!ImGui_ImplGlfw_InitForOpenGL(window_, true)
			|| !ImGui_ImplOpenGL3_Init("#version 330")) {
			NE_CORE_ERROR("ImguiGL", "Failed to init ImGui GL backend");
			return false;
		}

		initialized_ = true;
		return true;
	}

	void ImguiGLBackend::Shutdown() {
		if (!initialized_) return;
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		initialized_ = false;
	}

	void ImguiGLBackend::NewFrame() {
		if (!initialized_) return;
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
	}

	void ImguiGLBackend::RenderDrawData() {
		if (!initialized_) return;
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}

}