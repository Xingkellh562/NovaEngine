#pragma once

#include <gtest/gtest.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <render/opengl/context/chunk_mesh_cache.h>


using namespace nova_engine::render::opengl::context;

class MeshCacheTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        // 在每个测试之前运行的代码
        glfwInit();

        // 2. 关键配置：不显示窗口（隐藏）
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_FALSE); // 单缓冲省性能

        // 3. 创建 1x1 的极小窗口（只是为了满足 OpenGL 对“窗口”的依赖）
        window = glfwCreateWindow(1, 1, "OpenGL Test Context", nullptr, nullptr);
        if (!window) {
            FAIL() << "Failed to create off-screen GLFW window!";
        }

        glfwMakeContextCurrent(window);

        // 4. 加载 OpenGL 函数指针（必须）
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            FAIL() << "Failed to initialize GLAD!";
        }

        // 可选：禁用 VSync，避免测试卡在刷新率上
        glfwSwapInterval(0);
    }
    static void TearDownTestSuite() {
        // 在每个测试之后运行的代码
        // 清理：销毁窗口并终止 GLFW
        if (window) {
            glfwDestroyWindow(window);
        }
        glfwTerminate();
    }

    ChunkMeshCache meshs;
private:
    static GLFWwindow* window;
};