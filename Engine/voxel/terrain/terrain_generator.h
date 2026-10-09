#pragma once

#include <utils/noise/noise2d.h>
#include <utils/noise/samplex_noise.h>
#include <voxel/chunk/chunk.h>
#include <voxel/lod/lod_voxel_data.h>


namespace nova_engine::voxel::terrain {
	//地形生成器
	class TerrainGenerator{
	public:
		TerrainGenerator(int seed = 6767) :seed_(seed), noise_(seed), samplexNoise_(seed) {}
		~TerrainGenerator() = default;

		TerrainGenerator(const TerrainGenerator&) = delete;
		TerrainGenerator& operator=(const TerrainGenerator&) = delete;

		void BuildChunk(chunk::Chunk& chunk,int x,int y,int z, int chunkSize = 32) const;
		void BuildChunkNormal(chunk::Chunk& chunk, int x, int y, int z, int r, int worldY, int chunkSize = 32) const;
		lod::LodVoxelData SampleLodData(int x, int y, int z, int lodSize) const;
	private:
		int seed_;
		utils::noise::Noise2D noise_;
		utils::noise::SamplexNoise samplexNoise_;

		int GetHeight(int x, int y) const;
		int GetHeightNormal(double x, double y, double z) const;
	};
}