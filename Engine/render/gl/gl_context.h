#pragma once

#include <memory>

namespace nova_engine::render::device { class IRenderDevice; }

struct GLFWwindow;

namespace nova_engine::render::gl {

	// 让 GLFW 窗口的上下文成为当前上下文，并加载 GL 函数指针。
	// 成功后返回可用的渲染设备。
	std::unique_ptr<device::IRenderDevice> CreateDevice(GLFWwindow* window);

}