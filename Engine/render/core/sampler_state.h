#pragma once

#include <cstdint>

namespace nova_engine::render::core {
	enum class TextureFilter : uint8_t {
		Nearest,
		Linear,
		NearestMipmapLinear,
		LinearMipmapLinear,
	};

	enum class TextureWrap : uint8_t {
		Repeat,
		ClampToEdge,
		ClampToBorder,
	};
	
	//采样器状态值。后端按它来缓存采样器对象
	//属于管线状态而非纹理属性
	struct SamplerState {
		TextureFilter minFilter = TextureFilter::Linear;
		TextureFilter magFilter = TextureFilter::Linear;
		TextureWrap wrap = TextureWrap::ClampToEdge;
		
		// ★ 深度比较模式必须在这里，不能只设在纹理上。
		//
		// GL 规范（3.3+）把 GL_TEXTURE_COMPARE_MODE / GL_TEXTURE_COMPARE_FUNC
		// 归为**采样器对象状态**，采样器绑定到纹理单元时会覆盖纹理自身的设置。
		//
		// 踩过的坑：ApplyPipelineState 对 unit 0~7 统一绑一个默认采样器，
		// 其 COMPARE_MODE == GL_NONE，把 ShadowMap 纹理的
		// GL_COMPARE_REF_TO_TEXTURE 覆盖掉了 —— 而 post_process_frag.glsl 里
		// 声明的是 uniform sampler2DShadow，compare mode 为 NONE 时
		// texture() 的返回值是**未定义的**。阴影静默失效，不报任何错。
		bool depthCompareMode = false;

		friend bool operator==(const SamplerState&, const SamplerState&) = default;
		friend bool operator==(const SamplerState&, const SamplerState&) = default;
	};
}