#pragma once

#include <cstdint>

namespace nova_engine::render::core {
	enum class BlendMode : uint8_t {
		Opaque,
		AlphaBlend,
		Additive,
		WboitAccum,
	};

	enum class DepthTest :uint8_t {
		None,
		Less,
		LessEqual,
		Greater,
		Always,
	};

	enum class CullMode : uint8_t {
		None,
		Front,
		Back,
	};

	enum class LoadOp : uint8_t {
		Load,
		Clear,
		DontCare
	};

	enum class Topology : uint8_t {
		TriangleList,
		LineList,
		PointList,
	};
}