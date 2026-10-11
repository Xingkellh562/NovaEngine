#pragma once

#include <vector>
#include <cmath>

namespace nova_engine::voxel::chunk {
	enum ChunkState {
		Prepared = 0,
		Building = 1,
		Meshing = 2
	};
	template<typename T = int>
	class Chunk {
	public:
		Chunk(const unsigned int size) :size_(size), voxels_(size* size* size), offset_(static_cast<int>(std::log2(size))){}
		~Chunk();

		Chunk(const Chunk&) = default;
		Chunk& operator=(const Chunk&) = default;

		Chunk(Chunk&&) = default;
		Chunk& operator=(Chunk&&) = default;

		T GetVoxel(int x, int y, int z) const ;
		void SetVoxel(int x, int y, int z, T voxel);
		unsigned int Size() const { return size_; }

		inline const std::vector<T>& GetVoxelSpan() const { return voxels_; }
		inline std::vector<T>& GetWritableVoxelSpan() { return voxels_; }

		T operator[](size_t i) { return voxels_[i]; }

		ChunkState& State() { return state_; }
	private:
		unsigned int size_;
		unsigned int offset_;
		std::vector<T> voxels_;

		ChunkState state_ = Prepared;

		inline int GetIndexFromCoo(int x, int y, int z) const {
			return ((x << offset_) << offset_) + (y << offset_) + z; //x->y->z
		}
		void Cleanup();
	};
}

#include "chunk.tpp"
