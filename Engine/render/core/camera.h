#pragma once

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace nova_engine::render::core {
	class Camera {
	public:
		Camera(float nearPlane = 0.1f, float farPlane = 5000.0f, float fieldOfView = 3.1415926f / 3.0f) :
			nearPlane_(nearPlane), farPlane_(farPlane), fieldOfView_(fieldOfView),
			position(glm::vec3(0, 0, 0)), rotation(glm::vec3(0, 0, 1))
		{}
	
		~Camera() = default;

		const glm::mat4 GetTransformMatrix(int width,int height) const {
			return glm::perspective(fieldOfView_, ((float)width) / height, nearPlane_, farPlane_);
		}

		const glm::mat4 GetViewMatrix() const {
			return glm::lookAt(position, position + rotation, up);
		}

		const glm::mat4 GetRelativeViewMatrix() const {
			return glm::lookAt(glm::vec3(0, 0, 0), rotation, up);
		}

		const glm::mat4 GetRelativeViewMatrix(const glm::vec3& pos) const {
			return glm::lookAt(position - pos, position - pos + rotation, up);
		}

		const glm::mat4 GetOrthoMatrix(float halfSize) const {
			return glm::ortho(-halfSize, halfSize, -halfSize, halfSize, 0.1f, 300.0f);
		}

		const glm::vec3 GetWorldUp() const {
			return glm::vec3(0, 1, 0);
		}

		glm::vec3 position{0.0f};
		glm::vec3 rotation{0.0f,0.0f,1.0f};
		glm::vec3 up{0.0f,1.0f,0.0f};
	private:
		float nearPlane_;
		float farPlane_;
		float fieldOfView_;
	};
}