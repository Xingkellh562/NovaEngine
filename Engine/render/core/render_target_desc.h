#pragma once

#include "handles.h"
#include "texture_desc.h"

#include <cstdint>
#include <string>
#include <vector>

namespace nova_engine::render::core {
	struct RenderTargetAttachment {
		std::string name;

		TextureFormat format = TextureFormat::RGBA8;
		bool isDepth = false;

		//颜色附件序号,深度附件忽略此字段
		uint32_t index = 0;

		//固定尺寸，任何一边为0表示该方向跟随窗口尺寸
		uint32_t fixedWidth = 0;
		uint32_t fixedHeight = 0;

		//非空表示复用已有纹理
		TextureHandle imported{};
	};

	struct RenderTargetDesc {
		std::vector<RenderTargetAttachment> attachments;
	};
}