#pragma once

#include <vector>
namespace nova_engine::render {
	struct MeshData {
		std::vector<float> vertices;
		std::vector<int> indices;

		bool isValid() const {
			return !vertices.empty() && !indices.empty();
		}
	};
}