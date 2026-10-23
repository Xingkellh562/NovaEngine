#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "texture_region.h"

namespace nova_engine::render::core {

	// 允许用 std::string_view 直接查以 std::string 为 key 的 map
	// （C++20 的异构查找，需要 Hash / KeyEqual 提供 is_transparent）
	//
	// 关键：所有重载都必须经过 std::hash<std::string_view>。
	//   如果 std::string 走 hash<string>、string_view 走 hash<string_view>，
	//   两者对同一个内容的哈希值不保证相同 → 查不到已存在的 key。
	struct TransparentStringHash {
		using is_transparent = void;

		std::size_t operator()(std::string_view sv) const noexcept {
			return std::hash<std::string_view>{}(sv);
		}
		std::size_t operator()(const std::string& s) const noexcept {
			return std::hash<std::string_view>{}(std::string_view{ s });
		}
		std::size_t operator()(const char* s) const noexcept {
			return std::hash<std::string_view>{}(std::string_view{ s });
		}
	};

	struct TransparentStringEqual {
		using is_transparent = void;

		bool operator()(std::string_view a, std::string_view b) const noexcept {
			return a == b;
		}
	};

	using TextureRegionMap = std::unordered_map<
		std::string, TextureRegion,
		TransparentStringHash, TransparentStringEqual>;

	// 把一个 [0,1]² 的局部 UV 映射到 region 上，并写出层索引。
	// mesh builder 只需要调这一个函数。
	inline void ApplyRegion(const TextureRegion& r,
		const glm::vec2& localUV,
		glm::vec2& outUV, float& outLayer) {
		outUV = glm::vec2(r.uv.x, r.uv.y) + localUV * glm::vec2(r.uv.z, r.uv.w);
		outLayer = static_cast<float>(r.layer);
	}

}