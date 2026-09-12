#include "camera.h"
#include <glm/gtc/matrix_transform.hpp>

namespace nova_engine::render::opengl {
	const glm::mat4 Camera::GetTransformMatrix(int width, int height) const {
		return glm::perspective(fieldOfView_,((float)width)/height,nearPlane_,farPlane_);
	}
	const glm::mat4 Camera::GetViewMatrix() const {
		return glm::lookAt(position, position + rotation, Camera::GetWorldUp());
	}
	const glm::mat4 Camera::GetOrthoMatrix(float halfSize) const {
		return glm::ortho(-halfSize, halfSize,-halfSize, halfSize,0.1f,300.0f);
	}
}