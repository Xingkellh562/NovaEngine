#pragma once

#include "enums.h"
#include "handles.h"
#include "sampler_state.h"

#include <cstdint>

namespace nova_engine::render::core {
	enum class VertexLayoutId : uint64_t {
		VoxelMain = 0,
		VoxelShadow = 1,
		VoxelLight = 2,
		ScreenQuad = 3,
		UI = 4,
	};

	struct PipelineState {
		ShaderHandle shader{};

		BlendMode blendMode = BlendMode::Opaque;
		SamplerState samplerState{};

		DepthTest depthTest = DepthTest::Less;
		bool depthWrite = true;
		bool colorWrite = true;

		CullMode cullMode = CullMode::Back;
		Topology topology = Topology::TriangleList;

		uint32_t colorTargets = 1; //颜色输出目标数量,比如WBOIT需要2（accum/reveal）
		VertexLayoutId vertexLayout = VertexLayoutId::VoxelMain;

	};
}