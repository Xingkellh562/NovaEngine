#include "block_library.h"
#include <log/logger.h>

#include <fstream>
#include <filesystem>
#include <utils/loaders/model_loader.h>
#include <render/opengl/library/texture_array_library.h>

using json = nlohmann::json;

namespace nova_engine::voxel::block 
{
	const Block* BlockLibrary::FindBlock(int id) const{
		auto it = blocks_.find(id);
		if (it != blocks_.end()) {
			return it->second.get();
		}
		else {
			it = blocks_.find(0);
			if (it != blocks_.end()) {
				return it->second.get();
			}
		}
		return nullptr;
	}

	void BlockLibrary::AddBlock(int id, const std::string& name, const BlockMesh& mesh) {
		blocks_.try_emplace(id,std::make_unique<Block>(id,name,mesh));
		NE_CORE_INFO("BlockLoader","Load block <id:{};name:{},mesh_type:{}>",id,name,mesh.modelType);
	}

	

	const std::string BLOCKDIR = "assets/blocks";

	BlockLibrary::BlockLibrary() {
		namespace fs = std::filesystem;

		AddBlock(0, "air", BlockMesh({},-1));

		fs::path blocksDir(BLOCKDIR);
		if (!fs::exists(blocksDir) || !fs::is_directory(blocksDir)) {
			NE_CORE_ERROR("BlockLoader", "Block directory not found: {}", BLOCKDIR);
			throw std::runtime_error("Block directory not found!");
		}
		for (const auto& entry : fs::directory_iterator(blocksDir)) {
			if (!entry.is_directory()) continue;

			fs::path blockFolder = entry.path();
			std::string blockName = blockFolder.filename().string();

			fs::path dataPath = blockFolder / "block.json";
			fs::path modelPath = blockFolder / "model.json";

			if (!fs::exists(dataPath)) {
				NE_CORE_WARN("BlockLoader","Skipping '{}':missing block.json", blockName);
				continue;
			}
			if (!fs::exists(modelPath)) {
				NE_CORE_WARN("BlockLoader", "Skipping '{}':missing model.json", blockName);
				continue;
			}

			BlockBaseData meta;
			if (!LoadBaseData(dataPath.string(), meta)) {
				NE_CORE_ERROR("BlockLoader","Failed to load metadata for '{}'",blockName);
				continue;
			}

			BlockMesh mesh = utils::loader::ModelLoader::LoadBlockModelFromJsonPath(
				modelPath.string().c_str(),
				render::opengl::TextureArrayLibrary::GetInstance().GetTextureArray("BlockTextures"));

			AddBlock(meta.blockId, meta.name,mesh);
		}


	}

	bool LoadBaseData(const std::string& path, BlockBaseData& outData) {
		try {
			std::ifstream file(path);
			if (!file.is_open()) return false;

			json j = json::parse(file);
			outData = j.get<BlockBaseData>();
			return true;
		}
		catch(json::exception& e){
			NE_CORE_ERROR("BlockLoader","JSON parse error in {}:{}",path,e.what());
			return false;
		}
	}
}