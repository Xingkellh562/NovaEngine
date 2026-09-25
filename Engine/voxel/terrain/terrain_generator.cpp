#include "terrain_generator.h"
#include <random>
#include <algorithm>


namespace nova_engine::voxel::terrain {

	int randomInt(int min, int max) {
		if (min > max) std::swap(min, max);

		static std::mt19937 gen(std::random_device{}());
		std::uniform_int_distribution<int> dist(min, max);
		return dist(gen);
	}

	int TerrainGenerator::GetHeight(int x,int z) const {
		const int Range1 = 64, Range2 = 32, Range3 = 8;
		const int SeaLevel =-64;
		double mainHeight = noise_.Noise(x * 0.004,z * 0.004);
		double rollingHeight = noise_.Noise(x * 0.012, z * 0.012);
		double detailHeight = noise_.Noise(x * 0.04,z * 0.04);

		return static_cast<int>(
			(mainHeight * Range1 + 
			rollingHeight * Range2 + 
			detailHeight * Range3) + SeaLevel);
	}

	void TerrainGenerator::BuildChunk(chunk::Chunk& chunk, int x, int y, int z,int chunkSize)const {
		int fixedsize = chunkSize & ~31;
		int MASK = fixedsize - 1;
		int scale = fixedsize / 32;

		int fx = x & ~MASK,fy = y & ~MASK, fz = z & ~MASK;
		int size = chunk.Size();
		for (int xi = 0;xi < size;xi++) {
			for (int zi = 0;zi < size;zi++) {
				int height = GetHeight(fx + xi * scale,fz + zi * scale);

				for (int yi = 0; yi < size;yi++) {
					//简单地形
					if (fy + yi * scale > height)
						chunk.SetVoxel(xi, yi, zi, 0);//填充为空气
					else if (fy + yi * scale == height)
						chunk.SetVoxel(xi, yi, zi, 1);//填充为草方块
					else if (fy + yi * scale >= height - 3)
						chunk.SetVoxel(xi, yi, zi, 2);//填充为泥土
					else 
						chunk.SetVoxel(xi, yi, zi, 3);//填充为石
				}
			}
		}
	}
}