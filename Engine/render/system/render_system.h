#pragma once

#include <core/world/world.h>
#include <render/core/camera.h>
#include <render/core/render_settings.h>
#include <render/scene/chunk_mesh_registry.h>
#include <render/scene/lod_mesh_registry.h>
#include <render/scene/ui_mesh_registry.h>
#include <render/scene/render_view.h>
#include "chunk_lod_system.h"

#include <core/concurrency/worker.h>

#include <memory>
#include <unordered_set>

namespace nova_engine::render::device { class IRenderDevice; }

namespace nova_engine::render::system {

	struct MeshBuildResult {
		int x = 0, y = 0, z = 0;
		core::MeshData value;
	};

	class RenderSystem {
	public:
		static RenderSystem& GetInstance() {
			static RenderSystem instance;
			return instance;
		}

		// 构造不再需要 GL 上下文。设备由外部注入（Window 是组装点）。
		void Init(device::IRenderDevice& device);
		void Shutdown();

		~RenderSystem() { meshBuildWorker_.Stop(); }

		// ← 取代原来的 Update(world, context) + Render(context)
		// 只做「逻辑更新 + 产出渲染视图」，一个 gl* 都没有。
		scene::RenderView BuildView(nova_engine::core::world::World& world,
			const RenderSettings& settings);

		//const LodSystem& GetLodSystem() const { return lodSystem_; }

		const scene::ChunkMeshSet& GetChunkMesh() const { return *chunkMesh_; }

		const size_t GetMeshRequireCount() const {
			return meshBuildWorker_.GetSubmitTaskCount();
		}

	private:
		nova_engine::core::concurrency::Worker<MeshBuildResult> meshBuildWorker_{ "MeshBuildWorker", 512, 512 };
		std::unordered_set<glm::ivec3> existTasks_;

		std::unique_ptr<scene::ChunkMeshSet>     chunkMesh_;
		std::unique_ptr<scene::LodMeshRegistry>  lodMesh_;
		std::unique_ptr<scene::UIMeshRegistry>   uiMesh_;

		ChunkLodSystem lodSystem = ChunkLodSystem(3);

		// 取代改动前 CameraLibrary 的 "MainCamera" / "ShadowCamera" 两个字符串键条目
		core::Camera mainCamera_;
		core::Camera shadowCamera_;

		scene::RenderView view_{};

		RenderSystem() { meshBuildWorker_.Start(); }

		void SubmitMeshRequire(voxel::chunk::Chunk<float>*,
			const std::array<nova_engine::voxel::chunk::Chunk<float>*, 7>& nei,
			int cx, int cy, int cz);

		void UpdateShadowCamera(const nova_engine::core::world::World& world,
			const RenderSettings& settings);
	};
}