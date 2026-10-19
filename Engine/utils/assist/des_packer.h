#pragma once

#include <cstdint>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace nova_engine::utils::assist {


	inline float Pack(float d, uint32_t tex) {
		int32_t q = (int32_t)std::lround(d * 8192.0f);
		q = std::clamp(q, -8192, 8191);
		uint32_t uq = (uint32_t)(q + 8192);
		uint32_t bits = 0x3F800000u | ((uq << 9) | (tex & 0x1FFu));
		float stored;
		std::memcpy(&stored, &bits, 4);
		return stored;
	}

	inline float UnpackDensity(float stored) {
		uint32_t bits;
		std::memcpy(&bits, &stored, 4);
		uint32_t packed = bits & 0x7FFFFFu;
		uint32_t uq = packed >> 9;
		int32_t q = (int32_t)uq - 8192;
		return q / 8192.0f;
	}

	inline uint32_t UnpackTexIdx(float stored) {
		uint32_t bits;
		std::memcpy(&bits, &stored, 4);
		uint32_t packed = bits & 0x7FFFFFu;
		return packed & 0x1FFu;
	}

	inline float PackAir() { return Pack(1.0f, 0); }
	inline float PackSoild() { return Pack(-1.0f, 0); }
	inline bool  IsPacked(float stored) { return stored >= 1.0f && stored < 2.0f; }
}