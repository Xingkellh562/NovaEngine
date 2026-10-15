#pragma once

#include <cstdint>

namespace nova_engine::render::core {
	template<class Tag>
	struct Handle {
		uint32_t id = 0;
		constexpr bool Valid() const noexcept { return id != 0; }
		constexpr explicit operator bool() const noexcept { return Valid(); }
		friend constexpr bool operator==(Handle,Handle) = default;
	};

	using MeshHandle = Handle<struct MeshTag>;
	using TextureHandle = Handle<struct TextureTag>;
	using ShaderHandle = Handle<struct ShaderTag>;
	using RenderTargetHandle = Handle<struct RenderTargetTag>;
}
