#include "transparent_pass.h"

#include "../render_resources.h"

#include <render/core/transform.h>
#include <render/device/command_list.h>
#include <render/scene/render_view.h>

#include <glm/gtx/norm.hpp>

#include <algorithm>
#include <log/logger.h>

namespace nova_engine::render::framework {
	bool TransparentPass::Enabled(const scene::RenderView& view) const {
		if (!view.settings->useLucency || view.chunkMesh == nullptr) return false;
		const auto* lucency = view.chunkMesh->Layer(scene::MeshLayer::Lucency);
		if (lucency == nullptr || lucency->MeshCount() == 0) {
			static bool warned = false;
			if (!warned) { NE_CORE_WARN("TransparentPass", "Lucency layer has no meshes"); warned = true; }
			return false;
		}
		return true;
	}

	void TransparentPass::Record(const scene::RenderView& view,
		const RenderResources& res,
		device::ICommandList& cmd) {
		const scene::ChunkMeshRegistry* lucency =
			view.chunkMesh->Layer(scene::MeshLayer::Lucency);
		if (lucency == nullptr) return;

		// ---------- 收集 + 排序（对应 render_system.cpp:117-129）----------
		sorted_.clear();
		for (const scene::MeshInstance& m : lucency->GetAllMesh()) {
			if (m.awake) sorted_.push_back(&m);
		}
		std::sort(sorted_.begin(), sorted_.end(),
			[](const scene::MeshInstance* a, const scene::MeshInstance* b) {
				return glm::length2(a->position) > glm::length2(b->position);
			});

		{
			device::ClearValue clear{};
			clear.color[0][3] = 0.0f;      // accum = 0
			clear.color[1][0] = 1.0f;      // reveal = 1
			cmd.BeginRenderTarget(res.oitFrame, core::LoadOp::Clear, core::LoadOp::DontCare, clear);
			cmd.EndRenderTarget();
		}

		if (sorted_.empty()) return;

		const int vw = static_cast<int>(view.viewportWidth);
		const int vh = static_cast<int>(view.viewportHeight);
		const glm::mat4 viewMatrix = view.camera->GetRelativeViewMatrix();
		const glm::mat4 projection = view.camera->GetTransformMatrix(vw, vh);

		// ================= 阶段 A：深度预通道 =================
		// 对应 semi_transparent_renderer.cpp:47-79
		{
			device::ClearValue clear{};
			cmd.BeginRenderTarget(res.mainFrame,
				core::LoadOp::Load, core::LoadOp::Load, clear);
			cmd.SetViewport({ 0, 0, vw, vh });
			cmd.SetPipelineState(res.wboitDepthPipeline);

			cmd.SetShaderParam("uProj", projection);
			cmd.SetShaderParam("uView", viewMatrix);
			cmd.BindTexture(0, "uTextureArray", res.blockTextures);

			for (const scene::MeshInstance* m : sorted_) {
				cmd.DrawMesh(m->mesh,
					core::MakeModelMatrix(m->position, m->rotation, view.mainOrigin));
			}
			cmd.EndRenderTarget();
		}

		// ================= 阶段 B：累加 =================
		// 对应 semi_transparent_renderer.cpp:84-111
		{
			device::ClearValue clear{};
			clear.color[0][0] = 0.0f; clear.color[0][1] = 0.0f;   // accum 清 0
			clear.color[0][2] = 0.0f; clear.color[0][3] = 0.0f;
			clear.color[1][0] = 1.0f; clear.color[1][1] = 0.0f;   // reveal 清 1
			clear.color[1][2] = 0.0f; clear.color[1][3] = 0.0f;

			// 深度来自阶段 A 写入的 MainFrame 深度附件（OITFrame 复用了它），
			// 所以 depthLoad = Load
			cmd.BeginRenderTarget(res.oitFrame,
				core::LoadOp::Clear, core::LoadOp::Load, clear);
			cmd.SetViewport({ 0, 0, vw, vh });
			cmd.SetPipelineState(res.wboitAccumPipeline);

			cmd.SetShaderParam("uProj", projection);
			cmd.SetShaderParam("uView", viewMatrix);
			cmd.BindTexture(0, "uTextureArray", res.blockTextures);

			for (const scene::MeshInstance* m : sorted_) {
				cmd.DrawMesh(m->mesh,
					core::MakeModelMatrix(m->position, m->rotation, view.mainOrigin));
			}
			cmd.EndRenderTarget();
		}
	}
}