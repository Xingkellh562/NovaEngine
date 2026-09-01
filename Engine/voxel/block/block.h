#pragma once

#include <string>
#include "block_mesh.h"
namespace nova_engine::voxel::block {
	class Block {
	public:
		Block(int id, const std::string& name, const BlockMesh& mesh) :
			id_(id), name_(name), mesh_(mesh) {
		}
		~Block() = default;

		Block(const Block&) = delete;
		Block& operator=(const Block&) = delete;

		inline int GetID() const { return id_; }
		inline const std::string& GetName() const{ return name_; }
		inline const BlockMesh& GetMesh() const { return mesh_; }
	private:
		const int id_ = 0;
		const std::string name_ = "air";
		const BlockMesh mesh_;
	};
}