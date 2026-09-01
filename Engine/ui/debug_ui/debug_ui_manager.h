#pragma once

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_opengl3.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>

namespace nova_engine::ui::debug_ui {
	class DebugUIManager {
	public:
		static DebugUIManager& GetInstance() {
			static DebugUIManager instance;
			return instance;
		}
		
		DebugUIManager(const DebugUIManager&) = delete;
		DebugUIManager& operator=(const DebugUIManager&) = delete;

		bool Init(GLFWwindow* window,const char* glslVersion = "#version 330");
		void Shutdown();

		void BeginFrame();
		void EndFrame();

		//注册面板
		void RegisterPanel(const std::string& name, std::function<void()> callback, bool visible = true);
		//显隐面板
		void TogglePanel(const std::string& name);
		bool IsPanelVisable(const std::string& name) const;
	private:
		DebugUIManager() = default;
		~DebugUIManager() = default;

		void DrawAllPanels();
		bool show_ = false;
		bool initialized_ = false;
		std::vector<std::pair<std::string, std::function<void()>>> panels_;
		std::unordered_map<std::string, bool> visibility_;
	};
}