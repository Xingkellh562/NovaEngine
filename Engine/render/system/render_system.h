#pragma once

#include <core/world/world.h>
#include <render/opengl/context/render_context.h>
#include "lod_system.h"
#include <core/concurrency/worker.h>
#include <unordered_set>

namespace nova_engine::render::system {
	struct MeshBuildResult {
		int x = 0, y = 0, z = 0;
		std::array<std::optional<MeshData>, 2> value;
	};

	class RenderSystem {
	public:
		static RenderSystem& GetInstance() {
			static RenderSystem instance;
			return instance;
		}
		~RenderSystem() {
			meshBuildWorker_.Stop();
		}

		//渲染，使用渲染上下文
		void Render(opengl::context::RenderContext& context) const;
		//渲染相关的逻辑更新
		void Update(const nova_engine::core::world::World& world,opengl::context::RenderContext& context);

		const LodSystem& GetLodSystem() {
			return lodSystem_;
		}
	private:
		core::concurrency::Worker<MeshBuildResult> meshBuildWorker_{"MeshBuildWorker",512,512};
		std::unordered_set<glm::ivec3> existTasks_;

		RenderSystem() {
			meshBuildWorker_.Start();
		}

		void SubmitMeshRequire(voxel::chunk::Chunk*, 
			const std::array<nova_engine::voxel::chunk::Chunk*, 6>& nei,
			int cx, int cy, int cz);


		LodSystem lodSystem_;
	};
}