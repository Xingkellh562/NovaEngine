#include "camera_library.h"
#include <log/logger.h>

namespace nova_engine::render::opengl {
	CameraLibrary::CameraLibrary() {
		AddCamera("MainCamera");
	}

	Camera* CameraLibrary::GetCamera(const std::string& name) const {
		auto it = library_.find(name);
		if (it != library_.end()) {
			return it->second.get();
		}
		else {
			NE_CORE_ERROR("RenderSystem", "Failed to find camera with name : {}", name);
		}
		return nullptr;
	}

	void CameraLibrary::AddCamera(const std::string& name) {
		library_.try_emplace(name, std::make_unique<Camera>());
		NE_CORE_INFO("RenderSystem", "Add camera with name:{}", name);
	}
}