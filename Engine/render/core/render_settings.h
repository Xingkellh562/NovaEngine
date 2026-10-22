#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstdint>

namespace nova_engine::render {
	struct RenderSettings {
		bool useShadow = true;
		bool useLucency = true;
		bool useLod = true;
		bool debugUI = true;

		float lodSwitchDistance = 64;

		float shadowOrthoSize = 64.0f;
		uint32_t shadowResolution = 2048;
		float shadowDistance = 128.0f;
		glm::vec3 lightDirection = { 0.3f, 0.8f, 0.5f };

		glm::vec4 clearColor{ 0.4f,0.5f,0.9f,1.0f };
	};
}