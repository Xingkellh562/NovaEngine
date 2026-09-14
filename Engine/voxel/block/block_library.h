#pragma once
#include "block.h"
#include <memory>
#include <unordered_map>
#include <json/json.hpp>

namespace nova_engine::voxel::block {
	class BlockLibrary {
	public:
		static BlockLibrary& GetInstance() {
			static BlockLibrary instance;
			return instance;
		}

		~BlockLibrary() = default;

		BlockLibrary(const BlockLibrary&) = delete;
		BlockLibrary& operator=(const BlockLibrary&) = delete;

		const Block* FindBlock(int id) const;
		void AddBlock(int id,const std::string& name,const BlockMesh& mesh);

		const size_t Size() const {
			return blocks_.size();
		}
	private:
		BlockLibrary();
		std::unordered_map<int,std::unique_ptr<Block>> blocks_;
	};
	struct BlockBaseData
	{
		int blockId = 0;
		std::string name = "air";

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(BlockBaseData, blockId, name);
	};

	static bool LoadBaseData(const std::string& path, BlockBaseData& outData);
}