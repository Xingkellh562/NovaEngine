#pragma once

#include <render/core/texture_desc.h>
#include <render/core/texture_region.h>
#include <render/texture.h>          // render::TextureData / LoadTexture

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nova_engine::render::assets {

	// ---------------- 通用选项 ----------------
	struct TextureLoadOptions {
		core::TextureFormat format = core::TextureFormat::RGBA8;
		bool                generateMipmaps = false;

		core::SamplerState  sampler{
			core::TextureFilter::Linear,
			core::TextureFilter::Linear,
			core::TextureWrap::ClampToEdge
		};
	};

	// ---------------- 单张 ----------------
	struct TextureImageAsset {
		core::TextureDesc desc;      // width/height/format 已填好
		render::TextureData image;   // 已解码的像素

		TextureImageAsset() = default;

		bool Valid() const { return desc.width > 0 && desc.height > 0; }
	};

	// ---------------- 纹理数组 ----------------
	struct TextureArrayAsset {
		core::TextureArrayDesc           desc;    // tileSize / layers 已填好并校验
		std::vector<render::TextureData> layers;  // 与 desc.sources 一一对应

		TextureArrayAsset() = default;

		bool Valid() const {
			return desc.tileSize > 0 && !layers.empty()
				&& layers.size() == desc.sources.size();
		}
	};

	// ---------------- 图集 ----------------
	struct TextureAtlasAsset {
		std::vector<core::TextureAtlasDesc> descs;   // 每页一个 desc
		std::vector<render::TextureData>    pages;   // 与 descs 一一对应

		float insetTexels = 0.5f;

		TextureAtlasAsset() = default;

		bool Valid() const { return !pages.empty() && pages.size() == descs.size(); }
	};

	TextureImageAsset LoadTextureImage(std::string_view path,
		const TextureLoadOptions& opts = {});

	struct TextureArrayOptions : TextureLoadOptions {
		// 只收集这些扩展名的文件；空表示只收 .png
		std::vector<std::string> extensions{ ".png" };

		// 是否按文件名排序（保证确定性）
		// ★ 强烈建议保持 true —— 目录遍历顺序在 Windows/NTFS 上是确定的，
		//   但这属于文件系统的实现细节，不该被依赖。
		bool sortByName = true;

		// 是否用「所在子目录名」作为成员名（如 blocks/grass/top.png -> "grass/top"）
		bool useRelativeStem = false;
	};

	TextureArrayAsset LoadTextureArray(std::string_view directory,
		const TextureArrayOptions& opts = {});

	// ---------------- 自动打包 ----------------
	struct AtlasPackItem {
		std::string name;         // 查找键
		std::string sourcePath;   // 源图路径
	};

	struct AtlasPackOptions : TextureLoadOptions {
		uint32_t pageSize = 1024;   // 单页边长（正方形）
		uint32_t padding = 2;      // 每个成员四周预留的像素
		float    insetTexels = 0.5f;
	};

	TextureAtlasAsset PackTextureAtlas(std::span<const AtlasPackItem> items,
		const AtlasPackOptions& opts = {});

	// ---------------- 显式清单 ----------------
	TextureAtlasAsset LoadTextureAtlas(std::string_view manifestPath,
		const TextureLoadOptions& opts = {});
}