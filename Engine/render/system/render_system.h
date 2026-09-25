#pragma once

#include <core/world/world.h>
#include <render/opengl/context/render_context.h>
#include "lod_system.h"

namespace nova_engine::render::system {
	class RenderSystem {
	public:
		static RenderSystem& GetInstance() {
			static RenderSystem instance;
			return instance;
		}
		~RenderSystem() = default;
		//渲染，使用渲染上下文
		void Render(opengl::context::RenderContext& context) const;
		//渲染相关的逻辑更新
		void Update(const nova_engine::core::world::World& world,opengl::context::RenderContext& context);
	private:
		RenderSystem() = default;

		void UpdateLodMesh(opengl::context::RenderContext& renderContext, const voxel::chunk::ChunkData& chunkData, glm::vec3 playerPos);

		LodSystem lodSystem_;
	};
}