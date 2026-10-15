#include "post_process_pass.h"

#include "../render_resources.h"

#include <render/device/command_list.h>
#include <render/scene/render_view.h>

#include <glm/gtc/matrix_inverse.hpp>

namespace nova_engine::render::framework {

	void PostProcessPass::Record(const scene::RenderView& view,
		const RenderResources& res,
		device::ICommandList& cmd) {
		const int vw = static_cast<int>(view.viewportWidth);
		const int vh = static_cast<int>(view.viewportHeight);

		device::ClearValue clear{};
		cmd.BeginRenderTarget(res.backBuffer,
			core::LoadOp::Clear, core::LoadOp::DontCare, clear);
		cmd.SetViewport({ 0, 0, vw, vh });
		cmd.SetPipelineState(res.postProcessPipeline);

		// 对应 post_process_renderer.cpp:88-110 的六个纹理绑定
		cmd.BindTexture(1, "colorTex", res.mainColor);
		cmd.BindTexture(2, "depthTex", res.mainDepth);
		cmd.BindTexture(3, "shadowMap", res.shadowMap, &res.shadowSampler);
		cmd.BindTexture(4, "accumTex", res.accumTex);
		cmd.BindTexture(5, "revealTex", res.revealTex);
		cmd.BindTexture(6, "lightMap", res.lightTex);

		// 对应 post_process_renderer.cpp:54-60, 112-113
		const glm::mat4 viewMatrix = view.camera->GetRelativeViewMatrix();
		const glm::mat4 projection = view.camera->GetTransformMatrix(vw, vh);

		cmd.SetShaderParam("invViewProj", glm::inverse(projection * viewMatrix));
		cmd.SetShaderParam("lightSpaceMatrix", view.shadowSampleMatrix);

		// 对应 post_process_renderer.cpp:116-117 的 glDrawArrays(GL_TRIANGLES, 0, 6)
		cmd.DrawMesh(res.fullscreenQuad, glm::mat4(1.0f));

		cmd.EndRenderTarget();
	}

}