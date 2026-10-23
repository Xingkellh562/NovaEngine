#pragma once

#include "../render_pass.h"

namespace nova_engine::render::framework {

	class UIPass final : public RenderPass {
	public:
		std::string_view Name() const override { return "UIPass"; }
		void Record(const scene::RenderView&, const RenderResources&,
			device::ICommandList&) override;
	};

}