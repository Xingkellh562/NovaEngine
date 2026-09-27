#pragma once

#include <vector>
#include <array>

namespace nova_engine::voxel::lod {
	struct LodVoxelData {
		std::vector<unsigned int> datas_;
		std::array<std::vector<bool>,6> neighbours_;

		LodVoxelData(size_t size) :datas_(size* size* size){
			neighbours_.fill(std::vector<bool>(size * size));
		}
	};
}