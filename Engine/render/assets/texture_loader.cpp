#include "texture_loader.h"
#include <log/logger.h>
#include <filesystem>

namespace nova_engine::render::assets {

	TextureImageAsset LoadTextureImage(std::string_view path,
		const TextureLoadOptions& opts) {
		TextureImageAsset out;

		render::TextureData data = render::LoadTexture(std::string(path).c_str());
		if (data.width() <= 0 || data.height() <= 0) {
			NE_CORE_ERROR("TextureLoader", "failed to load '{}'", path);
			return out;
		}

		// 统一解码为 RGBA8，避免后续 glTexSubImage 的 format/type 组合出错
		// （stb_image 会把任意通道数解码成 4 通道 —— 见 render/texture.cpp 的 LoadTexture）
		if (data.channels() != 4) {
			NE_CORE_ERROR("TextureLoader",
				"'{}' has {} channels, expected RGBA (4)", path, data.channels());
			return out;
		}

		out.image = std::move(data);
		out.desc.width = static_cast<uint32_t>(out.image.width());
		out.desc.height = static_cast<uint32_t>(out.image.height());
		out.desc.format = core::TextureFormat::RGBA8;
		out.desc.layers = 1;
		out.desc.defaultSampler = opts.sampler;
		out.desc.generateMipmaps = opts.generateMipmaps;
		return out;
	}

	TextureArrayAsset LoadTextureArray(std::string_view directory,
		const TextureArrayOptions& opts) {
		namespace fs = std::filesystem;
		TextureArrayAsset out;

		if (!fs::is_directory(directory)) {
			NE_CORE_ERROR("TextureLoader", "not a directory: '{}'", directory);
			return out;
		}

		// ---- 1. 收集文件 ----
		std::vector<fs::path> files;
		for (const auto& entry : fs::directory_iterator(directory)) {
			if (!entry.is_regular_file()) continue;
			const auto ext = entry.path().extension().string();
			if (std::find(opts.extensions.begin(), opts.extensions.end(), ext)
				== opts.extensions.end()) continue;
			files.push_back(entry.path());
		}

		if (files.empty()) {
			NE_CORE_ERROR("TextureLoader", "no matching images in '{}'", directory);
			return out;
		}

		// ---- 2. 排序（确定性）----
		if (opts.sortByName) {
			std::sort(files.begin(), files.end());
		}

		// ---- 3. 全量加载 + 尺寸校验 ----
		std::vector<render::TextureData> layers;
		layers.reserve(files.size());

		uint32_t tileW = 0, tileH = 0;

		for (std::size_t i = 0; i < files.size(); ++i) {
			render::TextureData data = render::LoadTexture(files[i].string().c_str());
			if (data.width() <= 0 || data.channels() != 4) {
				NE_CORE_ERROR("TextureLoader", "layer '{}' failed to load",
					files[i].string());
				return out;
			}

			const auto w = static_cast<uint32_t>(data.width());
			const auto h = static_cast<uint32_t>(data.height());

			// 第一张决定 tileSize
			if (i == 0) {
				tileW = w; tileH = h;
			}
			// 后续必须完全一致，否则 glTexSubImage3D 会错位
			if (w != tileW || h != tileH) {
				NE_CORE_ERROR("TextureLoader",
					"size mismatch: '{}' is {}x{}, expected {}x{} ('{}')",
					files[i].string(), w, h, tileW, tileH, files[0].string());
				return out;
			}

			layers.push_back(std::move(data));
		}

		// ---- 4. 填满 desc（这一步以前是缺失的）----
		out.desc.tileSize = tileW;                          // ★
		out.desc.layers = static_cast<uint32_t>(layers.size());  // ★
		out.desc.width = tileW;
		out.desc.height = tileH;
		out.desc.format = core::TextureFormat::RGBA8;
		out.desc.defaultSampler = opts.sampler;
		out.desc.generateMipmaps = opts.generateMipmaps;

		out.desc.sources.reserve(layers.size());
		for (std::size_t i = 0; i < files.size(); ++i) {
			core::TextureArrayLayer layer;
			layer.name = opts.useRelativeStem
				? fs::relative(files[i], directory).replace_extension().string()
				: files[i].stem().string();
			layer.path = files[i].string();
			layer.index = static_cast<int>(i);              // ★
			out.desc.sources.push_back(std::move(layer));
		}

		out.layers = std::move(layers);

		NE_CORE_INFO("TextureLoader",
			"array '{}': {} layers, {}x{}", directory,
			out.desc.layers, tileW, tileH);
		return out;
	}



}