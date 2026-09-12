#pragma once

#include <vector>
#include <array>
#include <string>
#include <json/json.hpp>
#include <voxel/block/block_mesh.h>
#include <render/opengl/library/texture_array_library.h>

namespace nova_engine::utils::loader {
	struct OriginalModelData {
		std::vector<std::vector<std::array<float, 3>>> vertices;
		std::vector<std::vector<std::array<float, 2>>> uvs;
		std::vector<int> orientation;
		std::vector<std::string> textures;
		int type;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(OriginalModelData, vertices, uvs, orientation, textures,type);
	};

	class ModelLoader {
	public:
		static nova_engine::voxel::block::BlockMesh LoadBlockModelFromJsonPath(const char* path,const render::opengl::GLTextureArray* blockTexture);
	};
}