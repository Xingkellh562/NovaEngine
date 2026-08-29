#pragma once

#include <memory>
#include <unordered_map>
#include <render/opengl/camera.h>
#include <string>

namespace nova_engine::render::opengl {
	class CameraLibrary {
	public:
		static CameraLibrary& GetInstance() {
			static CameraLibrary instance;
			return instance;
		}

		Camera* GetCamera(const std::string& name) const;

		void AddCamera(const std::string& name);

		CameraLibrary(const CameraLibrary&) = delete;
		CameraLibrary& operator=(const CameraLibrary&) = delete;
	private:
		std::unordered_map<std::string, std::unique_ptr<Camera>> library_;

		CameraLibrary();
		~CameraLibrary() = default;
		void Cleanup();
	};
}