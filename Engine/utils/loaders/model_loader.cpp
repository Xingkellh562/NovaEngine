#include "model_loader.h"
#include <fstream>
#include <log/logger.h>


using namespace nova_engine::voxel::block;
using namespace nova_engine::render::opengl;
using json = nlohmann::json;

namespace nova_engine::utils::loader {
	BlockMesh ModelLoader::LoadBlockModelFromJsonPath(const char* path , const GLTextureArray* blockTexture) {
		std::ifstream file(path);
		if (!file.is_open()) {
			NE_CORE_ERROR("ModelLoader","Could not open file with path: {}",path);
			return BlockMesh({});
		}

		try {
			json jData = json::parse(file);
			auto oData = jData.get<OriginalModelData>();

			if (oData.vertices.size() != oData.uvs.size()) {
				NE_CORE_ERROR("ModelLoader", "Could not build model! The file format is uncurrent!");
				return BlockMesh({});
			}
			std::vector<QuadFace> faces;

			int count = oData.vertices.size();
			for (int i = 0;i < count;i++) {
				if (oData.vertices[i].size() != oData.uvs[i].size() && oData.vertices[i].size() != 4) {
					NE_CORE_ERROR("ModelLoader", "Could not build model! The file format is uncurrent!");
					return BlockMesh({});
				}
				int orientation = oData.orientation.size() > i ? oData.orientation[i] : None;

				std::vector<std::array<float, 3>> uvs;
				int textureIndex = blockTexture != nullptr ? blockTexture->FindIndex(oData.textures[i]) : 0;
				for (int j = 0;j < 4;j++) {
					std::array<float, 3> array = { oData.uvs[i][j][0],oData.uvs[i][j][1],textureIndex };
					uvs.push_back(array);
				}
				QuadFace face = QuadFace(std::move(oData.vertices[i]), std::move(uvs), (QuadFaceOrientation)orientation);
				faces.push_back(face);
			}

			return BlockMesh(faces);
		}
		catch (const json::exception& e) {
			NE_CORE_ERROR("ModelLoader", "Failed to build model.{}" ,e.what());
		}
		return BlockMesh({});
	}
}