#pragma once

#include <string>
#include "planet_chunk_data.h"

namespace nova_engine::voxel::planet {
	struct Planet {
		size_t id = 0;
		size_t radius = 1024;
		glm::vec3 pos = glm::vec3(0.0f);

		PlanetChunkData chunkData;
	};

	struct PlanetData {
		size_t id = 0;
		size_t radius = 1024;
		glm::vec3 pos = glm::vec3(0.0f);
	};
}