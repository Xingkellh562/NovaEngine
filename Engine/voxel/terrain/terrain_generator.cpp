#include "terrain_generator.h"
#include <random>
#include <algorithm>
#include <core/config/config.h>


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
		int fixedsize = chunkSize & ~(core::config::CHUNK_SIZE - 1);
		int MASK = fixedsize - 1;
		int scale = fixedsize / core::config::CHUNK_SIZE;

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
					else if (fy + yi * scale >= height - 3 * scale)
						chunk.SetVoxel(xi, yi, zi, 2);//填充为泥土
					else 
						chunk.SetVoxel(xi, yi, zi, 3);//填充为石
				}
			}
		}
	}

	lod::LodVoxelData TerrainGenerator::SampleLodData(int x, int y, int z, int lodSize) const {
		int size = core::config::CHUNK_SIZE;
		int fixedsize = lodSize & ~(size - 1);
		int MASK = fixedsize - 1;
		int scale = fixedsize / size;

		//区块原点
		int fx = x & ~MASK, fy = y & ~MASK, fz = z & ~MASK;

		int sampleSize = (size << 1) + 2;

		lod::LodVoxelData data(size);
		
		//填充高度图(高一级精度)
		std::vector<int> heightMap(sampleSize * sampleSize);
		for (int xi = 0;xi < sampleSize;xi++) {
			for (int zi = 0;zi < sampleSize;zi++) {
				heightMap[xi * sampleSize + zi] = GetHeight(fx + xi * scale / 2, fz + zi * scale / 2);
			}
		}

		//填充内部体素
		for (int xi = 0;xi < size;xi++) {
			for (int zi = 0;zi < size;zi++) {
				int idx = (xi * 2 + 1) * sampleSize + (zi * 2 + 1);
				int height = heightMap[idx];

				for (int yi = 0; yi < size;yi++) {
					int voxelIdx = xi * size * size + yi * size + zi;
					//简单地形
					
					if (height >= fy + yi * scale && height < fy + (yi + 1) * scale)
						data.datas_[voxelIdx] = 1;//填充为草方块
					else if (fy + yi * scale > height)
						data.datas_[voxelIdx] = 0;//填充为空气
					else if (fy + yi * scale >= height - 3 * scale)
						data.datas_[voxelIdx] = 2;//填充为泥土
					else
						data.datas_[voxelIdx] = 3;//填充为石
				}
			}
		}

		//填充边界体素
		//左 右 上 下 前 后
		static const std::array<std::array<int, 3>, 6> neighbours_ = {
			{{-1,0,0},{1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}
		};

		static const std::array<std::array<std::array<int, 3>, 4>, 6> borderOffset = {{
				{{{0,0,0},{0,0,1},{0,1,1},{0,1,0}}}, 
				{{{0,0,0},{0,0,1},{0,1,1},{0,1,0}}},
				{{{0,0,0},{0,0,1},{1,0,1},{1,0,0}}},
				{{{0,0,0},{0,0,1},{1,0,1},{1,0,0}}},
				{{{0,0,0},{0,1,0},{1,1,0},{1,0,0}}},
				{{{0,0,0},{0,1,0},{1,1,0},{1,0,0}}}
			}};

		for (int dir = 0;dir < 6;dir++) {
			int dirX = neighbours_[dir][0];
			int dirY = neighbours_[dir][1];
			int dirZ = neighbours_[dir][2];

			bool freeX = dirX == 0;
			bool freeY = dirY == 0;
			bool freeZ = dirZ == 0;

			for (int xi = 0;xi < size;xi++) {
				int trueX;
				if (!freeX) trueX = dirX == 1 ? size : 0;
				else trueX = xi; //坐标对齐

				for (int yi = 0;yi < size;yi++) {
					int trueY;
					if (!freeY) trueY = dirY == 1 ? size : 0;
					else trueY = yi;

					for (int zi = 0;zi < size;zi++) {
						int trueZ;
						if (!freeZ) trueZ = dirZ == 1 ? size : 0;
						else trueZ = zi;

						//获取体素状态
						bool solid = true;
						for (int off = 0;off < 4;off++) {
							//sampleSize == size * 2 + 2
							//trueX/Y/Z == size => size * 2 + 1 => sampleSize - 1
							int offX = borderOffset[dir][off][0] + ((trueX || freeX) ? trueX * 2 + 1 : 0);
							int offY = borderOffset[dir][off][1] + ((trueY || freeY) ? trueY * 2 + 1 : 0);
							int offZ = borderOffset[dir][off][2] + ((trueZ || freeZ) ? trueZ * 2 + 1 : 0);

							//从高度图中获取高度，高度图大小(sampleSize * sampleSize)
							int height = heightMap[offX * sampleSize + offZ];

							solid = (trueY * scale + fy) <= height - 2 * scale;
							if (!solid) break;
						}

						//填充边界面(大小size * size)
						//x -> y -> z
						if (dirX != 0)
							data.neighbours_[dir][trueY * size + trueZ] = solid;
						else if (dirY != 0)
							data.neighbours_[dir][trueX * size + trueZ] = solid;
						else if (dirZ != 0)
							data.neighbours_[dir][trueX * size + trueY] = solid;

						if (!freeZ) break;
					}
					if (!freeY) break;
				}
				if (!freeX) break;
			}
		}
		return data;
	}

}