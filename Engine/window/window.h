#pragma once
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace nova_engine::window {
	class Window {
	public:
		static Window& GetInstance() {
			static Window instance;
			return instance;
		}

		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;

		void Run();
	private:
		GLFWwindow* window_;
		Window() = default;
		~Window() = default;

		void Init();
		void Load();
		void Render(double time);
		void Update(double time);
		void Unload();

		static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
		static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
		static void mouseMoveCallBack(GLFWwindow* window,double xPos,double yPos);
		static void mouseButtonCallBack(GLFWwindow* window, int button, int action, int mods);
		static void scrollCallBack(GLFWwindow* window,double xoffset,double yoffset);
		void processInput(GLFWwindow* window);
	};
}