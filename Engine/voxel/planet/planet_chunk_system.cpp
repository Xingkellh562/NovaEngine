#include "planet_chunk_system.h"
#include <utils/spatial/chunk_range_finder.h>
#include <glm/gtx/norm.hpp>
#include <voxel/terrain/terrain_generator.h>
#include <voxel/terrain/planet_terrain_generator.h>
#include <core/config/config.h>
#include <utils/spatial/spherical_transform.h>

namespace nova_engine::voxel::planet {
	void PlanetChunkSystem::BuildChunk(PlanetChunkData& chunkData, int x, int y, int z) {
		static const terrain::PlanetTerrainGenerator generator(123);

		const int MASK = core::config::CHUNK_SIZE - 1;
		//将转入的绝对坐标修正为区块原点(左下后)(区块大小32)，可处理负数
		int fx = x & ~MASK, fy = y & ~MASK, fz = z & ~MASK;
		//如果任务存在则取消提交
		if (existTasks_.find({ fx,fy,fz }) != existTasks_.end()) return;

		//从池中获取一个可供修改的对象
		
		//提交任务
		try {
			auto cancel = std::make_shared<core::concurrency::CancelToken>();

			std::unique_ptr<chunk::Chunk<float>> chunkPtr = chunkData.AcquireChunk();
			if (!chunkPtr) return;

			existTasks_.insert({ fx,fy,fz });

			bool success = generatorWorker_.TrySubmitTask(core::concurrency::Task<ChunkResult>{
				.id = 0, .priority = 0, .cancel = cancel,
					.fn = [chunk = std::move(chunkPtr), fx, fy, fz](core::concurrency::TaskContext<ChunkResult>& ctx) mutable {
					ChunkResult result;
					result.x = fx;
					result.y = fy;
					result.z = fz;

					if (chunk) {
						generator.BuildChunk(*chunk, { fx, fy, fz }, { 0,1024,{0,0,0} });
					}

					result.chunk = std::move(chunk);
					ctx.emit(std::move(result));
					}
			});

			if (!success) {
				existTasks_.erase({ fx,fy,fz });
				chunkData.RecycleChunk(std::move(chunkPtr));
			}
		}
		catch (const std::exception& e) {
			NE_CORE_ERROR("ChunkSystem", "Failed to submit chunk build task, catch error:{}", e.what());
		}
	}


	void PlanetChunkSystem::Update(Planet& planet,glm::vec3 playerPos) {
		static const std::vector<glm::ivec3> chunksPos = utils::spatial::FindChunkInRange(128);

		for (const glm::ivec3& chunkPos : chunksPos) {
			int cx = chunkPos.x + playerPos.x,
				cy = chunkPos.y + playerPos.y,
				cz = chunkPos.z + playerPos.z;

			auto chunk = planet.chunkData.FindChunk(cx, cy, cz);
			const glm::ivec3& cp = chunkPos;
			//((cp.x * cp.x + cp.y * cp.y + cp.z * cp.z) < 160 * 160) &&
			if (!chunk) {
				//这里改成发布区块构建任务
				BuildChunk(planet.chunkData, cx, cy, cz);
			}
			//else if (((cp.x * cp.x + cp.y * cp.y + cp.z * cp.z) >= 192 * 192) && chunk)
			//	planet.chunkData.RemoveChunk(cx, cy, cz);
		}

		std::vector<glm::ivec3> needRemove{};
		for (const auto& [pos,chunk] : planet.chunkData) {
			if (chunk && (glm::length(static_cast<glm::vec3>(pos << nova_engine::core::config::CHUNK_BIT1) - playerPos)) > 128 + 32) {
				needRemove.push_back(pos << nova_engine::core::config::CHUNK_BIT1);
			}
		}
		for (const auto& pos : needRemove) {
			planet.chunkData.RemoveChunk(pos.x, pos.y, pos.z);
		}

		core::concurrency::Result<ChunkResult> out;
		while (generatorWorker_.TryPopResult(out)) {
			planet.chunkData.SubmitChunk(out.value.x, out.value.y, out.value.z, std::move(out.value.chunk));
			existTasks_.erase({ out.value.x, out.value.y, out.value.z });
		}
	}
}