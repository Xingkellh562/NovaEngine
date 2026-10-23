#pragma once

#include "passes/light_pass.h"
#include "passes/opaque_pass.h"
#include "passes/post_process_pass.h"
#include "passes/shadow_pass.h"
#include "passes/transparent_pass.h"
#include "passes/ui_pass.h"

namespace nova_engine::render::device { class ICommandList; }
namespace nova_engine::render::scene { struct RenderView; }

namespace nova_engine::render::framework {

	struct RenderResources;

	// 一帧的 Pass 编排。
	//
	// 刻意做成一个简单的有序列表而不是帧图：5 个 Pass、顺序固定、资源固定。
	// 若将来需要自动依赖排序 / 瞬态资源复用，把 Execute 换成帧图遍历即可，
	// 各 Pass 的 Record 实现不需要改动（它们不拥有渲染目标）。
	class FrameRenderer {
	public:
		void Execute(const scene::RenderView& view,
			const RenderResources& resources,
			device::ICommandList& cmd);

	private:
		ShadowPass      shadowPass_;
		OpaquePass      opaquePass_;
		LightPass       lightPass_;
		TransparentPass transparentPass_;
		PostProcessPass postProcessPass_;
		UIPass          uiPass_;
	};

}