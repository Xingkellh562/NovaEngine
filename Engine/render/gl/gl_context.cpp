#include "gl_context.h"

#include "gl_device.h"

#include <log/logger.h>

#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace nova_engine::render::gl {

	std::unique_ptr<device::IRenderDevice> CreateDevice(GLFWwindow* window) {
		glfwMakeContextCurrent(window);

		if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
			NE_CORE_ERROR("RenderGL", "Failed to initialize GLAD");
			return nullptr;
		}

		NE_CORE_INFO("RenderGL", "OpenGL {}, GLSL {}",
			reinterpret_cast<const char*>(glGetString(GL_VERSION)),
			reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));

		return std::make_unique<GLDevice>();
	}

}