#pragma once
#include <cstdint>
#include <vector>

namespace nova_engine::render::core {
	struct MeshData
	{
		std::vector<float> vertices;
		std::vector<float> uvws;
		std::vector<float> aos;
		std::vector<float> normals;
		std::vector<float> colors;
		std::vector<uint32_t> indices;

		bool IsValid() const {
			return !vertices.empty() && !indices.empty();
		}
	};
}