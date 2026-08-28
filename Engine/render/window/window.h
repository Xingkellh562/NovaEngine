#pragma once
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace nova_engine::render::window {
	class Window {
	public:
		Window();
		~Window();

		void Init();
		void Load();
		void Render(double time);
		void Unload();
	};
}