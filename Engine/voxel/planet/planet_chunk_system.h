#pragma once

#include <core/world/world.h>
#include "planet.h"
#include <core/concurrency/worker.h>
#include <unordered_set>

namespace nova_engine::voxel::planet {
	struct ChunkResult {
		int x, y, z;
		std::unique_ptr<chunk::Chunk<float>> chunk;
	};

	//PlanetChunkSystem是一个管理区块数据的系统，负责星球区块的加载、卸载、更新等操作
	//但它不持有任何区块数据与状态，所有的区块数据与状态都应该存储在World中，ChunkSystem只负责操作World中的区块数据
	class PlanetChunkSystem {
	public:
		static PlanetChunkSystem& GetInstance() {
			static PlanetChunkSystem instance;
			return instance;
		}

		~PlanetChunkSystem() {
			generatorWorker_.Stop();
		}

		PlanetChunkSystem(const PlanetChunkSystem&) = delete;
		PlanetChunkSystem& operator=(const PlanetChunkSystem&) = delete;

		void Update(Planet& planet, glm::vec3);
	private:
		core::concurrency::Worker<ChunkResult> generatorWorker_{ "chunkGeneratorWorker",512,512 };
		std::unordered_set<glm::ivec3> existTasks_;
		/*往区块中填入体素数据，需要的区块对象从chunkData获取
		to do:如果将来拥有地形生成器，那么这里将调用地形生成器的方法来填充地形*/
		void BuildChunk(PlanetChunkData& chunkData, int x, int y, int z);
		//暂时先包含这一个函数用于测试
		PlanetChunkSystem() {
			generatorWorker_.Start();
		}
	};
}