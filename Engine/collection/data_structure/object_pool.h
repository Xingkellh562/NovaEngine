#pragma once

#include <vector>
#include <memory>

namespace nova_engine::collection::data_structure {
	template <typename T,size_t MaxSize>
	class ObjectPool {
		static_assert(MaxSize > 0, "MaxSize must be greater than 0");
	public :
		template <typename... Args>
		std::unique_ptr<T> Acquire(Args&&... args) {
			if (!pool_.empty()) {
				auto obj = std::move(pool_.back());
				pool_.pop_back();
				return obj;
			}
			return std::make_unique<T>(std::forward<Args>(args)...);
		}

		void Recycle(std::unique_ptr<T> obj) {
			if (!obj) return;

			if (pool_.size() < MaxSize) {
				pool_.push_back(std::move(obj));
			}
		}

		void Shrink() {
			pool_.clear();
			pool_.shrink_to_fit();
		}

		size_t Size() const {
			return pool_.size();
		}
	private:
		std::vector<std::unique_ptr<T>> pool_;
	};
}