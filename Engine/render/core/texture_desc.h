#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "sampler_state.h"

namespace nova_engine::render::core {
	enum class TextureFormat : uint8_t {
		RGBA8,
		RGB16F,
		RGBA16F,
		R16F,
		Depth32F,
	};

	struct TextureDesc
	{
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t layers = 1;	//大于1表示2D纹理数组
		TextureFormat format = TextureFormat::RGBA8;

		//纹理自身的默认采样状态
		SamplerState defaultSampler{};

		bool generateMipmaps = false;

		bool depthCompareMode = false;
		float borderColor[4] = { 1.0f,1.0f,1.0f,1.0f };

		bool preferRgbaDecode = true;
	};

	struct TextureArrayLayer {
		std::string name;
		std::string path;
		int index = 0;
	};

	struct TextureArrayDesc : TextureDesc
	{
		//契约（由 assets::LoadTextureArray 保证）：
		//   加载器返回的 desc 里，tileSize 和 layers 一定已经被填好并校验过。
		uint32_t tileSize = 0;
		std::vector<TextureArrayLayer> sources;
	};

	struct TextureSpirit {
		std::string name;
		//像素偏移坐标，原点一般为左下角
		uint32_t u = 0;
		uint32_t v = 0;
		uint32_t width = 0;
		uint32_t height = 0;
	};

	struct TextureAtlasDesc : TextureDesc {
		std::vector<TextureSpirit> sources;

		//归一化 UV 时向内收缩的半像素数。
		//
		//   Linear 采样 + 缩小时，成员边界会和邻居混色（bleeding）。
		//   内缩半个纹素能显著缓解；有 padding 时配合 edge-extend 可以基本消除。
		//   设为 0 表示不内缩（Nearest 采样 + 1:1 显示时不需要）。
		float insetTexels = 0.5f;

		//这张图在 TextureSet 里的页号
		uint32_t pageIndex = 0;
	};
}