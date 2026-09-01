#include "chunk.h"

namespace nova_engine::voxel::chunk {
	int Chunk::GetVoxel(int x, int y, int z) const {
		int index = GetIndexFromCoo(x, y, z);
		return voxels_[index];
	}

	void Chunk::SetVoxel(int x, int y, int z, int voxel) {
		int index = GetIndexFromCoo(x, y, z);
		voxels_[index] = voxel;
	}

	void Chunk::Cleanup() {
		voxels_.clear();
	}

    Chunk::~Chunk() {
		Cleanup();
	}
}