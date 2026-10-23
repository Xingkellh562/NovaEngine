#include "frame_renderer.h"

#include <render/scene/render_view.h>
#include <log/logger.h>

namespace nova_engine::render::framework {

	void FrameRenderer::Execute(const scene::RenderView& view,
		const RenderResources& resources,
		device::ICommandList& cmd) {
		if (!view.IsValid()) {
			NE_CORE_WARN("FrameRenderer", "RenderView not populated (settings={}, camera={}, chunkMesh={}); skip frame",
				(const void*)view.settings, (const void*)view.camera, (const void*)view.chunkMesh);
			return;                       // 默认构造的 RenderView 会崩在 0x1（settings->useLucency 偏移 1）
		}

		shadowPass_.Record(view, resources, cmd);

		opaquePass_.Record(view, resources, cmd);
		lightPass_.Record(view, resources, cmd);

		if (transparentPass_.Enabled(view)) {
			transparentPass_.Record(view, resources, cmd);
		}

		postProcessPass_.Record(view, resources, cmd);

		uiPass_.Record(view, resources, cmd);
	}

}