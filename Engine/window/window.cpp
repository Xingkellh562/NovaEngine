#include "window.h"

#include <render/gl/gl_context.h>       // ← 唯一 include render/gl 的地方（组装点）
#include <render/gl/imgui_gl_backend.h>
#include <render/system/render_system.h>
#include <ui/debug_ui/debug_ui_manager.h>
#include <render/device/render_device.h>   

#include <voxel/chunk/chunk_system.h>
#include <core/input/input_system.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace rd = nova_engine::render;

namespace nova_engine::window {
	constexpr double FIXED_STEP_MS = 1000.0 / 60.0;  // 16.666...
	constexpr double MAX_ACCUM_MS = 100.0;

	Window::Window() = default;
	Window::~Window() = default;

	Window& Window::GetInstance() {
		static Window instance;
		return instance;
	}

	void Window::framebuffer_size_callback(GLFWwindow* window, int width, int height)
	{
		if (width == 0 || height == 0) return;
		Window& self = Window::GetInstance();
		if (!self.device_) return;
		Window::GetInstance().device_->OnResize(
			static_cast<uint32_t>(width), static_cast<uint32_t>(height));
		self.width_ = width;
		self.height_ = height;
	}

	void Window::processInput(GLFWwindow* window)
	{
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			glfwSetWindowShouldClose(window, true);
		
	}

	void Window::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
		nova_engine::core::input::InputState* state = &nova_engine::core::input::InputSystem::GetInstance().GetState();
		Window& self = Window::GetInstance();
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
			case GLFW_KEY_F1:
				self.settings_.useShadow = !self.settings_.useShadow;
				break;
			case GLFW_KEY_F2:
				self.settings_.useLucency= !self.settings_.useLucency;
				break;
			case GLFW_KEY_F3:
				self.settings_.useLod = !self.settings_.useLod;
				break;
			case GLFW_KEY_F4:
				self.settings_.debugUI = !self.settings_.debugUI;
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

	void Window::mouseButtonCallBack(GLFWwindow* window,int button ,int action,int mods) {
		nova_engine::core::input::InputState* state = &nova_engine::core::input::InputSystem::GetInstance().GetState();
		if (state == nullptr) return;
		if (action == GLFW_PRESS) {
			if (button == GLFW_MOUSE_BUTTON_LEFT) {
				state->destroyBlock = true;
			}
			if (button == GLFW_MOUSE_BUTTON_RIGHT) {
				state->buildBlock = true;
			}
		}
		
	}

	void Window::scrollCallBack(GLFWwindow* window, double xoffset, double yoffset) {
		nova_engine::core::input::InputState* state = &nova_engine::core::input::InputSystem::GetInstance().GetState();
		if (state == nullptr) return;
		if (yoffset > 0) {
			state->nowBlock = std::clamp(++state->nowBlock, 1, 11);
		}
		else if (yoffset < 0) {
			state->nowBlock = std::clamp(--state->nowBlock, 1, 11);
		}
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
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_SAMPLES, 0);

		window_ = glfwCreateWindow(800, 600, "NovaEngine", nullptr, nullptr);
		if (window_ == nullptr) {
			NE_CORE_ERROR("GameCore", "Failed to create window");
			glfwTerminate();
			return;
		}

		// ← 取代原来的 glfwMakeContextCurrent + gladLoadGLLoader
		//    （window.cpp:240-246），GL 相关代码全部收进 render/gl
		//device_ = rd::gl::CreateDevice(window_);
		device_ = render::gl::CreateDevice(window_);

		if (!device_) {
			NE_CORE_ERROR("GameCore", "Failed to create render device");
			return;
		}

		glfwSetFramebufferSizeCallback(window_, framebuffer_size_callback);
		glfwSetKeyCallback(window_, keyCallback);
		glfwSetCursorPosCallback(window_, mouseMoveCallBack);
		glfwSetMouseButtonCallback(window_, mouseButtonCallBack);
		glfwSetScrollCallback(window_, scrollCallBack);

		glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		glfwSetCursorPos(window_, 400, 300);
		glfwSwapInterval(0);
		glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

		// ← 删除 glEnable(GL_DEPTH_TEST) / glDepthMask / glEnable(GL_CULL_FACE)
		//    / glCullFace / glEnable(GL_MULTISAMPLE)（window.cpp:259-263）
		//    这些都已成为 PipelineState 的一部分

		// ImGui 初始化：先建核心上下文，再交给后端
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		ImGui::StyleColorsDark();

		uiBackend_ = std::make_unique<rd::gl::ImguiGLBackend>(window_);
		if (!uiBackend_->Init()) {
			NE_CORE_ERROR("GameCore", "Failed to init UI backend");
		}
		ui::debug_ui::DebugUIManager::GetInstance().Init(*uiBackend_);

		
	}
	// 调用：int x = rnd(1, 100);

	float fps = 1.0f;

	void Window::Load() {
		device_->OnResize(width_, height_);
		resources_ = rd::framework::CreateRenderResources(*device_, 800, 600);

		rd::system::RenderSystem::GetInstance().Init(*device_);

		// ... 调试面板注册
		ui::debug_ui::DebugUIManager::GetInstance().RegisterPanel(
			"Debug", [this]() {
				auto& chunkMeshs = render::system::RenderSystem::GetInstance().GetChunkMesh();
				size_t meshCount = chunkMeshs.Layer(render::scene::MeshLayer::Default)->MeshCount();
				size_t poolSize = chunkMeshs.Layer(render::scene::MeshLayer::Default)->PoolLength();
				size_t subCount = render::system::RenderSystem::GetInstance().GetMeshRequireCount();
				glm::vec3 playerPos = world_.playerPos;
				ImGui::Text("Application average %.3f ms / frame (%.1f FPS) ",
					1000.0f / fps, fps);
				ImGui::Text("playerPos: < x: %.2f , y: %.2f , z: %.2f >",
					playerPos.x, playerPos.y, playerPos.z);

				ImGui::Text("Chunk mesh count: %d", meshCount);
				ImGui::Text("Pool length: %d", poolSize);

				ImGui::Text("Chunk count: %d", world_.planet.chunkData.Size());
				ImGui::Text("Chunk pool length: %d", world_.planet.chunkData.PoolSize());

				ImGui::Text("Chunk mesh build submit count: %d", subCount);

				ImGui::Text("Pointed block: <%d,%d,%d> <%s>",
					world_.selectedBlock.x, world_.selectedBlock.y, world_.selectedBlock.z, world_.blockName.c_str());

				ImGui::Text("Seleted block: <%s>", world_.heldBlock.c_str());
					
			},true
		);

		view_ = rd::system::RenderSystem::GetInstance().BuildView(world_, settings_);
	}

	void Window::Render(double time) {
		
		auto& cmd = device_->Commands();

		device_->BeginFrame({ static_cast<unsigned int>(width_), static_cast<unsigned int>(height_), 0.0 });

		frameRenderer_.Execute(view_, resources_, cmd);

		fps = ImGui::GetIO().Framerate;

		if (settings_.debugUI) {
			ui::debug_ui::DebugUIManager::GetInstance().BeginFrame();
			ui::debug_ui::DebugUIManager::GetInstance().EndFrame();
		}

		glfwSwapBuffers(window_);   // ← 唯一的 platform 调用
	}

	void Window::Update(double time) {
		nova_engine::core::input::InputSystem::GetInstance().Update(time, world_);   // ← 少一个参数
		glfwPollEvents();
		voxel::chunk::ChunkSystem::GetInstance().Update(world_);

		view_ = rd::system::RenderSystem::GetInstance().BuildView(world_, settings_);
		view_.viewportWidth = width_;
		view_.viewportHeight = height_;
	}

	void Window::Unload() {
		rd::system::RenderSystem::GetInstance().Shutdown();
		rd::framework::DestroyRenderResources(*device_, resources_);
		ui::debug_ui::DebugUIManager::GetInstance().Shutdown();
		ImGui::DestroyContext();
		uiBackend_.reset();
		device_.reset();
		glfwTerminate();
		NE_CORE_INFO("GameCore", "Unload game.");
	}
}