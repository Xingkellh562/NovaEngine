#pragma once

#include <utils/noise/samplex_noise.h>
#include <voxel/planet/planet_player_data.h>
#include <voxel/chunk/chunk.h>
#include <voxel/lod/lod_voxel_data.h>
#include <voxel/planet/planet.h>

namespace nova_engine::voxel::terrain {
	class PlanetTerrainGenerator {
	public:
		PlanetTerrainGenerator(int seed) : samplexNoise_(seed){}
		~PlanetTerrainGenerator() = default;

		PlanetTerrainGenerator(const PlanetTerrainGenerator&) = delete;
		PlanetTerrainGenerator& operator=(const PlanetTerrainGenerator&) = delete;

		float GetDensity(double x, double y, double z) const;

		/*
		向传入区块容器引用中填充体素数据，
		chunkPos为区块的世界坐标，
		playerPos为玩家的世界坐标，
		playerData为玩家在星球上的数据, 
		sampleSize为采样区域的大小*/
		void BuildChunk(chunk::Chunk<float>& chunk,glm::ivec3 chunkPos, planet::PlanetData planetData, int sampleSize = 32) const;

		lod::LodVoxelSDFData BuildLodSDF(glm::ivec3 chunkPos, planet::PlanetData planetData,int size) const;
	private:
		utils::noise::SamplexNoise samplexNoise_;
	};
}