#pragma once

#include <string>
#include <vector>

namespace nova_engine::render::core {
	enum class ShaderStage {
		Vertex,
		Fragment,
		Geometry,
		Compute,
	};

	struct ShaderStageDesc {
		ShaderStage stage = ShaderStage::Vertex;
		std::string path;//相对可执行文件的glsl源码路径
	};

	struct ShaderDesc {
		std::vector<ShaderStageDesc> stages;
		std::vector<std::string> defines;
	};
}