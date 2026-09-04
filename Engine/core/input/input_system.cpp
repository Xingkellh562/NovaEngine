#include "input_system.h"
#include "input_behavior.h"
#include <render/opengl/library//camera_library.h>
#include <log/logger.h>


using namespace nova_engine::render::opengl;

namespace nova_engine::core::input {
	const float SPEED = 10.0f;
	void InputSystem::Update(double time,world::World& world) {
		world.playerPos += MoveUpdate(time, SPEED, world.playerRot, glm::vec3(0,1,0), state_);
		world.playerRot = UpdateRotation(time, world.playerRot, 40, state_);
	}
}