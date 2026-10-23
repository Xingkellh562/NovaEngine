#include "ui_pass.h"

#include "../render_resources.h"

#include <render/core/transform.h>
#include <render/device/command_list.h>
#include <render/scene//render_view.h>

namespace nova_engine::render::framework {
	void UIPass::Record(const scene::RenderView& view,
		const RenderResources& res,
		device::ICommandList& cmd) {
		const int vw = static_cast<int>(view.viewportWidth);
		const int vh = static_cast<int>(view.viewportHeight);

		static core::Camera uiCamera{};

		device::ClearValue clear{};
		cmd.BeginRenderTarget(res.backBuffer,
			core::LoadOp::DontCare, core::LoadOp::DontCare, clear);

		cmd.SetViewport({ 0, 0, vw, vh });
		cmd.SetPipelineState(res.uiPipeline);

		cmd.SetShaderParam("projection", uiCamera.GetOrthoMatrix(
			static_cast<int>(view.viewportWidth),
			static_cast<int>(view.viewportHeight)
		));

		if (view.uiMesh) {
			auto& m = view.uiMesh->GetMeshData();
			
			cmd.DrawMesh(m.mesh, core::MakeModelMatrix(m.position, m.rotation, glm::vec3(0.0f)));
		}

		cmd.EndRenderTarget();
	}
}