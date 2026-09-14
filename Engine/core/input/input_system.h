#pragma once

#include "input_state.h"
#include <core/world/world.h>
#include <render/opengl/context/render_context.h>

namespace nova_engine::core::input {
	class InputSystem {
	public:
		static InputSystem& GetInstance() {
			static InputSystem instance;
			return instance;
		}

		InputState& GetState() { return state_; }

		void Update(double time, world::World& world, render::opengl::context::RenderContext& context);

		InputSystem(const InputSystem&) = delete;
		InputSystem& operator=(const InputSystem&) = delete;
	private:
		InputState state_;

		InputSystem() = default;
		~InputSystem() = default;
	};
}