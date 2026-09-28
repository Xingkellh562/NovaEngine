#pragma once

#include <numeric>
#include <memory>
#include <functional>
#include <atomic>

namespace nova_engine::core::concurrency {
	class CancelToken {
	public:
		void cancel() noexcept { flag_.store(true,std::memory_order_release); }

		bool cancelled() const noexcept {
			return flag_.load(std::memory_order_acquire);
		}
	private:
		std::atomic<bool> flag_{false};
	};

	template <class R>
	struct TaskContext {
		uint64_t id;
		std::shared_ptr<CancelToken> cancel;

		// ctx.emit 只在 t.fn(ctx) 执行期间有效
		// 不要把它存到任务外部，否则会悬空
		std::function<bool(R&&)> emit;   // 推到当前 worker 的 resultQueue_

		bool cancelled() const {
			return cancel && cancel->cancelled();
		}
	};

	template<class R>
	struct Result {
		uint64_t id;
		R value;
	};

	template <class R>
	struct Task
	{
		uint64_t id;
		int priority;
		std::shared_ptr<CancelToken> cancel;
		std::move_only_function<void(TaskContext<R>&)> fn;
	}; 
}