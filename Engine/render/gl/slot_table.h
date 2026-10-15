#pragma once

#include <cstdint>
#include <utility>
#include <vector>

namespace nova_engine::render::gl {
	template <typename T>
	class SlotTable {
	public:
		uint32_t Add(T&& value) {
			if (free_.empty()) {
				slots_.push_back(std::move(value));
				return static_cast<uint32_t>(slots_.size());
			}
			const uint32_t id = free_.back();
			free_.pop_back();
			slots_[id - 1] = std::move(value);
			return id;
		}

		T* Get(uint32_t id) {
			if (id == 0 || id > slots_.size()) return nullptr;
			return &slots_[id - 1];
		}
		const T* Get(uint32_t id) const {
			if (id == 0 || id > slots_.size()) return nullptr;
			return &slots_[id - 1];
		}

		void Remove(uint32_t id) {
			if (id == 0 || id > slots_.size()) return;
			slots_[id - 1] = T{};
			free_.push_back(id);
		}

		void Clear() {
			slots_.clear();
			free_.clear();
		}

		std::size_t Capacity() const { return slots_.size(); }

	private:
		std::vector<T> slots_;
		std::vector<uint32_t> free_;
	};


}