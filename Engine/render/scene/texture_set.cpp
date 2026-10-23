// render/scene/texture_set.cpp
#include "texture_set.h"

#include <render/device/render_device.h>

#include <log/logger.h>

#include <glm/vec4.hpp>

namespace nova_engine::render::scene {

	using namespace nova_engine::render;

	namespace {

		core::TextureRegion MakeArrayRegion(uint32_t layer) {
			core::TextureRegion r;
			r.page = 0;
			r.layer = layer;
			r.uv = glm::vec4{ 0.0f, 0.0f, 1.0f, 1.0f };
			return r;
		}

		core::TextureRegion MakeAtlasRegion(uint32_t page,
			const core::TextureSpirit& s,
			uint32_t atlasW, uint32_t atlasH,
			float insetTexels) {
			core::TextureRegion r;
			r.page = page;
			r.layer = 0;
			r.pixelSize = { static_cast<int>(s.width),
							static_cast<int>(s.height) };

			const float fw = static_cast<float>(atlasW);
			const float fh = static_cast<float>(atlasH);
			const float ix = insetTexels / fw;
			const float iy = insetTexels / fh;

			r.uv = glm::vec4{
				static_cast<float>(s.u) / fw + ix,
				static_cast<float>(s.v) / fh + iy,
				static_cast<float>(s.width) / fw - 2.0f * ix,
				static_cast<float>(s.height) / fh - 2.0f * iy,
			};
			return r;
		}

		std::size_t BytesOf(const core::TextureDesc& d) {
			return static_cast<std::size_t>(d.width) * d.height * 4u;
		}

	} // namespace

	// ---------------------------------------------------------------------------

	TextureSet::~TextureSet() { Destroy(); }

	TextureSet::TextureSet(TextureSet&& o) noexcept
		: device_(o.device_), kind_(o.kind_), size_(o.size_),
		pages_(std::move(o.pages_)), regions_(std::move(o.regions_)) {
		o.device_ = nullptr;
		o.pages_.clear();
	}

	TextureSet& TextureSet::operator=(TextureSet&& o) noexcept {
		if (this != &o) {
			Destroy();
			device_ = o.device_;
			kind_ = o.kind_;
			size_ = o.size_;
			pages_ = std::move(o.pages_);
			regions_ = std::move(o.regions_);
			o.device_ = nullptr;
			o.pages_.clear();
		}
		return *this;
	}

	void TextureSet::Destroy() {
		if (device_ != nullptr) {
			for (const core::TextureHandle& h : pages_) {
				if (h.Valid()) device_->DestroyTexture(h);
			}
		}
		pages_.clear();
		regions_.clear();
		device_ = nullptr;
	}

	// ---------------------------------------------------------------------------
	// 工厂
	// ---------------------------------------------------------------------------

	TextureSet TextureSet::CreateFromArray(device::IRenderDevice& device,
		const core::TextureArrayDesc& desc,
		std::span<const std::byte* const> layers) {
		TextureSet set;

		if (desc.tileSize == 0 || desc.layers == 0 ||
			layers.size() != desc.sources.size()) {
			NE_CORE_ERROR("TextureSet",
				"CreateFromArray: bad desc (tileSize={} layers={} sources={} pixels={})",
				desc.tileSize, desc.layers, desc.sources.size(), layers.size());
			return set;
		}

		const core::TextureHandle h = device.CreateTextureArray(desc, layers);
		if (!h.Valid()) {
			NE_CORE_ERROR("TextureSet", "CreateFromArray: device rejected the array");
			return set;
		}

		set.device_ = &device;
		set.kind_ = Kind::Array2D;
		set.size_ = { static_cast<int>(desc.tileSize),
						static_cast<int>(desc.tileSize) };
		set.pages_.push_back(h);

		set.regions_.reserve(desc.sources.size());
		for (const core::TextureArrayLayer& layer : desc.sources) {
			// 用 index 而不是枚举下标 —— 两者应当相同（加载器保证），
			// 但如果有人手工填充 desc，用 index 更符合语义。
			const auto r = MakeArrayRegion(static_cast<uint32_t>(layer.index));
			set.regions_.emplace(layer.name, r);
		}

		NE_CORE_INFO("TextureSet",
			"array created: {} members, {}x{} x{}",
			set.regions_.size(), desc.tileSize, desc.tileSize, desc.layers);
		return set;
	}

	TextureSet TextureSet::CreateFromAtlas(
		device::IRenderDevice& device,
		std::span<const core::TextureAtlasDesc> pageDescs,
		std::span<const std::byte* const> pagePixels) {

		TextureSet set;

		if (pageDescs.empty() || pageDescs.size() != pagePixels.size()) {
			NE_CORE_ERROR("TextureSet",
				"CreateFromAtlas: {} pages, {} pixel blocks",
				pageDescs.size(), pagePixels.size());
			return set;
		}

		set.device_ = &device;
		set.kind_ = Kind::Atlas2D;
		set.pages_.reserve(pageDescs.size());

		for (std::size_t i = 0; i < pageDescs.size(); ++i) {
			const core::TextureAtlasDesc& d = pageDescs[i];

			if (d.width == 0 || d.height == 0 || pagePixels[i] == nullptr) {
				NE_CORE_ERROR("TextureSet", "CreateFromAtlas: page {} has no data", i);
				set.Destroy();
				return {};
			}

			const std::span<const std::byte> bytes{ pagePixels[i], BytesOf(d) };
			const core::TextureHandle h = device.CreateTexture(d, bytes);
			if (!h.Valid()) {
				NE_CORE_ERROR("TextureSet", "CreateFromAtlas: device rejected page {}", i);
				set.Destroy();
				return {};
			}
			set.pages_.push_back(h);

			if (i == 0) {
				set.size_ = { static_cast<int>(d.width), static_cast<int>(d.height) };
			}

			set.regions_.reserve(set.regions_.size() + d.sources.size());
			for (const core::TextureSpirit& s : d.sources) {
				const auto r = MakeAtlasRegion(static_cast<uint32_t>(i), s,
					d.width, d.height, d.insetTexels);

				const auto [it, inserted] = set.regions_.emplace(s.name, r);
				if (!inserted) {
					NE_CORE_WARN("TextureSet",
						"duplicate sprite name '{}' (page {}), keeping the first",
						s.name, i);
				}
			}
		}

		NE_CORE_INFO("TextureSet", "atlas created: {} members over {} page(s)",
			set.regions_.size(), set.pages_.size());
		return set;
	}

	TextureSet TextureSet::CreateFromSingle(device::IRenderDevice& device,
		const core::TextureDesc& desc,
		const std::byte* pixels) {
		TextureSet set;

		if (desc.width == 0 || desc.height == 0 || pixels == nullptr) {
			NE_CORE_ERROR("TextureSet", "CreateFromSingle: empty desc / null pixels");
			return set;
		}

		const std::span<const std::byte> bytes{ pixels, BytesOf(desc) };
		const core::TextureHandle h = device.CreateTexture(desc, bytes);
		if (!h.Valid()) {
			NE_CORE_ERROR("TextureSet", "CreateFromSingle: device rejected the texture");
			return set;
		}

		set.device_ = &device;
		set.kind_ = Kind::Single2D;
		set.size_ = { static_cast<int>(desc.width), static_cast<int>(desc.height) };
		set.pages_.push_back(h);
		return set;
	}

	// ---------------------------------------------------------------------------
	// 查询
	// ---------------------------------------------------------------------------

	const core::TextureRegion* TextureSet::Find(std::string_view name) const {
		const auto it = regions_.find(name);      // 透明查找：不构造 std::string
		if (it == regions_.end()) {
			NE_CORE_ERROR("TextureSet", "texture member '{}' not found", name);
			return nullptr;
		}
		return &it->second;
	}

	bool TextureSet::Contains(std::string_view name) const {
		return regions_.find(name) != regions_.end();
	}

	core::TextureHandle TextureSet::PageHandle(uint32_t page) const {
		if (page >= pages_.size()) {
			NE_CORE_ERROR("TextureSet", "page {} out of range ({} pages)", page, pages_.size());
			return {};
		}
		return pages_[page];
	}

}