#include "window.h"
#include <log/logger.h>
#include <core/input/input_system.h>
#include <render/utils/mesh_builder.h>
#include <iostream>
#include <ui/debug_ui/debug_ui_manager.h>
#include <random>
#include <core/world/world.h>
#include <render/system/render_system.h>
#include <voxel/chunk/chunk_system.h>
#include <render/opengl/library/frame_buffer_library.h>
#include <utils/spatial/spherical_transform.h>
#include <render/utils/smooth_mesher.h>

namespace rd = nova_engine::render;
namespace rd_gl = nova_engine::render::opengl;

namespace nova_engine::window {

	constexpr double FIXED_STEP_MS = 1000.0 / 60.0;  // 16.666...
	constexpr double MAX_ACCUM_MS = 100.0;

	core::world::World world;
	std::optional<rd_gl::context::RenderContext> context;

	void Window::framebuffer_size_callback(GLFWwindow* window, int width, int height)
	{
		if (width == 0 || height == 0)
			return;
		glViewport(0, 0, width, height);
		
		if (context != std::nullopt) {
			auto& real = *context;
			real.blockRenderer.OnResize(width, height);
			real.postProcessRenderer.OnResize(width, height);
			real.parallelLightRenderer.OnResize(width, height);
			real.semiTranRenderer.OnResize(width, height);
			real.lightRenderer.OnResize(width, height);
		}

		rd_gl::FrameBufferLibrary::GetInstance().OnResize(width, height);
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
			case GLFW_KEY_F1:
				if (context != std::nullopt) {
					auto& real = *context;
					real.useShadow = !real.useShadow;
				}
				break;
			case GLFW_KEY_F2:
				if (context != std::nullopt) {
					auto& real = *context;
					real.lucencyMesh = !real.lucencyMesh;
				}
				break;
			case GLFW_KEY_F3:
				if (context != std::nullopt) {
					auto& real = *context;
					real.lodMesh = !real.lodMesh;
				}
				break;
			case GLFW_KEY_F4:
				if (context != std::nullopt) {
					auto& real = *context;
					real.debugUI = !real.debugUI;
				}
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
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);  //指定openGL版本号
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);//指定openGL渲染模式为立即模式
		glfwWindowHint(GLFW_SAMPLES,0);

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
		glfwSetMouseButtonCallback(window_, mouseButtonCallBack);
		glfwSetScrollCallback(window_,scrollCallBack);

		glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		glfwSetCursorPos(window_, 400, 300);
		glfwSwapInterval(0);
		glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glEnable(GL_MULTISAMPLE);

		//创建ImGui上下文
		ui::debug_ui::DebugUIManager::GetInstance().Init(window_,"#version 330");
	}
	// 调用：int x = rnd(1, 100);

	float fps = 1.0f;
	
	render::opengl::GLMesh* mesh1;
	
	static int randInt(int min, int max) {
		static std::mt19937 gen(std::random_device{}());
		return std::uniform_int_distribution<>(min, max)(gen);
	}

	void Window::Load() {
		
		glViewport(0, 0, 800, 600);
		glClearColor(0.4f, 0.5f, 0.9f, 1.0f);
		//glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		/*renderer = new render::opengl::BlockRenderer();
		renderer->OnResize(800, 600);*/

		context.emplace();
		
		rd_gl::FrameBufferLibrary::GetInstance().OnResize(800, 600);
		if (context != std::nullopt) {
			context->blockRenderer.OnResize(800, 600);
			context->postProcessRenderer.OnResize(800, 600);
			context->parallelLightRenderer.OnResize(800, 600);
			context->semiTranRenderer.OnResize(800, 600);
			context->lightRenderer.OnResize(800, 600);
		}
			

		ui::debug_ui::DebugUIManager::GetInstance().RegisterPanel(
			"Debug", [this]() {
				auto& lodSys = render::system::RenderSystem::GetInstance().GetLodSystem();
				glm::vec2 playerPos = { world.playerPos.x, world.playerPos.z };

				auto spherPos = utils::spatial::PlaneToSphereNormal(playerPos + glm::vec2(0, 32), playerPos, 256.0f, true);

				ImGui::Text("Application average %.3f ms / frame (%.1f FPS) ",
					1000.0f / fps, fps);
				ImGui::Text("playerPos: < x: %.2f , y: %.2f , z: %.2f >",
					world.playerPos.x, world.playerPos.y, world.playerPos.z);
				ImGui::Text("planetNormal: < x: %.2f , y: %.2f , z: %.2f >",
					world.planetNormal.x, world.planetNormal.y, world.planetNormal.z);
				ImGui::Text("Chunk mesh count: %d", context ? context->chunkMesh.GetLayer(Default) ?
					context->chunkMesh.GetLayer(Default)->MeshCount() : 0 : 0);
				ImGui::Text("Pool length: %d", context ? context->chunkMesh.GetLayer(Default) ?
					context->chunkMesh.GetLayer(Default)->PoolLength() : 0 : 0);
				ImGui::Text("Chunk count: %d", world.chunkData.Size());
				ImGui::Text("Chunk pool length: %d", world.chunkData.PoolSize());

				ImGui::Text("LOD subdivide submit count: %d", lodSys.GetSubSubmitCount());
				ImGui::Text("LOD subdivide result count: %d", lodSys.GetSubResultCount());

				ImGui::Text("LOD combine submit count: %d", lodSys.GetCobSubmitCount());
				ImGui::Text("LOD combine result count: %d", lodSys.GetCobResultCount());

				ImGui::Text("Pointed block: <%d,%d,%d> <%s>",
					world.selectedBlock.x, world.selectedBlock.y, world.selectedBlock.z, world.blockName.c_str());

				ImGui::Text("Seleted block: <%s>", world.heldBlock.c_str());
					
			},true
		);

		voxel::chunk::Chunk<float> chunk(32);
		for(int x = 0;x< 32;x++)
			for (int y = 0;y < 32;y++)
				for (int z = 0;z < 32;z++) {
					if(!(x == 6 && z == 6) && x > 0 && y > 0 && z > 0 && x > y)
					/*int rad = randInt(0, 200);
					if (rad < 100)*/
						chunk.SetVoxel(x,y,z,-1.0);
					else chunk.SetVoxel(x, y, z, 1.0f);
				}

		voxel::chunk::Chunk<float> neiChunk(32);
		for (int x = 0;x < 32;x++)
			for (int y = 0;y < 32;y++)
				for (int z = 0;z < 32;z++) {
					neiChunk.SetVoxel(x, y, z, 1.0f);
				}
		std::array<const voxel::chunk::Chunk<float>*, 7> neis = {
			&neiChunk,&neiChunk ,&neiChunk ,&neiChunk ,&neiChunk ,&neiChunk,& neiChunk
		};

		render::MeshData data = render::utils::BuildSNMeshFromChunk(chunk, neis, 1);
		mesh1 = new rd_gl::GLMesh();
		mesh1->GetOrUpdateData(data);
		mesh1->rotation = glm::vec3(0.0, 0.0, -1.0);
		mesh1->position = glm::vec3(0, 128, 0);
	}

	void Window::Render(double time) {

		//我渲染:
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);//清除颜色缓冲(使用清屏色)

		glfwPollEvents();

		fps = ImGui::GetIO().Framerate;

		if (context != std::nullopt) {
			auto& real = *context;
			render::system::RenderSystem::GetInstance().Render(real);

			glEnable(GL_DEPTH_TEST);
			glEnable(GL_CULL_FACE);
			glDepthFunc(GL_LESS);
			real.blockRenderer.Render({});
			real.blockRenderer.Render(*mesh1);

			if (real.debugUI) {
				ui::debug_ui::DebugUIManager::GetInstance().BeginFrame();
				ui::debug_ui::DebugUIManager::GetInstance().EndFrame();
			}
		}

		glfwSwapBuffers(window_);//交换前后缓冲区
	}

	void Window::Update(double time) {
		if (context != std::nullopt) {
			auto& real = *context;
			nova_engine::core::input::InputSystem::GetInstance().Update(time, world, real);
		}

		voxel::chunk::ChunkSystem::GetInstance().Update(world);

		if (context != std::nullopt) {
			auto& realContext = *context;
			render::system::RenderSystem::GetInstance().Update(world, realContext);
		}
	}

	void Window::Unload() {
		glfwTerminate();
		ui::debug_ui::DebugUIManager::GetInstance().Shutdown();
		//delete(renderer);
		NE_CORE_INFO("GameCore", "Unload game.");
	}
}