#include "program.h"
#include <glm/vec3.hpp>
#include <render/opengl/library/shader_library.h>
#include <render/opengl/library/texture_library.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <render/opengl/camera.h>


namespace rd = nova_engine::render;
namespace rd_gl = nova_engine::render::opengl;

const rd_gl::Camera Camera = rd_gl::Camera();
glm::mat4 projection;
glm::mat4 view;

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
	projection = Camera.GetTransformMatrix(width,height);
	if (rd_gl::ShaderLibrary::GetInstance().GetShader("BaseShader") != nullptr)
		rd_gl::ShaderLibrary::GetInstance().GetShader("BaseShader")->SetMat4("projection", projection);
}

void processInput(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);
}

int render() {
	//初始化OpenGL
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);  //指定openGL版本号
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);//指定openGL渲染模式为立即模式

	//创建一个大小为800*600的窗口
	GLFWwindow* window = glfwCreateWindow(800, 600, "GANGLV", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	//将创建完的窗口的上下文设置为当前线程的主上下文
	glfwMakeContextCurrent(window);
	//初始化glad以使用openGL函数
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	glViewport(0, 0, 800, 600);//设置视口大小
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);//绑定窗口尺寸改变的回调函数
	projection = Camera.GetTransformMatrix(800, 600);

	glClearColor(0.2f, 0.3f, 0.6f, 1.0f);//设置清屏色 我要Blue哈哈哈

	

	float vertices[] = {
	0.5f, 0.5f, 4.0f,0,0,0,   // 右上角
	0.5f, -0.5f, 4.0f,1,0,1,  // 右下角
	-0.5f, -0.5f, 3.0f,1,1,0,// 左下角
	-0.5f, 0.5f, 3.0f,0,1,0,  // 左上角
	};

	unsigned int indices[] = {
		// 注意索引从0开始! 
		// 此例的索引(0,1,2,3)就是顶点数组vertices的下标，
		// 这样可以由下标代表顶点组合成矩形

		0, 1, 3, // 第一个三角形
		1, 2, 3  // 第二个三角形
	};

	//绑定VBO
	unsigned int VBO;
	glGenBuffers(1, &VBO);
	//绑定VAO
	unsigned int VAO;
	glGenVertexArrays(1, &VAO);
	//绑定EBO
	unsigned int EBO;
	glGenBuffers(1, &EBO);

	glBindVertexArray(VAO);
	//把顶点数组复制到缓冲中供OpenGL使用
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
	//设置顶点属性指针
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	//弄一张临时图片
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, (rd_gl::TextureLibrary::GetInstance().GetTexture("Grass")->get()));

	const rd_gl::Shader* shader = rd_gl::ShaderLibrary::GetInstance().GetShader("BaseShader");
	//std::move();
	if (shader == nullptr) {
		return -1;
	}
	shader->Use();
	shader->SetMat4("projection", projection);
	shader->SetTextureUnit("uTexture",0);

	auto console = spdlog::stdout_color_mt("console");
	
	//渲染循环，启动!!
	while (!glfwWindowShouldClose(window))
	{
		processInput(window);

		//我渲染:
		glClear(GL_COLOR_BUFFER_BIT);//清除颜色缓冲(使用清屏色)
		
		shader->Use(); //指定着色器程序
		view = Camera.GetViewMatrix();

		shader->SetMat4("view", view);

		glBindVertexArray(VAO);      //绑定顶点数组对象VAO
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glDrawElements(GL_TRIANGLES, sizeof(indices) / sizeof(int), GL_UNSIGNED_INT, 0);
		//glDrawArrays(GL_TRIANGLES, 0, sizeof(vertices)/ sizeof(float));//以三角形形式绘制顶点

		glfwSwapBuffers(window);//交换前后缓冲区
		glfwPollEvents();
	}
	//退出前释放资源
	glfwTerminate();

	return 0;
}