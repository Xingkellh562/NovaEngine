#pragma once

#include <vector>

namespace nova_engine::voxel::chunk {
	class Chunk {
	public:
		Chunk(const unsigned int size) :size_(size), voxels_(size* size* size) {}
		~Chunk();

		Chunk(const Chunk&) = delete;
		Chunk& operator=(const Chunk&) = delete;

		Chunk(Chunk&&) = default;
		Chunk& operator=(Chunk&&) = default;

		int GetVoxel(int x, int y, int z) const ;
		void SetVoxel(int x, int y, int z, int voxel);
		unsigned int Size() const { return size_; }
		inline const std::vector<int>& GetVoxelSpan() const { return voxels_; }
	private:
		unsigned int size_;
		std::vector<int> voxels_;
		inline int GetIndexFromCoo(int x, int y, int z) const {
			return x * size_ * size_ + y * size_ + z; //x->y->z
		}
		void Cleanup();
	};
}

