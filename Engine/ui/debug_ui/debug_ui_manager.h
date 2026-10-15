// debug_ui_manager.h
#pragma once

#include <imgui/imgui.h>
// ← 删除: #include <imgui/backends/imgui_impl_opengl3.h>
// ← 删除: #include <imgui/backends/imgui_impl_glfw.h>
#include <ui/ui_backend.h>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace nova_engine::ui::debug_ui {
	class DebugUIManager {
	public:
		static DebugUIManager& GetInstance() {
			static DebugUIManager instance;
			return instance;
		}

		// ← 参数从 (GLFWwindow*, const char* glslVersion) 改为 IUiBackend&
		bool Init(IUiBackend& backend);
		void Shutdown();

		void BeginFrame();
		void EndFrame();

		void RegisterPanel(const std::string& name, std::function<void()> callback,
			bool visible = true);
		void TogglePanel(const std::string& name);
		bool IsPanelVisable(const std::string& name) const;

	private:
		DebugUIManager() = default;
		~DebugUIManager() = default;

		void DrawAllPanels();

		IUiBackend* backend_ = nullptr;   // ← 新增：非拥有
		bool show_ = false;
		bool initialized_ = false;
		std::vector<std::pair<std::string, std::function<void()>>> panels_;
		std::unordered_map<std::string, bool> visibility_;
	};
}