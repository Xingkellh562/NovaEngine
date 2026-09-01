#include "window.h"
#include <log/logger.h>
#include <render/opengl/renderer/block_renderer.h>
#include <render/opengl/glmesh.h>
#include <core/input/input_system.h>
#include <render/utils/mesh_builder.h>
#include <iostream>
#include <ui/debug_ui/debug_ui_manager.h>
#include <random>

namespace rd = nova_engine::render;
namespace rd_gl = nova_engine::render::opengl;

namespace nova_engine::window {

	constexpr double FIXED_STEP_MS = 1000.0 / 60.0;  // 16.666...
	constexpr double MAX_ACCUM_MS = 100.0;

	rd_gl::BlockRenderer* renderer;

	void Window::framebuffer_size_callback(GLFWwindow* window, int width, int height)
	{
		glViewport(0, 0, width, height);
		renderer->OnResize(width,height);
	}

	void Window::processInput(GLFWwindow* window)
	{
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			glfwSetWindowShouldClose(window, true);
		
	}

	void Window::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
		nova_engine::core::input::InputState* state = &nova_engine::core::input::InputSystem::GetInstance().GetState();
		if (action == GLFW_REPEAT || action == GLFW_PRESS) {
			switch (key)
			{
			case GLFW_KEY_W:
				state->forward = true;
				break;
			case GLFW_KEY_S:
				state->back = true;
				break;
			case GLFW_KEY_A:
				state->left = true;
				break;
			case GLFW_KEY_D:
				state->right = true;
				break;
			case GLFW_KEY_SPACE:
				state->up = true;
				break;
			case GLFW_KEY_LEFT_SHIFT:
				state->down = true;
				break;
			default:
				break;
			}
		}
		else {
			switch (key)
			{
			case GLFW_KEY_W:
				state->forward = false;
				break;
			case GLFW_KEY_S:
				state->back = false;
				break;
			case GLFW_KEY_A:
				state->left = false;
				break;
			case GLFW_KEY_D:
				state->right = false;
				break;
			case GLFW_KEY_SPACE:
				state->up = false;
				break;
			case GLFW_KEY_LEFT_SHIFT:
				state->down = false;
				break;
			default:
				break;
			}
		}
		
	}

	void Window::mouseMoveCallBack(GLFWwindow* window, double xPos, double yPos) {
		static double pastX = xPos;
		static double pastY = yPos;
		nova_engine::core::input::InputState* state = &nova_engine::core::input::InputSystem::GetInstance().GetState();
		if (state == nullptr) return;
		if(state->trunX == 0)
			state->trunX = xPos - pastX;
		if (state->trunY == 0)
			state->trunY = yPos - pastY;

		pastX = xPos, pastY = yPos;
	}

	void Window::Run() {
		Init();

		double time = glfwGetTime() * 1000;
		NE_CORE_INFO("GameCore","Successful initialization, taking {} ms" ,time);

		//这里可等待Init中的某些异步工作完成执行
		//现在还没有，所以先确保Load在Init后面执行
		//以后有需要异步完成的工作再来该改这里
		double pastTime = time;
		Load();
		time = glfwGetTime() * 1000;
		NE_CORE_INFO("GameCore", "Successfully loaded, taking {} ms", time - pastTime);

		double deltaTime = 0;

		try
		{
			while (!glfwWindowShouldClose(window_)) {
				// 1. 获取时间 & 处理事件（必须每帧都处理！）
				processInput(window_);  // ✅ 关键修改：移到外面来，每帧无条件执行

				double currentTime = glfwGetTime() * 1000.0;
				double frameDelta = currentTime - pastTime;
				pastTime = currentTime;

				// 防螺旋死锁（防止Debug断点后画面爆炸）
				if (frameDelta > MAX_ACCUM_MS) frameDelta = MAX_ACCUM_MS;

				deltaTime += frameDelta;

				// 2. 固定步长物理更新（Catch-up 机制）
				// 为了防止“死亡螺旋”，这里限制单次最大更新次数（例如 3~5 次）
				int loopCount = 0;
				while (deltaTime >= FIXED_STEP_MS && loopCount < 5) {
					Update(FIXED_STEP_MS);
					deltaTime -= FIXED_STEP_MS;
					loopCount++;
				}

				// 3. 渲染插值（黄金标准：传入 Alpha 百分比，而非绝对毫秒）
				// Alpha = 剩余时间 / 固定步长，范围 [0.0, 1.0]
				float alpha = (float)(deltaTime / FIXED_STEP_MS);
				Render(alpha);  // 你的 Render 内部需要使用这个 Alpha 进行插值

				// 注意：pastTime 已在帧开头更新过了，这里不要再更新！
			}
		}
		catch (const std::exception& exception)
		{
			NE_CORE_ERROR("GameGore", exception.what());
		}
		
		Unload();
	}

	void Window::Init() {
		glfwInit();
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);  //指定openGL版本号
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);//指定openGL渲染模式为立即模式

		//创建一个大小为800*600的窗口
		window_ = glfwCreateWindow(800, 600, "GANGLV", NULL, NULL);
		if (window_ == NULL)
		{
			NE_CORE_ERROR("RenderSystem","Failed to create window!");
			glfwTerminate();
			return;
		}

		glfwMakeContextCurrent(window_);
		//初始化glad以使用openGL函数
		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
		{
			NE_CORE_ERROR("RenderSystem", "Failed to initialize GLAD!");
			return;
		}

		glfwSetFramebufferSizeCallback(window_, framebuffer_size_callback);
		glfwSetKeyCallback(window_,keyCallback);
		glfwSetCursorPosCallback(window_, mouseMoveCallBack);

		glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		glfwSetCursorPos(window_, 400, 300);
		glfwSwapInterval(1);
		glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);

		//创建ImGui上下文
		ui::debug_ui::DebugUIManager::GetInstance().Init(window_,"#version 330");
	}

	rd_gl::GLMesh* mesh = nullptr;

	int rnd(int min, int max) {
		static std::mt19937 gen(std::random_device{}()); // 只初始化一次
		return std::uniform_int_distribution<int>(min, max)(gen);
	}
	// 调用：int x = rnd(1, 100);

	float fps = 1.0f;

	void Window::Load() {
		
		glViewport(0, 0, 800, 600);
		glClearColor(0.2f, 0.3f, 0.6f, 1.0f);
		renderer = new rd_gl::BlockRenderer();
		renderer->OnResize(800, 600);

		ui::debug_ui::DebugUIManager::GetInstance().RegisterPanel(
			"Debug", [this]() {
				ImGui::Text("Application average %.3f ms / frame (%.1f FPS) ",
					1000.0f / fps, fps);
				ImGui::Text("爸爸的爸爸叫爷爷 ");
				ImGui::Text("我也要被显示吗 ");
				ImGui::Text("? ! UIIU ! ? ");
				ImGui::Text("Nuclear_Clay是给 ");
			},true
		);

		voxel::chunk::Chunk chunk = (32);
		for (int x = 0; x < chunk.Size(); x++)
			for (int y = 0; y < chunk.Size(); y++)
				for (int z = 0; z < chunk.Size(); z++) {
					int i = rnd(1, 100);
					if (i > 97) {
						if (y < 31)
							chunk.SetVoxel(x, y, z, 2);
						else
							chunk.SetVoxel(x, y, z, 1);
					}
					else chunk.SetVoxel(x, y, z, 0);
					
				}
		mesh = new rd_gl::GLMesh(render::utils::MeshBuilder::BuildMeshFromChunk(chunk,voxel::block::BlockLibrary::GetInstance()));
	}

	void Window::Render(double time) {
		//我渲染:
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);//清除颜色缓冲(使用清屏色)

		glfwPollEvents();

		//ImGui_ImplOpenGL3_NewFrame();
		//ImGui_ImplGlfw_NewFrame();
		//ImGui::NewFrame();

		if(mesh != nullptr)
			renderer->Render(*mesh);

		fps = ImGui::GetIO().Framerate;

		ui::debug_ui::DebugUIManager::GetInstance().BeginFrame();
		ui::debug_ui::DebugUIManager::GetInstance().EndFrame();

		glfwSwapBuffers(window_);//交换前后缓冲区
	}

	void Window::Update(double time) {
		nova_engine::core::input::InputSystem::GetInstance().Update(time);
	}

	void Window::Unload() {
		glfwTerminate();
		ui::debug_ui::DebugUIManager::GetInstance().Shutdown();
		delete(mesh);
		delete(renderer);
		NE_CORE_INFO("GameCore", "Unload game.");
	}

}