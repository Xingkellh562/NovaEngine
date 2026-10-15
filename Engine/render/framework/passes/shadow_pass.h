#pragma once

#include <render/framework/render_pass.h>

namespace nova_engine::render::framework {

	class ShadowPass final : public RenderPass {
	public:
		std::string_view Name() const override { return "ShadowPass"; }
		// 始终执行：清屏是必须的
		void Record(const scene::RenderView&, const RenderResources&,
			device::ICommandList&) override;
	};

}