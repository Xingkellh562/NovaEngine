#include "debug_ui_manager.h"

#include <log/logger.h>
#include <algorithm>

namespace nova_engine::ui::debug_ui {
	// debug_ui_manager.cpp —— 三个函数的实现变化

	bool DebugUIManager::Init(IUiBackend& backend) {
		if (initialized_) return true;
		backend_ = &backend;

		// ← ImGui::CreateContext() / StyleColorsDark() 移到了 Window::Init，
		//    因为它们是 ImGui 核心，不属于后端；这里只记录后端引用
		initialized_ = true;
		NE_CORE_INFO("DebugUISystem", "Init UI successfully!");
		return true;
	}

	void DebugUIManager::Shutdown() {
		if (!initialized_) return;
		// ← ImGui::DestroyContext() 移到 Window::Unload
		if (backend_) backend_->Shutdown();
		backend_ = nullptr;
		initialized_ = false;
		panels_.clear();
		visibility_.clear();
	}

	void DebugUIManager::BeginFrame() {
		if (!initialized_) return;
		// ← 取代 ImGui_ImplOpenGL3_NewFrame() + ImGui_ImplGlfw_NewFrame()
		if (backend_) backend_->NewFrame();
		ImGui::NewFrame();
	}

	void DebugUIManager::EndFrame() {
		if (!initialized_) return;
		DrawAllPanels();
		if (show_) ImGui::ShowDemoWindow();
		ImGui::Render();
		// ← 取代 ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData())
		if (backend_) backend_->RenderDrawData();
	}

	void DebugUIManager::DrawAllPanels() {
		for (auto& [name, callback] : panels_) {
			if (IsPanelVisable(name)) {
				ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Appearing, ImVec2(0.0f, 0.0f));
				if (ImGui::Begin(name.c_str()))
					callback();
				ImGui::End();
			}
		}
	}

	void DebugUIManager::RegisterPanel(const std::string& name,
		std::function<void()> callback, bool visible) {
		if (!callback) return;

		auto it = std::find_if(panels_.begin(), panels_.end(),
			[&name](const std::pair<std::string, std::function<void()>>& entry) {
				return entry.first == name;
			});

		if (it != panels_.end()) {
			it->second = callback;
		}
		else {
			panels_.push_back({ name, callback });
			auto vis_it = visibility_.find(name);
			if (vis_it == visibility_.end()) visibility_[name] = visible;
		}

		NE_CORE_INFO("DebugUISystem", "Registe ui panel for name: '{}'", name);
	}

	bool DebugUIManager::IsPanelVisable(const std::string& name) const {
		auto it = visibility_.find(name);
		return it != visibility_.end() && it->second;
	}
}