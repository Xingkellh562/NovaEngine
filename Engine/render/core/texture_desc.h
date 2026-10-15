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
	};

	struct TextureArrayLayer {
		std::string name;
		std::string path;
		int index = 0;
	};

	struct TextureArrayDesc : TextureDesc
	{
		uint32_t tileSize = 0; //单层边长，0=由第一张图决定
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
	};
}