#pragma once

#include <vector>
#include <cmath>
#include <atomic>

namespace nova_engine::voxel::chunk {
	enum class ChunkState : uint32_t
	{
		None = 0,
		Complete = 1,
		Meshing = 2,
		InPool = 3,
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

		ChunkState GetState() { 
			ChunkState s = state_.load(std::memory_order_acquire);
			return s;
		}
		void SetState(ChunkState state) {
			state_.store(state,std::memory_order_release);
		}
	private:
		unsigned int size_;
		unsigned int offset_;
		std::vector<T> voxels_;

		std::atomic<ChunkState> state_ = ChunkState::None;

		inline int GetIndexFromCoo(int x, int y, int z) const {
			return ((x << offset_) << offset_) + (y << offset_) + z; //x->y->z
		}
		void Cleanup();
	};
}

#include "chunk.tpp"
