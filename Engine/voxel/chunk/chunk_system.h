#pragma once

#include <core/world/world.h>
#include <core/concurrency/worker.h>
#include <unordered_set>

namespace nova_engine::voxel::chunk {
	
	struct ChunkResult {
		int x, y, z;
		std::unique_ptr<Chunk<int>> chunk;
	};

	

	//ChunkSystem是一个管理区块数据的系统，负责区块的加载、卸载、更新等操作
	//但它不持有任何区块数据与状态，所有的区块数据与状态都应该存储在World中，ChunkSystem只负责操作World中的区块数据
	class ChunkSystem {
	public:
		static ChunkSystem& GetInstance() {
			static ChunkSystem instance;
			
			return instance;
		}
		
		~ChunkSystem() {
			generatorWorker_.Stop();
		}

		ChunkSystem(const ChunkSystem&) = delete;
		ChunkSystem& operator=(const ChunkSystem&) = delete;

		void Update(nova_engine::core::world::World& world);
	private:
		core::concurrency::Worker<ChunkResult> generatorWorker_{"chunkGeneratorWorker",512,512};
		std::unordered_set<glm::ivec3> existTasks_;
		//往区块中填入体素数据，需要的区块对象从chunkData获取
		//to do:如果将来拥有地形生成器，那么这里将调用地形生成器的方法来填充地形
		void BuildChunk(ChunkData& chunkData, int x, int y, int z);
		//暂时先包含这一个函数用于测试
		ChunkSystem() {
			generatorWorker_.Start();
		}
	};

}