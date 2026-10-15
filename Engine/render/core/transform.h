#pragma once

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace nova_engine::render::core {
	inline glm::mat4 MakeModelMatrix(const glm::vec3& position,
		const glm::vec3& rotation,
		const glm::vec3& origin,
		const glm::vec3& up = glm::vec3(0.0f,1.0f,0.0f)) 
	{
		const glm::vec3 fixedPos = position - origin;
		glm::mat4 view = glm::lookAt(fixedPos, fixedPos + rotation, up);
		return glm::inverse(view);
	}
}