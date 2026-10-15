#include "shadow_pass.h"

#include "../render_resources.h"

#include <render/core/transform.h>
#include <render/device/command_list.h>
#include <render/scene/render_view.h>

namespace nova_engine::render::framework {

	void ShadowPass::Record(const scene::RenderView& view,
		const RenderResources& res,
		device::ICommandList& cmd) {
		device::ClearValue clear{};
		clear.depth = 1.0f;

		cmd.BeginRenderTarget(res.shadowFrame,
			core::LoadOp::Clear, core::LoadOp::Clear, clear);
		cmd.SetViewport({ 0, 0,
						  static_cast<int>(res.shadowResolution),
						  static_cast<int>(res.shadowResolution) });

		if (view.settings->useShadow && view.chunkMesh != nullptr) {
			cmd.SetPipelineState(res.shadowPipeline);
			cmd.SetShaderParam("lightSpaceMatrix", view.shadowViewProj);

			// 只画 Default 层
			for (const scene::MeshInstance& m :
				(view.chunkMesh->Layer(scene::MeshLayer::Default)->GetAllMesh())) {
				if (!m.awake) continue;
				cmd.DrawMesh(m.mesh,
					core::MakeModelMatrix(m.position, m.rotation, view.shadowOrigin));
			}
		}

		cmd.EndRenderTarget();
	}

}