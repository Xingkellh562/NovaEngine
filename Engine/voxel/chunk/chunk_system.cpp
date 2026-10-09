#include "chunk_system.h"
#include <utils/spatial/chunk_range_finder.h>
#include <glm/gtx/norm.hpp>
#include <voxel/terrain/terrain_generator.h>
#include <voxel/terrain/planet_terrain_generator.h>
#include <core/config/config.h>
#include <utils/spatial/spherical_transform.h>


namespace nova_engine::voxel::chunk {
	struct ChunkSubmitTask {
		int x, y, z;
		std::unique_ptr<Chunk> chunk;
	};

	void ChunkSystem::BuildChunk(ChunkData& chunkData, int x, int y, int z) {
		static const terrain::TerrainGenerator generator(123);
		static const terrain::PlanetTerrainGenerator planetGenerator(123);

		const int MASK = core::config::CHUNK_SIZE - 1;
		//将转入的绝对坐标修正为区块原点(左下后)(区块大小32)，可处理负数
		int fx = x & ~MASK, fy = y & ~MASK, fz = z & ~MASK;
		//如果任务存在则取消提交
		if (existTasks_.find({ fx,fy,fz }) != existTasks_.end()) return;

		//从池中获取一个可供修改的对象
		std::unique_ptr<Chunk> chunkPtr = chunkData.AcquireChunk();

		if (chunkPtr != nullptr) {
			//提交任务
			try {
				auto cancel = std::make_shared<core::concurrency::CancelToken>();

				ChunkSubmitTask task{
					.x = fx,.y = fy,.z = fz,
					.chunk = std::move(chunkPtr),
				};

				existTasks_.insert({ fx,fy,fz });

				bool success = generatorWorker_.TrySubmitTask(core::concurrency::Task<ChunkResult>{
					.id = 0,.priority = 0,.cancel = cancel,
					.fn = [task = std::move(task)](core::concurrency::TaskContext<ChunkResult>& ctx) mutable {
						ChunkResult result;
						result.x = task.x;
						result.y = task.y;
						result.z = task.z;

						if (task.chunk) {
							generator.BuildChunk(*task.chunk, task.x, task.y, task.z);
							/*planetGenerator.BuildChunkNormal(*task.chunk, playerData, playerPos, 
								{ static_cast<float>(task.x), 
								static_cast<float>(task.y), 
								static_cast<float>(task.z) }, 1024);*/
						}

						result.chunk = std::move(task.chunk);
						ctx.emit(std::move(result));
					}
				});

				if (!success) {
					existTasks_.erase({ fx,fy,fz });
				}
				//generator.BuildChunk(*chunkPtr, x, y, z, core::config::CHUNK_SIZE);
				//将对象提交给data容器管理
				//chunkData.SubmitChunk(fx, fy, fz, std::move(chunkPtr));
			}
			catch(const std::exception& e){
				NE_CORE_ERROR("ChunkSystem", "Failed to submit chunk build task, catch error:{}", e.what());
			}
		}
	}

	

	static const std::vector<glm::ivec3> chunksPos = utils::spatial::FindChunkInRange(256);

	

	void ChunkSystem::Update(nova_engine::core::world::World& world) {
		glm::ivec3 playerPos = world.playerPos;
		const int MAXCOUNT = 6;

		int count = 0;
		for (const glm::ivec3& chunkPos : chunksPos) {
			if (count >= MAXCOUNT) break;
			int cx = chunkPos.x + playerPos.x, 
				cy = chunkPos.y + playerPos.y, 
				cz = chunkPos.z + playerPos.z;
			auto chunk = world.chunkData.FindChunk(cx,cy,cz);
			const glm::ivec3& cp = chunkPos;
			if (((cp.x * cp.x + cp.y * cp.y + cp.z * cp.z) < 160 * 160) && !chunk) {
				//count++;
				//这里改成发布区块构建任务
				BuildChunk(world.chunkData, cx, cy, cz);
			}
			else if(((cp.x * cp.x + cp.y * cp.y + cp.z * cp.z) >= 192 * 192) && chunk)
				world.chunkData.RemoveChunk(cx, cy, cz);
		}

		core::concurrency::Result<ChunkResult> out;
		while (generatorWorker_.TryPopResult(out)) {
			world.chunkData.SubmitChunk(out.value.x, out.value.y, out.value.z, std::move(out.value.chunk));
			existTasks_.erase({ out.value.x, out.value.y, out.value.z });
		}
	}
}