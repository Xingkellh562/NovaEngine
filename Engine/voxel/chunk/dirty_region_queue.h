#pragma once

#include <glm/vec3.hpp>

#include <cstdint>
#include <vector>

namespace nova_engine::voxel::chunk {

	// 区块网格失效标记队列。
	//
	// 纯数据：只记录"哪些区块的网格需要重建"，不关心谁来消费、怎么重建。
	// 这正是 World「只存储数据不做逻辑」理念的直接应用。
	class DirtyRegionQueue {
	public:
		void MarkDirty(const glm::ivec3& pos) {
			pending_.push_back(pos);
		}

		// 取出
		bool Drain(glm::ivec3& out) {
			bool empty = pending_.empty();
			if (!empty) {
				out = pending_.back();
				pending_.pop_back();
			}
			else out = glm::ivec3(0.0f);
			return empty;
		}

		bool Empty() const { return pending_.empty(); }
		std::size_t Size() const { return pending_.size(); }

	private:
		std::vector<glm::ivec3> pending_;
	};

}