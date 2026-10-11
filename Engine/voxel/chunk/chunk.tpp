namespace nova_engine::voxel::chunk {
	template<typename T>
	T Chunk<T>::GetVoxel(int x, int y, int z) const {
		int index = GetIndexFromCoo(x, y, z);
		return voxels_[index];
	}

	template<typename T>
	void Chunk<T>::SetVoxel(int x, int y, int z, T voxel) {
		int index = GetIndexFromCoo(x, y, z);
		voxels_[index] = voxel;
	}

	template<typename T>
	void Chunk<T>::Cleanup() {
		voxels_.clear();
	}

	template<typename T>
    Chunk<T>::~Chunk() {
		Cleanup();
	}
}