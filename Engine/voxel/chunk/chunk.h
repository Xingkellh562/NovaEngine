#pragma once

#include <vector>
#include <cmath>

namespace nova_engine::voxel::chunk {
	enum ChunkState {
		Prepared = 0,
		Building = 1
	};
	class Chunk {
	public:
		Chunk(const unsigned int size) :size_(size), voxels_(size* size* size), offset_(static_cast<int>(std::log2(size))){}
		~Chunk();

		Chunk(const Chunk&) = delete;
		Chunk& operator=(const Chunk&) = delete;

		Chunk(Chunk&&) = default;
		Chunk& operator=(Chunk&&) = default;

		int GetVoxel(int x, int y, int z) const ;
		void SetVoxel(int x, int y, int z, int voxel);
		unsigned int Size() const { return size_; }

		inline const std::vector<int>& GetVoxelSpan() const { return voxels_; }
		inline std::vector<int>& GetWritableVoxelSpan() { return voxels_; }

		int& operator[](const size_t& i) { return voxels_[i]; }

		ChunkState& State() { return state_; }
	private:
		unsigned int size_;
		unsigned int offset_;
		std::vector<int> voxels_;

		ChunkState state_;

		inline int GetIndexFromCoo(int x, int y, int z) const {
			return ((x << offset_) << offset_) + (y << offset_) + z; //x->y->z
		}
		void Cleanup();
	};
}

