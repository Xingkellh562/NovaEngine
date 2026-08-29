#pragma once

#include <glm/vec3.hpp>
#include "input_state.h"

namespace nova_engine::core::input {
	glm::vec3 MoveUpdate(double deltaTime,double speed,glm::vec3 lookUp,glm::vec3 worldUp,InputState& state);


}