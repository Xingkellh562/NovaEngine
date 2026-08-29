#include "input_system.h"
#include "input_behavior.h"
#include <render/opengl/library//camera_library.h>
#include <log/logger.h>

using namespace nova_engine::render::opengl;

namespace nova_engine::core::input {
	const float SPEED = 10.0f;
	void InputSystem::Update(double time) {
		Camera* camera = CameraLibrary::GetInstance().GetCamera("MainCamera");
		if (camera != nullptr) {
			camera->position += MoveUpdate(time,SPEED,camera->rotation,camera->GetWorldUp(),state_);
			//NE_CORE_DEBUG("InputSystem", "state.fwd：{}", state_.forward);
		}
	}
}