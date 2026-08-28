#pragma once

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

namespace nova_engine::render::opengl {
	class Camera {
	public:
		Camera(float nearPlane = 0.1f, float farPlane = 1000.0f, float fieldOfView = 3.14f / 4) :
			nearPlane_(nearPlane), farPlane_(farPlane), fieldOfView_(fieldOfView),
			position(glm::vec3(0, 0, 0)), rotation(glm::vec3(0, 0, 1))
		{}
	
		~Camera() = default;

		const glm::mat4 GetTransformMatrix(int width,int height) const;
		const glm::mat4 GetViewMatrix() const;

		glm::vec3 position;
		glm::vec3 rotation;
	private:
		float nearPlane_;
		float farPlane_;
		float fieldOfView_;

		const glm::vec3 GetWorldUp() const {
			return glm::vec3(0,1,0);
		}
	};
}