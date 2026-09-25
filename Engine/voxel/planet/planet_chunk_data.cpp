#include "planet_chunk_data.h"
#include <algorithm>

namespace nova_engine::voxel::planet {
	PlanetChunkData::PlanetChunkData(size_t size) :planetSize_(size)
	,chunkCount_(0),poolLength_(0){
		chunks_.reserve(6 * size * size);
		for (int i = 0;i < 6 * size * size;i++) {
			chunks_[i] = {};
		}
	}

	PlanetChunkData::~PlanetChunkData() {
		chunks_.clear();
	}

	void PlanetChunkData::SubmitChunk(int x, int y, int z, std::unique_ptr<chunk::Chunk>) {
		size_t idx = MapPlanetPosToAddress(x,y,z);


	}

	//void PlanetChunkData::RemoveChunk(int x, int y, int z) {

	//}

	//const chunk::Chunk* PlanetChunkData::FindChunk(int x, int y, int z) const {

	//}

	void PlanetChunkData::Clear() {
		//chunks_.clear();
	}

	//const std::array<chunk::Chunk*, 6> PlanetChunkData::GetChunkNeighbours(int x, int y, int z) const {

	//}
	////从对象池中获取一个块，如果对象池中没有，则创建一个新的块
	std::unique_ptr<chunk::Chunk> PlanetChunkData::AcquireChunk() {
		return pool_.Acquire(32);
	}

	//将球面绝对弧长坐标（单位：米）转换为经纬度
	glm::vec2 PlanetChunkData::TransToLonLatCoo(int x, int z) const {
		constexpr float Pi = 3.1415926535f;
		constexpr float HalfPi = Pi * 0.5f;
		constexpr float Eps = 1e-6f;

		// 半径：显式、可解释
		const float r = 0.8660254f * (planetSize_ * 2048.0f);

		// z 是沿经线的弧长
		float lat = z / r;
		if (lat < -HalfPi - Eps || lat > HalfPi + Eps)
			return { -1.0f, -1.0f };
		lat = std::clamp(lat, -HalfPi, HalfPi);

		// 极点：经度未定义，约定返回 0
		float cosLat = std::cos(lat);
		float lon = 0.0f;
		if (std::abs(cosLat) > Eps) {
			lon = x / (r * cosLat);
		}
		else {
			return { 0.0f, lat };  // 或返回 {-1,-1}，看你约定
		}

		if (std::abs(lon) > Pi + Eps)
			return { -1.0f, -1.0f };

		return { lon, lat };  // 弧度
	}

	glm::ivec3 PlanetChunkData::PlanetCooTrans(float x, float y) const {
		const float Pi = 3.1415926535f;
		float lon = x * Pi / 180.0f, lat = y * Pi / 180.0f;

		int face = 0, u = 0, v = 0;

		if (x > 180.0f || x < -180.0f || y > 90.0f || y < -90.0f)
			return { face,u,v };

		glm::vec3 decareCoo = {
			std::sin(lon) * std::cos(lat),
			std::sin(lat),
			std::cos(lon) * std::cos(lat)
		};

		float s = 0, t = 0;
		float zdx = std::abs(decareCoo.x), zdy = std::abs(decareCoo.y), zdz = std::abs(decareCoo.z);
		if (zdx >= zdy && zdx >= zdz) {
			if (decareCoo.x > 0){
				face = 0;
				s = -decareCoo.z / zdx;
				t = -decareCoo.y / zdx;
			}
			else {
				face = 1;
				s =  decareCoo.z / zdx;
				t = -decareCoo.y / zdx;
			}
		}
		else if (zdy >= zdx && zdy >= zdz) {
			if (decareCoo.y > 0) {
				face = 2;
				s = decareCoo.x / zdy;
				t = decareCoo.z / zdy;
			}
			else {
				face = 3;
				s =  decareCoo.x / zdy;
				t = -decareCoo.z / zdy;
			}
		}
		else if (zdz >= zdx && zdz >= zdy) {
			if (decareCoo.z > 0) {
				face = 4;
				s = decareCoo.x / zdz;
				t = -decareCoo.y / zdz;
			}
			else {
				face = 5;
				s = -decareCoo.x / zdz;
				t = -decareCoo.y / zdz;
			}
		}
	
		float uNorm = (s + 1) / 2.0f, vNorm = (t + 1) / 2.0f;
		
		u = std::min(static_cast<size_t>(floor(uNorm * planetSize_)), planetSize_ - 1);
		v = std::min(static_cast<size_t>(floor(vNorm * planetSize_)), planetSize_ - 1);

		return{face,u,v};
	}
}