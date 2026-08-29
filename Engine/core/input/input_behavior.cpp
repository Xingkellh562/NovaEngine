#include "input_behavior.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <glm/gtc/constants.hpp>
#include <cmath>

namespace nova_engine::core::input {
    glm::vec3 MoveUpdate(
        double deltaTime,
        double speed,
        glm::vec3 lookUp,
        glm::vec3 worldUp,
        InputState& state)
    {
        float dt = static_cast<float>(deltaTime / 1000.0); // 转换为秒

        // 加速和减速速率（可调，单位：1/s）
        const float ACCEL = 12.0f;   // 从0到1约需 0.083s
        const float DECEL = 10.0f;   // 自然衰减速率

        // 辅助 lambda：更新单个轴值
        auto updateAxis = [&](float& axis, bool pos, bool neg) {
            if (pos) {
                axis += ACCEL * dt;
                if (axis > 1.0f) axis = 1.0f;
            }
            else if (neg) {
                axis -= ACCEL * dt;
                if (axis < -1.0f) axis = -1.0f;
            }
            else {
                // 无输入时自然减速归零
                if (axis > 0.0f) {
                    axis -= DECEL * dt;
                    if (axis < 0.0f) axis = 0.0f;
                }
                else if (axis < 0.0f) {
                    axis += DECEL * dt;
                    if (axis > 0.0f) axis = 0.0f;
                }
            }
            };

        // 更新三个轴
        updateAxis(state.axisZ, state.forward, state.back);
        updateAxis(state.axisX, state.right, state.left);
        updateAxis(state.axisY, state.up, state.down);

        // 计算各方向上的速度分量（未限制总速度）
        float speedZ = static_cast<float>(speed) * state.axisZ;
        float speedX = static_cast<float>(speed) * state.axisX;
        float speedY = static_cast<float>(speed) * state.axisY;

        // --- 构建正交移动基 ---
        glm::vec3 forward = glm::normalize(lookUp);
        glm::vec3 up = glm::normalize(worldUp);

        // 处理 forward 与 up 平行（或接近平行）的情况
        glm::vec3 right;
        float dot = glm::dot(forward, up);
        if (std::abs(dot) > 0.999f) {
            // 若平行，使用一个临时向量构建正交基
            glm::vec3 temp = (std::abs(forward.y) < 0.9f) ? glm::vec3(0.0f, 1.0f, 0.0f)
                : glm::vec3(1.0f, 0.0f, 0.0f);
            right = glm::normalize(glm::cross(forward, temp));
            up = glm::normalize(glm::cross(right, forward)); // 重新计算真正的上方向
        }
        else {
            right = glm::normalize(glm::cross(forward, up));
            up = glm::normalize(glm::cross(right, forward));
        }

        // 根据坐标系习惯，检查 right 方向是否正确（通常 cross(前, 上) 得到左方向，取反得右）
        // 这里假设我们想要的是“右手系”的右方向，即 forward × up = -right（取决于具体实现）
        // 若发现移动左右颠倒，可取消下面一行的注释：
        // right = -right;

        // --- 合成最终速度并限制总大小 ---
        glm::vec3 velocity = forward * speedZ + right * speedX + up * speedY;
        float maxSpeed = static_cast<float>(speed);
        float len = glm::length(velocity);
        if (len > maxSpeed) {
            velocity = (velocity / len) * maxSpeed; // 归一化后缩放
        }

        // 乘以时间增量得到位移
        return velocity * dt;
    }
}