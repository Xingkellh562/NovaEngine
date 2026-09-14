#pragma once

namespace nova_engine::core::input {
	struct InputState {
		bool forward = false, back = false, left = false, right = false;
		bool up = false, down = false;

		bool destoryBlock = false,buildBlock = false;
		int nowBlock = 4;

		float axisZ = 0, axisX = 0, axisY = 0;

		float trunX = 0, trunY = 0;
	};
}