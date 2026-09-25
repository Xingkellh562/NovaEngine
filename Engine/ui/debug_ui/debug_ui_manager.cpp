#include "debug_ui_manager.h"
#include <log/logger.h>

namespace nova_engine::ui::debug_ui {
	bool DebugUIManager::Init(GLFWwindow* window, const char* glslVersion) {
		if (initialized_) return true;

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		// 加载字体文件，并指定字符范围
		// 暂时注释，等以后用到中文字符再说
		//io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 18.0f, NULL,
		//	io.Fonts->GetGlyphRangesChineseFull()); // 加载常用全量中文字符[reference:1]

		ImGui::StyleColorsDark();

		if (!ImGui_ImplGlfw_InitForOpenGL(window, true) 
			|| !ImGui_ImplOpenGL3_Init(glslVersion)) {
			return false;
			NE_CORE_ERROR("DebugUISystem" ,"Failed to init UI!");
		}

		initialized_ = true;
		NE_CORE_INFO("DebugUISystem", "Init UI successfully!");
		return true;
	}

	void DebugUIManager::Shutdown() {
		if (!initialized_) return;
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		NE_CORE_INFO("DebugUISystem", "Delete UI context");
		initialized_ = false;
		panels_.clear();
		visibility_.clear();
	}

	void DebugUIManager::BeginFrame() {
		assert(initialized_ && "DebugUIManager not initialized!");
		if(!initialized_)
			NE_CORE_ERROR("DebugUISystem", "DebugUIManager not initialized!");

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	void DebugUIManager::EndFrame() {
		assert(initialized_ && "DebugUIManager not initialized!");
		if (!initialized_)
			NE_CORE_ERROR("DebugUISystem", "DebugUIManager not initialized!");

		DrawAllPanels();

		if (show_) ImGui::ShowDemoWindow();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}

	void DebugUIManager::RegisterPanel(const std::string& name, std::function<void()> callback, bool visible) {
		if (!callback) return;

		auto it = std::find_if(panels_.begin(), panels_.end(),
			[&name](const std::pair<std::string, std::function<void()>> entry) {
				return entry.first == name;
			});

		if (it != panels_.end()) {
			it->second = callback;
		}
		else {
			panels_.push_back({name,callback});

			auto vis_it = visibility_.find(name);
			if (vis_it == visibility_.end()) {
				visibility_[name] = visible;
			}
		}

		NE_CORE_INFO("DebugUISystem", "Registe ui panel for name: '{}'", name);
	}

	void DebugUIManager::TogglePanel(const std::string& name) {
		auto it = visibility_.find(name);
		if (it != visibility_.end()) {
			it->second = !it->second;
		}
	}

	bool DebugUIManager::IsPanelVisable(const std::string& name) const {
		auto it = visibility_.find(name);
		if (it != visibility_.end()) {
			return it->second;
		}
		return false;
	}

	void DebugUIManager::DrawAllPanels() {
		for (auto& [name, callback] : panels_) {
			if (IsPanelVisable(name)) {
				ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Appearing,ImVec2(0.0f,0.0f));
				if(ImGui::Begin(name.c_str()))
					callback();
				ImGui::End();
			}
		}
	}
}