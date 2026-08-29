#pragma once

#include "input_state.h"

namespace nova_engine::core::input {
	class InputSystem {
	public:
		static InputSystem& GetInstance() {
			static InputSystem instance;
			return instance;
		}

		InputState& GetState() { return state_; }

		void Update(double time);

		InputSystem(const InputSystem&) = delete;
		InputSystem& operator=(const InputSystem&) = delete;
	private:
		InputState state_;

		InputSystem() = default;
		~InputSystem() = default;
	};
}