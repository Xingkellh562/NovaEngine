#include "planet_terrain_generator.h"
#include <core/config/config.h>
#include <utils/spatial/spherical_transform.h>

namespace nova_engine::voxel::terrain {
	int PlanetTerrainGenerator::GetHeightNormal(double x, double y, double z) const {
		const int Range1 = 64, Range2 = 32, Range3 = 8;
		const int SeaLevel = -64;
		double mainHeight = samplexNoise_.Noise(x, y, z);
		double rollingHeight = samplexNoise_.Noise(x * 4.0, y * 4.0, z * 4.0);
		double detailHeight = samplexNoise_.Noise(x * 16.0, y * 16.0, z * 16.0);

		return static_cast<int>(
			(mainHeight * Range1 +
				rollingHeight * Range2 +
				detailHeight * Range3
				) + SeaLevel);
	}

	void PlanetTerrainGenerator::BuildChunkNormal(chunk::Chunk& chunk,
		const planet::PlanetPlayerData& playerData,
		glm::vec3 playerPos, glm::vec3 chunkPos, int R,int sampleSize) const {
		int fixedsize = sampleSize & ~(core::config::CHUNK_SIZE - 1);

		int mask = fixedsize - 1;
		int fx = static_cast<int>(chunkPos.x) & ~mask;
		int fz = static_cast<int>(chunkPos.z) & ~mask;

		int fpx = static_cast<int>(playerPos.x) & ~mask;
		int fpz = static_cast<int>(playerPos.z) & ~mask;
		//计算偏移法线获取四个角点的法线

		//auto ori = glm::normalize(playerData.orientation * utils::spatial::ComputeRollQuaternion({ fx - playerPos.x, fz - playerPos.z }, static_cast<double>(R)));
		glm::dvec3 n00 = utils::spatial::ComputeRollQuaternion({ fx - fpx, fz - fpz }, static_cast<double>(R)) * playerData.normal;
		//utils::spatial::OffsetNormal(playerData.normal, glm::dvec2(fx - playerPos.x, fz - playerPos.z), static_cast<double>(R));
		glm::dvec3 n01 = utils::spatial::OffsetNormal(n00, glm::dvec2(0, fixedsize), static_cast<double>(R));
		glm::dvec3 n11 = utils::spatial::OffsetNormal(n00, glm::dvec2(fixedsize, fixedsize), static_cast<double>(R));
		glm::dvec3 n10 = utils::spatial::OffsetNormal(n00, glm::dvec2(fixedsize, 0), static_cast<double>(R));

		int fy = static_cast<int>(chunkPos.y) & ~mask;
		int scale = fixedsize / core::config::CHUNK_SIZE;

		int size = chunk.Size();
		for (int xi = 0; xi < size; xi += scale) {
			//计算u插值
			double u = static_cast<double>(xi) / size;
			glm::dvec3 c0 = glm::normalize(glm::mix(n00, n10, u));
			glm::dvec3 c1 = glm::normalize(glm::mix(n01, n11, u));
			for(int zi = 0;zi < size;zi += scale){
				double v = static_cast<double>(zi) / size;
				glm::dvec3 c = glm::normalize(glm::mix(c0, c1, v));

				int height = GetHeightNormal(c.x, c.y, c.z);
				for (int yi = 0; yi < size;yi += scale) {
					if(fy + yi > height)
						chunk.SetVoxel(xi, yi, zi, 0);
					else if(fy + yi == height)
						chunk.SetVoxel(xi, yi, zi, 1);
					else if(fy + yi >= height - 3)
						chunk.SetVoxel(xi, yi, zi, 2);
					else
						chunk.SetVoxel(xi, yi, zi, 3);
				}

			}
		}
		
	}
}