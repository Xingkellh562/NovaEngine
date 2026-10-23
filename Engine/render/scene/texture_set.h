#pragma once

#include <render/core/handles.h>
#include <render/core/texture_desc.h>
#include <render/core/texture_hash.h>
#include <render/core/texture_region.h>

#include <glm/vec2.hpp>

#include <span>
#include <vector>

namespace nova_engine::render::device { class IRenderDevice; }

namespace nova_engine::render::scene {

	// 一份具名的纹理资源 + O(1) 成员查找表。
	//
	// 三种来源共用同一个查询接口：
	//   CreateFromArray  —— 纹理数组（方块纹理）
	//   CreateFromAtlas  —— 图集（UI 图标、字体），支持多页
	//   CreateFromSingle —— 单张 2D
	//
	// 典型用法（每帧/每批）：
	//     const core::TextureRegion* r = uiSet.Find("icon_pickaxe");
	//     if (!r) return;                       // ★ 找不到要显式处理
	//     cmd.BindTexture(0, "uAtlas", uiSet.PageHandle(r->page));
	//
	// ★ 生命周期：与 MeshInstance 相同 —— 必须在 IRenderDevice 销毁前释放。
	//   见 7.4。
	class TextureSet {
	public:
		enum class Kind : uint8_t { Single2D, Array2D, Atlas2D };

		TextureSet() = default;
		~TextureSet();

		TextureSet(const TextureSet&) = delete;
		TextureSet& operator=(const TextureSet&) = delete;
		TextureSet(TextureSet&&) noexcept;
		TextureSet& operator=(TextureSet&&) noexcept;

		// ---- 工厂 ----
		static TextureSet CreateFromArray(device::IRenderDevice&,
			const core::TextureArrayDesc&,
			std::span<const std::byte* const> layers);

		static TextureSet CreateFromAtlas(device::IRenderDevice&,
			std::span<const core::TextureAtlasDesc> pageDescs,
			std::span<const std::byte* const> pagePixels);

		static TextureSet CreateFromSingle(device::IRenderDevice&,
			const core::TextureDesc&,
			const std::byte* pixels);

		// ---- 查询 ----
		// O(1)。未找到返回 nullptr 并记录 error —— 绝不返回"默认成员"。
		const core::TextureRegion* Find(std::string_view name) const;
		bool Contains(std::string_view name) const;

		// ---- 资源 ----
		core::TextureHandle PageHandle(uint32_t page) const;
		uint32_t            PageCount() const { return static_cast<uint32_t>(pages_.size()); }

		Kind       kind() const { return kind_; }
		glm::ivec2 size() const { return size_; }
		bool       Valid() const { return !pages_.empty(); }

	private:
		void Destroy();

		device::IRenderDevice* device_ = nullptr;   // 仅用于析构归还
		Kind                   kind_ = Kind::Single2D;
		glm::ivec2             size_{ 0, 0 };
		std::vector<core::TextureHandle> pages_;
		core::TextureRegionMap regions_;
	};

}