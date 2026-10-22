#include "opaque_pass.h"
#include "../render_resources.h"

#include <render/core/transform.h>
#include <render/device/command_list.h>
#include <render/scene/render_view.h>

namespace nova_engine::render::framework {
	void OpaquePass::Record(const scene::RenderView& view,
		const RenderResources& res,
		device::ICommandList& cmd) {
		device::ClearValue clear{};
		if (view.settings) {
			clear.color[0][0] = view.settings->clearColor.r;
			clear.color[0][1] = view.settings->clearColor.g;
			clear.color[0][2] = view.settings->clearColor.b;
			clear.color[0][3] = view.settings->clearColor.a;
			clear.depth = 1.0f;
		}
		
		cmd.BeginRenderTarget(res.mainFrame, core::LoadOp::Clear, core::LoadOp::Clear, clear);

		cmd.SetViewport({ 0,0,
			static_cast<int>(view.viewportWidth),
			static_cast<int>(view.viewportHeight) });

		if (view.settings->useLod && view.lodMesh != nullptr) {
			cmd.SetPipelineState(res.lodPipeline);

			cmd.SetShaderParam("view", view.camera->GetRelativeViewMatrix());
			cmd.SetShaderParam("projection", view.camera->GetTransformMatrix(
				static_cast<int>(view.viewportWidth),
				static_cast<int>(view.viewportHeight)
			));

			cmd.SetShaderParam("distance", view.settings->lodSwitchDistance);

			cmd.BindTexture(0, "uTextures", res.blockTextures);

			for (const auto& m : view.lodMesh->AllData()) {
				if (!m.awake) continue;
				cmd.DrawMesh(m.mesh, core::MakeModelMatrix(m.position, m.rotation, view.mainOrigin));
			}
		}
		cmd.SetPipelineState(res.opaquePipeline);

		cmd.SetShaderParam("view", view.camera->GetRelativeViewMatrix());
		cmd.SetShaderParam("projection", view.camera->GetTransformMatrix(
			static_cast<int>(view.viewportWidth),
			static_cast<int>(view.viewportHeight)
		));

		cmd.BindTexture(0, "uTextures", res.blockTextures);
		if (view.chunkMesh->Layer(scene::MeshLayer::Default)) {
			for (const scene::MeshInstance& m : view.chunkMesh->Layer(scene::MeshLayer::Default)->GetAllMesh()) {
				if (!m.awake) continue;
				cmd.DrawMesh(m.mesh, core::MakeModelMatrix(m.position, m.rotation, view.mainOrigin));
			}
		}
		cmd.EndRenderTarget();
	}
}