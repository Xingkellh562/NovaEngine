#pragma once

#include <render/core/render_settings.h>
#include <render/framework/frame_renderer.h>
#include <render/framework/render_resources.h>
#include <render/scene/render_view.h>

#include <core/world/world.h>

#include <memory>

struct GLFWwindow;   // ← 前置声明即可，不再 include glad / glfw3.h

namespace nova_engine::render::device { class IRenderDevice; }
namespace nova_engine::render::gl { class ImguiGLBackend; }

namespace nova_engine::window {
	class Window {
	public:
		static Window& GetInstance();

		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;

		void Run();

	private:
		GLFWwindow* window_ = nullptr;

		std::unique_ptr<render::device::IRenderDevice> device_;
		std::unique_ptr<render::gl::ImguiGLBackend>    uiBackend_;

		render::framework::RenderResources resources_{};
		render::framework::FrameRenderer   frameRenderer_;
		render::RenderSettings             settings_;

		core::world::World world_;
		render::scene::RenderView view_{};

		int width_ = 800, height_ = 600;

		Window();
		~Window();

		void Init();
		void Load();
		void Render(double time);
		void Update(double time);
		void Unload();

		// GLFW 回调（签名不变）
		static void framebuffer_size_callback(GLFWwindow*, int w, int h);
		static void keyCallback(GLFWwindow*, int key, int, int action, int mods);
		static void mouseMoveCallBack(GLFWwindow*, double x, double y);
		static void mouseButtonCallBack(GLFWwindow*, int button, int action, int);
		static void scrollCallBack(GLFWwindow*, double dx, double dy);
		void processInput(GLFWwindow*);
	};
}