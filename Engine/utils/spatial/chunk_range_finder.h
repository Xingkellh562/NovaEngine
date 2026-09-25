#pragma once

#include <vector>
#include <glm/vec3.hpp>

namespace nova_engine::utils::spatial {
	const std::vector<glm::ivec3> FindChunkInRange(int range);
	const std::vector<glm::ivec3> GetLatticeCooInRange(int range, int size);
}