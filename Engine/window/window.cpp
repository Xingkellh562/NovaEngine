#include "window.h"
#include <log/logger.h>
#include <render/opengl/library/shader_library.h>
#include <render/opengl/library/texture_library.h>
#include <render/opengl/library/camera_library.h>
#include <render/opengl/glmesh.h>
#include <core/input/input_system.h>

namespace rd = nova_engine::render;
namespace rd_gl = nova_engine::render::opengl;

namespace nova_engine::window {

	constexpr double FIXED_STEP_MS = 1000.0 / 60.0;  // 16.666...
	constexpr double MAX_ACCUM_MS = 100.0;

	glm::mat4 projection;
	glm::mat4 view;

	void Window::framebuffer_size_callback(GLFWwindow* window, int width, int height)
	{
		glViewport(0, 0, width, height);
		rd_gl::Camera* camera = rd_gl::CameraLibrary::GetInstance().GetCamera("MainCamera");
		if(camera != nullptr)
			projection = camera->GetTransformMatrix(width, height);
		if (rd_gl::ShaderLibrary::GetInstance().GetShader("BaseShader") != nullptr)
			rd_gl::ShaderLibrary::GetInstance().GetShader("BaseShader")->SetMat4("projection", projection);
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
				time = glfwGetTime() * 1000;
				glfwPollEvents();
				deltaTime += time - pastTime;
				while (deltaTime >= FIXED_STEP_MS) {
					double step = std::min(deltaTime, MAX_ACCUM_MS);
					processInput(window_);
					Update(step);
					deltaTime -= step;
				}
				Render(time - pastTime);
				pastTime = time;
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
	}

	unsigned int VAO;
	unsigned int VBO;
	unsigned int EBO;

	rd_gl::GLMesh* mesh = nullptr;

	void Window::Load() {
		
		glViewport(0, 0, 800, 600);
		glClearColor(0.2f, 0.3f, 0.6f, 1.0f);
		rd_gl::Camera* camera = rd_gl::CameraLibrary::GetInstance().GetCamera("MainCamera");
		if (camera != nullptr)
			projection = camera->GetTransformMatrix(800, 600);

		mesh = new rd_gl::GLMesh({
			{
				0.5f, 0.5f, 4.0f,0,0,0,
				0.5f, -0.5f, 4.0f,1,0,1,
				-0.5f, -0.5f, 3.0f,1,1,0,
				-0.5f, 0.5f, 3.0f,0,1,0,},
			{
				0, 1, 3,
				1, 2, 3
			} });
		
		//弄一张临时图片
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, (rd_gl::TextureLibrary::GetInstance().GetTexture("Grass")->get()));

		const rd_gl::Shader* shader = rd_gl::ShaderLibrary::GetInstance().GetShader("BaseShader");
		//std::move();
		if (shader == nullptr) {
			return;
		}
		shader->Use();
		shader->SetMat4("projection", projection);
		shader->SetTextureUnit("uTexture", 0);

		auto console = spdlog::stdout_color_mt("console");
	}

	void Window::Render(double time) {
		//我渲染:
		glClear(GL_COLOR_BUFFER_BIT);//清除颜色缓冲(使用清屏色)

		rd_gl::ShaderLibrary::GetInstance().GetShader("BaseShader")->Use(); //指定着色器程序
		rd_gl::Camera* camera = rd_gl::CameraLibrary::GetInstance().GetCamera("MainCamera");
		if (camera != nullptr)
			view = camera->GetViewMatrix();

		rd_gl::ShaderLibrary::GetInstance().GetShader("BaseShader")->SetMat4("view", view);

		if (mesh != nullptr) {
			mesh->Bind();
			glDrawElements(GL_TRIANGLES,6, GL_UNSIGNED_INT, 0);
		}
		
		//glDrawArrays(GL_TRIANGLES, 0, sizeof(vertices)/ sizeof(float));//以三角形形式绘制顶点

		glfwSwapBuffers(window_);//交换前后缓冲区
	}

	void Window::Update(double time) {
		nova_engine::core::input::InputSystem::GetInstance().Update(time);
	}

	void Window::Unload() {
		glfwTerminate();
		delete(mesh);
	}

}