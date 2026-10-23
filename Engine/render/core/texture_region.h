#pragma once

#include <cstdint>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace nova_engine::render::core {

	// 一个具名纹理成员在纹理资源里的定位。
	//
	// 三种纹理的取值：
	//   单张 2D  : page=0  layer=0  uv=(0,0,1,1)
	//   纹理数组 : page=0  layer=n  uv=(0,0,1,1)
	//   图集    : page=k  layer=0  uv=子矩形（已归一化）
	//
	// uv 的语义是 (u0, v0, du, dv) 而不是 (u0,v0,u1,v1)，
	// 因为 mesh builder 需要的是缩放+平移：
	//     finalUV = uv0 + localUV * uvScale
	// 用 (du,dv) 少一次减法。
	struct TextureRegion {
		uint32_t  page = 0;
		uint32_t  layer = 0;
		glm::vec4 uv{ 0.0f, 0.0f, 1.0f, 1.0f };
		glm::ivec2 pixelSize{ 0, 0 };   // 便于 UI 保持宽高比；非必需

		bool valid() const {
			return uv.z > 0.0f && uv.w > 0.0f;
		}
	};

}