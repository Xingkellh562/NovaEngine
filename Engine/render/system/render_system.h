#pragma once

#include <core/world/world.h>
#include <render/opengl/context/render_context.h>

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
	};
}