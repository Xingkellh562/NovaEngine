#include "planet_terrain_generator.h"
#include <core/config/config.h>
#include <utils/spatial/spherical_transform.h>
#include <utils/assist/des_packer.h>

namespace nova_engine::voxel::terrain {
	float PlanetTerrainGenerator::GetDensity(double x, double y, double z) const {
		const float Range1 = 0.64, Range2 = 0.32, Range3 = 0.04;
		double mainNoise = samplexNoise_.Noise(x * 0.008,y * 0.008, z * 0.008);
		double rollingNoise = samplexNoise_.Noise(x * 0.024, y * 0.024, z * 0.024);
		double detailNoise = 0.0f;//samplexNoise_.Noise(x * 0.08, y * 0.08, z * 0.08);

		return static_cast<float>(
			(mainNoise * Range1 +
				rollingNoise * Range2 +
				detailNoise * Range3
				));
	}

	void PlanetTerrainGenerator::BuildChunk(chunk::Chunk<float>& chunk,
		glm::ivec3 chunkPos,
		planet::PlanetData planetData, int sampleSize) const {
		int fixedsize = sampleSize & ~(core::config::CHUNK_SIZE - 1);

		int mask = fixedsize - 1;
		int fx = static_cast<int>(chunkPos.x) & ~mask;
		int fy = static_cast<int>(chunkPos.y) & ~mask;
		int fz = static_cast<int>(chunkPos.z) & ~mask;

		glm::ivec3 center = {0,-8192,0};

		for(int x = 0;x < fixedsize;x++)
			for (int y = 0;y < fixedsize;y++)
				for (int z = 0;z < fixedsize;z++) {
					int nx = (fx + x) - center.x;
					int ny = (fy + y) - center.y;
					int nz = (fz + z) - center.z;

					float dist = glm::length(glm::vec3(nx,ny,nz));
					float noise = GetDensity(nx,ny,nz );

					float surface = 8192 + noise * 16;
					//double texNoise = (samplexNoise_.Noise((fx + x) * 0.4, (fy + y) * 0.4, (fz + z)* 0.4) + 1.0) * 6;
					//uint32_t tex = d < 0 ? static_cast<uint32_t>(texNoise) : 0;

					//密度值：-1.0表示实心(内部)，1.0表示空气(外部)
					float density = (dist - surface) / 32.0f;
					density = glm::clamp(density, -1.0f, 1.0f);

					int tex = 0;

					if (density > 0.1)tex = 0;
					else if (density > -0.1) tex = 3;
					else tex = 1;

					chunk.SetVoxel(x, y, z, utils::assist::Pack(density, tex));
				}

	}

	lod::LodVoxelSDFData PlanetTerrainGenerator::BuildLodSDF(glm::ivec3 chunkPos, planet::PlanetData planetData, int lodSize) const {
		int size = core::config::CHUNK_SIZE;
		int sampleSize = core::config::CHUNK_SIZE + 2;
		int fixedSize = lodSize >> core::config::CHUNK_BIT1 << core::config::CHUNK_BIT1;
		int scale = fixedSize / size;

		int mask = fixedSize - 1;
		int fx = static_cast<int>(chunkPos.x) & ~mask;
		int fy = static_cast<int>(chunkPos.y) & ~mask;
		int fz = static_cast<int>(chunkPos.z) & ~mask;

		glm::ivec3 center = { 0,-8192,0 };

		lod::LodVoxelSDFData data(sampleSize);

		for (int x = 0;x < sampleSize;x++)
			for (int y = 0;y < sampleSize;y++)
				for (int z = 0;z < sampleSize;z++) {
					int nx = (fx + x * scale - scale) - center.x;
					int ny = (fy + y * scale - scale) - center.y;
					int nz = (fz + z * scale - scale) - center.z;

					float dist = glm::length(glm::vec3(nx, ny, nz));
					float noise = GetDensity(nx, ny, nz);

					float surface = 8192 + noise * 16;
					//double texNoise = (samplexNoise_.Noise((fx + x) * 0.4, (fy + y) * 0.4, (fz + z)* 0.4) + 1.0) * 6;
					//uint32_t tex = d < 0 ? static_cast<uint32_t>(texNoise) : 0;

					//密度值：-1.0表示实心(内部)，1.0表示空气(外部)
					float density = (dist - surface) / 32.0f;
					density = glm::clamp(density, -1.0f, 1.0f);

					int tex = 0;

					if (density > 0.1)tex = 0;
					else if (density > -0.1) tex = 3;
					else tex = 1;

					data.datas_[x * sampleSize * sampleSize + y * sampleSize + z] = utils::assist::Pack(density, tex);
				}
		return data;
	}
}