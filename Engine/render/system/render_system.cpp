#include "render_system.h"
#include <render/utils/mesh_builder.h>
#include <utils/spatial/chunk_range_finder.h>
#include <glm/gtx/norm.hpp>
#include <log/logger.h>
#include <voxel/terrain/terrain_generator.h>
#include <render/utils/smooth_mesher.h>
#include <voxel/terrain/planet_terrain_generator.h>

namespace nova_engine::render::system {
	void RenderSystem::Init(device::IRenderDevice& device) {
		if (chunkMesh_) return;   // 幂等

		chunkMesh_ = std::make_unique<scene::ChunkMeshSet>(
			device, static_cast<uint32_t>(scene::MeshLayer::Count));
		lodMesh_ = std::make_unique<scene::LodMeshRegistry>(device);

		// LodSystem 不需要 device —— 它只产出 MeshData，
		// GPU 上传由 LodMeshRegistry 内部完成
		NE_CORE_INFO("RenderSystem", "Initialized on {}", device.Name());
	}

	void RenderSystem::Shutdown() {
		lodMesh_.reset();
		chunkMesh_.reset();
		// meshBuildWorker_ 由构造/析构管理，这里不动
	}

	// 逐行对应改动前 render_system.cpp:252-275
	void RenderSystem::UpdateShadowCamera(const nova_engine::core::world::World& world,
		const RenderSettings& settings) {
		const float orthoSize = settings.shadowOrthoSize;
		const int   shadowRes = static_cast<int>(settings.shadowResolution);
		const float texelWorldPos = (2.0f * orthoSize) / static_cast<float>(shadowRes);

		const glm::vec3 lightDir = glm::normalize(settings.lightDirection);
		const float     distance = settings.shadowDistance;

		const glm::vec3 lightPos = world.playerPos + lightDir * distance;
		const glm::mat4 lightView = glm::lookAt(lightPos, world.playerPos,
			glm::vec3(0.0f, 1.0f, 0.0f));

		// 纹素对齐，消除阴影抖动。与改动前一致：只对齐 x/y，不对齐光轴
		glm::vec4 p = lightView * glm::vec4(world.playerPos, 1.0f);
		p.x = glm::floor(p.x / texelWorldPos) * texelWorldPos;
		p.y = glm::floor(p.y / texelWorldPos) * texelWorldPos;

		const glm::vec3 alignedTarget = glm::vec3(glm::inverse(lightView) * p);

		shadowCamera_.position = alignedTarget + lightDir * distance;
		shadowCamera_.rotation = -lightDir;
	}

	static const std::vector<glm::ivec3> chunksPos = nova_engine::utils::spatial::FindChunkInRange(256);

	scene::RenderView RenderSystem::BuildView(nova_engine::core::world::World& world,
		const RenderSettings& settings) {
		static const voxel::terrain::PlanetTerrainGenerator generator(123);

		// ---- 1. 消费脏区域（取代 input 系统反向调用 RequireMeshUpdate）----
		scene::ChunkMeshRegistry* meshCache = chunkMesh_->Layer(scene::MeshLayer::Default);
		scene::ChunkMeshRegistry* meshCache2 = chunkMesh_->Layer(scene::MeshLayer::Lucency);
		if (!meshCache || !meshCache2) return view_;
		//meshCache->ConsumeDirty(world);

		// ---- 2. 远处的网格回收（逻辑与改动前 render_system.cpp:165-173 一致）----
		glm::ivec3 playerPos = world.playerPos;
		auto& meshs = const_cast<std::vector<scene::MeshInstance>&>(meshCache->GetAllMesh());
		for (const auto& mesh : meshs) {
			auto chunk = world.planet.chunkData.FindChunk(
				(int)mesh.position.x, (int)mesh.position.y, (int)mesh.position.z);
			//int dis = glm::length2(mesh.position - (glm::vec3)playerPos);
			//dis > 192 * 192 ||
			if (!chunk) {
				meshCache->FreeMesh((int)mesh.position.x, (int)mesh.position.y, (int)mesh.position.z);
				meshCache2->FreeMesh((int)mesh.position.x, (int)mesh.position.y, (int)mesh.position.z);
			}
		}

		

		// ---- 3. 提交网格构建任务（逻辑不变，仅类型替换）----
		for (const glm::ivec3& chunkPos : chunksPos) {
			int cx = chunkPos.x + ((int)playerPos.x & ~31),
				cy = chunkPos.y + ((int)playerPos.y & ~31),
				cz = chunkPos.z + ((int)playerPos.z & ~31);

			auto chunk = world.planet.chunkData.FindChunk(cx, cy, cz);
			bool exist = meshCache->Exist(cx, cy, cz);
			const glm::ivec3& cp = chunkPos;
			if (chunk && !exist) {
				//只要有邻居不存在，不构建网格
				bool neiExist = world.planet.chunkData.NeighboursExist(cx, cy, cz);
				if (!neiExist) continue;
				const std::array<nova_engine::voxel::chunk::Chunk<float>*, 7>& nei = world.planet.chunkData.GetChunkNeighbours(cx, cy, cz);

				//多线程异步提交
				SubmitMeshRequire(chunk,nei,cx,cy,cz);
			}
		}

		glm::ivec3 outChunk;
		while (world.planet.chunkData.GetDirtyChunks(outChunk)) {
			int cx = outChunk.x;
			int cy = outChunk.y;
			int cz = outChunk.z;

			auto chunk = world.planet.chunkData.FindChunk(cx, cy, cz);
			const std::array<nova_engine::voxel::chunk::Chunk<float>*, 7>& nei = world.planet.chunkData.GetChunkNeighbours(cx, cy, cz);

			if (chunk)
				SubmitMeshRequire(chunk, nei, cx, cy, cz);
		}

		// ---- 4. 回收构建结果并上传 ----
		nova_engine::core::concurrency::Result<MeshBuildResult> out;
		while (meshBuildWorker_.TryPopResult(out)) {
			const int cx = out.value.x, cy = out.value.y, cz = out.value.z;
			meshCache->SubmitMesh(cx, cy, cz, out.value.value);   // ← 内部走 IRenderDevice
			existTasks_.erase({ cx, cy, cz });
		}

		// ---- 5. 更新相机（逻辑与改动前 render_system.cpp:246-275 一致）----
		mainCamera_.position = world.playerPos;
		mainCamera_.rotation = world.playerRot;
		UpdateShadowCamera(world, settings);

		lodSystem.UpdateLod(world.playerPos, *meshCache, *lodMesh_, generator);

		// ---- 6. 产出渲染视图 ----
		view_.camera = &mainCamera_;
		view_.shadowCamera = &shadowCamera_;
		view_.settings = &settings;
		view_.chunkMesh = chunkMesh_.get();
		view_.lodMesh = settings.useLod ? lodMesh_.get() : nullptr;

		view_.mainOrigin = mainCamera_.position;
		view_.shadowOrigin = shadowCamera_.position;

		// 两个矩阵语义不同，分别对应改动前两条代码路径，见 3.5 节
		view_.shadowViewProj = shadowCamera_.GetOrthoMatrix(32.0f)
			* shadowCamera_.GetRelativeViewMatrix();
		view_.shadowSampleMatrix = shadowCamera_.GetOrthoMatrix(32.0f)
			* shadowCamera_.GetRelativeViewMatrix(mainCamera_.position);
		return view_;
	}

	void RenderSystem::SubmitMeshRequire(voxel::chunk::Chunk<float>* chunk,
		const std::array<nova_engine::voxel::chunk::Chunk<float>*, 7>& nei,
		int cx,int cy,int cz){
		if (existTasks_.find({ cx, cy, cz }) != existTasks_.end()) return;

		chunk->SetState(voxel::chunk::ChunkState::Meshing);
		for (int i = 0;i < nei.size();i++) {
			nei[i]->SetState(voxel::chunk::ChunkState::Meshing);
		}

		auto cancel = std::make_shared<nova_engine::core::concurrency::CancelToken>();
		existTasks_.insert({ cx, cy, cz });

		try {
			bool success = meshBuildWorker_.TrySubmitTask(nova_engine::core::concurrency::Task<MeshBuildResult>{
				.id = 0, .priority = 0, .cancel = cancel,
					.fn = [chunk, nei, cx, cy, cz](nova_engine::core::concurrency::TaskContext<MeshBuildResult>& ctx) mutable {
					MeshBuildResult result;
					result.x = cx;
					result.y = cy;
					result.z = cz;

					auto defaultMesh = utils::BuildSNMeshFromChunk(*chunk, nei);
					result.value = std::move(defaultMesh);
					ctx.emit(std::move(result));
					chunk->SetState(voxel::chunk::ChunkState::Complete);
					for (int i = 0;i < nei.size();i++) {
						nei[i]->SetState(voxel::chunk::ChunkState::Complete);
					}
					}
			});

			if (!success) existTasks_.erase({ cx,cy,cz });
		}
		catch (const std::exception& e) {
			NE_CORE_ERROR("RenderSystem", "Failed to submit chunk build task, catch error:{}", e.what());
		}
	}
}