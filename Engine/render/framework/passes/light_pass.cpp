#include "light_pass.h"

#include "../render_resources.h"

#include <render/core/transform.h>
#include <render/device/command_list.h>
#include <render/scene//render_view.h>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/mat3x3.hpp>

namespace nova_engine::render::framework {
	void LightPass::Record(const scene::RenderView& view,
		const RenderResources& res,
		device::ICommandList& cmd) {
		device::ClearValue clear{};
		clear.color[0][0] = 0.0f;
		clear.color[0][1] = 0.0f;
		clear.color[0][2] = 0.0f;
		clear.color[0][3] = 1.0f;
		clear.depth = 1.0f;

		cmd.BeginRenderTarget(res.lightFrame, core::LoadOp::Clear, core::LoadOp::Clear, clear);

		cmd.SetViewport({ 0,0,
			static_cast<int>(view.viewportWidth),
			static_cast<int>(view.viewportHeight) });

		cmd.SetPipelineState(res.lightPipeline);

		cmd.SetShaderParam("view", view.camera->GetRelativeViewMatrix());
		cmd.SetShaderParam("projection", view.camera->GetTransformMatrix(
			static_cast<int>(view.viewportWidth),
			static_cast<int>(view.viewportHeight)
		));
		if (view.settings)
			cmd.SetShaderParam("lightDir", view.settings->lightDirection);

		cmd.BindTexture(0, "uTextureArray", res.blockTextures);

		auto drawOne = [&](const scene::MeshInstance& m) {
			const glm::mat4 model = core::MakeModelMatrix(m.position, m.rotation, view.mainOrigin);
			cmd.SetShaderParam("model", model);
			cmd.SetShaderParam("normalMatrix", glm::transpose(glm::inverse(glm::mat3(model))));
			cmd.DrawMesh(m.mesh, model);
		};

		if (view.chunkMesh->Layer(scene::MeshLayer::Default)) {
			for (const auto& m : view.chunkMesh->Layer(scene::MeshLayer::Default)->GetAllMesh()) {
				if (m.awake) drawOne(m);
			}
		}

		cmd.SetPipelineState(res.lodLightPipeline);

		cmd.SetShaderParam("view", view.camera->GetRelativeViewMatrix());
		
		cmd.SetShaderParam("projection", view.camera->GetTransformMatrix(
			static_cast<int>(view.viewportWidth),
			static_cast<int>(view.viewportHeight)
		));

		cmd.BindTexture(0, "uTextureArray", res.blockTextures);
		if (view.settings) {
			cmd.SetShaderParam("distance", view.settings->lodSwitchDistance);
			cmd.SetShaderParam("lightDir", view.settings->lightDirection);
			if (view.settings->useLod && view.lodMesh != nullptr) {
				for (const auto& m : view.lodMesh->AllData()) {
					if (m.awake) drawOne(m);
				}
			}
		}

		cmd.EndRenderTarget();
	}
}