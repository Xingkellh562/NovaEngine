#pragma once

#include "../render_pass.h"
#include <vector>

namespace nova_engine::render::scene {
	struct MeshInstance;
}

namespace nova_engine::render::framework {
	class TransparentPass final : public RenderPass {
	public:
		std::string_view Name() const override { return "TransparentPass"; }

		bool Enabled(const scene::RenderView&) const override;

		void Record(const scene::RenderView&,
			const RenderResources&,
			device::ICommandList&) override;
	private:
		mutable std::vector<const scene::MeshInstance*> sorted_;
	};
}