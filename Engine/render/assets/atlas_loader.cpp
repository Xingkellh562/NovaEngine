#include "texture_loader.h"
#include "atlas_packer.h"
#include <log/logger.h>

namespace nova_engine::render::assets {
	namespace {

		using Placed = struct {
			std::size_t sprite;     // 在 sprites 里的下标
			uint32_t x, y, w, h;    // 成员矩形（不含 padding）
		};

		struct Sprite {
			std::string name;
			render::TextureData image;
		};

		render::TextureData AssemblePage(const std::vector<Sprite>& sprites,
			const std::vector<Placed>& placed,
			uint32_t pageSize, uint32_t pad) {
			// 透明背景。注意：如果用 mipmap，透明黑会在缩小时混入边缘，
			// 所以下面必须做 edge-extend。
			std::vector<unsigned char> px(
				static_cast<std::size_t>(pageSize) * pageSize * 4, 0u);

			const auto stride = static_cast<std::size_t>(pageSize) * 4;

			// ---- blit ----
			for (const Placed& p : placed) {
				const render::TextureData& src = sprites[p.sprite].image;
				const unsigned char* s = src.data();
				const std::size_t srcStride = static_cast<std::size_t>(p.w) * 4;

				for (uint32_t row = 0; row < p.h; ++row) {
					std::memcpy(&px[(p.y + row) * stride + std::size_t(p.x) * 4],
						s + row * srcStride,
						srcStride);
				}
			}

			// ---- edge-extend：把成员边缘像素向外复制到 padding 区 ----
			//
			// 这一步是「任意排布」能不能用的关键：
			//   Linear 采样时，如果 padding 是透明的，缩小后的图标边缘会发暗；
			//   Nearest 时无所谓，但一旦开了 mipmap 或者用 Linear 缩放就必须做。
			//
			// 做法：先把左右列向外扩，再把上下排（含刚扩出来的角）向外扩。
			if (pad > 0) {
				for (const Placed& p : placed) {
					// 左右
					for (uint32_t row = 0; row < p.h; ++row) {
						const unsigned char* left = &px[(p.y + row) * stride + std::size_t(p.x) * 4];
						const unsigned char* right = &px[(p.y + row) * stride + std::size_t(p.x + p.w - 1) * 4];

						for (uint32_t k = 1; k <= pad; ++k) {
							if (p.x >= k) {
								std::memcpy(&px[(p.y + row) * stride + std::size_t(p.x - k) * 4],
									left, 4);
							}
							if (p.x + p.w - 1 + k < pageSize) {
								std::memcpy(&px[(p.y + row) * stride + std::size_t(p.x + p.w - 1 + k) * 4],
									right, 4);
							}
						}
					}

					// 上下（横向范围包含刚扩出的 padding）
					const uint32_t x0 = (p.x >= pad) ? (p.x - pad) : 0u;
					const uint32_t x1 = std::min(p.x + p.w + pad, pageSize);   // 开区间

					for (uint32_t col = x0; col < x1; ++col) {
						const unsigned char* top = &px[std::size_t(p.y) * stride + col * 4];
						const unsigned char* bottom = &px[std::size_t(p.y + p.h - 1) * stride + col * 4];

						for (uint32_t k = 1; k <= pad; ++k) {
							if (p.y >= k) {
								std::memcpy(&px[std::size_t(p.y - k) * stride + col * 4], top, 4);
							}
							if (p.y + p.h - 1 + k < pageSize) {
								std::memcpy(&px[std::size_t(p.y + p.h - 1 + k) * stride + col * 4], bottom, 4);
							}
						}
					}
				}
			}

			return render::TextureData(static_cast<int>(pageSize),
				static_cast<int>(pageSize), 4, px.data());
		}

		core::TextureAtlasDesc MakePageDesc(const std::vector<Sprite>& sprites,
			const std::vector<Placed>& placed,
			uint32_t pageSize, uint32_t pageIndex,
			const AtlasPackOptions& opts) {
			core::TextureAtlasDesc desc;
			desc.width = pageSize;
			desc.height = pageSize;
			desc.layers = 1;
			desc.format = core::TextureFormat::RGBA8;
			desc.defaultSampler = opts.sampler;
			desc.generateMipmaps = opts.generateMipmaps;
			desc.insetTexels = opts.insetTexels;
			desc.pageIndex = pageIndex;

			desc.sources.reserve(placed.size());
			for (const Placed& p : placed) {
				core::TextureSpirit s;
				s.name = sprites[p.sprite].name;
				s.u = p.x;
				s.v = p.y;
				s.width = p.w;
				s.height = p.h;
				desc.sources.push_back(std::move(s));
			}
			return desc;
		}
	} // namespace



	TextureAtlasAsset PackTextureAtlas(std::span<const AtlasPackItem> items,
		const AtlasPackOptions& opts) {
		TextureAtlasAsset out;
		out.insetTexels = opts.insetTexels;

		// ---- 1. 加载全部源图 ----
		
		std::vector<Sprite> sprites;
		sprites.reserve(items.size());

		for (const AtlasPackItem& it : items) {
			render::TextureData img = render::LoadTexture(it.sourcePath.c_str());
			if (img.width() <= 0 || img.channels() != 4) {
				NE_CORE_ERROR("Atlas", "failed to load '{}' ({})", it.name, it.sourcePath);
				return {};
			}
			sprites.push_back({ it.name, std::move(img) });
		}

		if (sprites.empty()) {
			NE_CORE_ERROR("Atlas", "no sprites to pack");
			return {};
		}

		// ---- 2. 按高度降序排序 ----
		// Skyline 对「先放高的」敏感：高的先占位，矮的能塞进缝里。
		std::sort(sprites.begin(), sprites.end(),
			[](const Sprite& a, const Sprite& b) {
				return a.image.height() > b.image.height();
			});

		// ---- 3. 逐页打包 ----
		const uint32_t pad = opts.padding;
		const uint32_t pageSize = opts.pageSize;

		std::size_t cursor = 0;
		std::size_t pageIndex = 0;

		while (cursor < sprites.size()) {
			SkylinePacker packer(pageSize, pageSize);
			std::vector<Placed> placed;

			std::size_t i = cursor;
			for (; i < sprites.size(); ++i) {
				const auto w = static_cast<uint32_t>(sprites[i].image.width());
				const auto h = static_cast<uint32_t>(sprites[i].image.height());

				// 单个成员就装不下一页 —— 配置错误，立刻报错
				if (w + 2 * pad > pageSize || h + 2 * pad > pageSize) {
					if (placed.empty()) {
						NE_CORE_ERROR("Atlas",
							"sprite '{}' ({}x{}) exceeds page {}x{} (padding {})",
							sprites[i].name, w, h, pageSize, pageSize, pad);
						return {};
					}
					break;   // 留给下一页
				}

				SkylinePacker::Rect r;
				// ★ 打包的是「含 padding 的外框」，成员本身放在内框
				if (!packer.Pack(w + 2 * pad, h + 2 * pad, r)) break;

				placed.push_back(Placed{ i, r.x + pad, r.y + pad, w, h });
			}

			if (placed.empty()) {
				NE_CORE_ERROR("Atlas", "could not place any sprite on {}x{} page",
					pageSize, pageSize);
				return {};
			}

			// ---- 4. 组装这一页 ----
			out.pages.push_back(AssemblePage(sprites, placed, pageSize, pad));
			out.descs.push_back(MakePageDesc(sprites, placed, pageSize,
				static_cast<uint32_t>(pageIndex), opts));

			cursor = i;
			++pageIndex;
		}

		NE_CORE_INFO("Atlas", "packed {} sprites into {} page(s) of {}x{}",
			sprites.size(), out.pages.size(), pageSize, pageSize);
		return out;
	}



}