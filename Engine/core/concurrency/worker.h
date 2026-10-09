#pragma once

#include <atomic>
#include <string>
#include <thread>
#include <log/logger.h>
#include "task.h"
#include "spsc_queue.h"

namespace nova_engine::core::concurrency {
	

	template <class R>
	class Worker {
	public:
		Worker(std::string name,size_t taskCap,size_t resultCap):
			name_(name),taskQueue_(taskCap),resultQueue_(resultCap)
		{}

		~Worker() { Stop(); }

		Worker(const Worker&) = delete;
		Worker& operator=(const Worker&) = delete;
		Worker(Worker&&) = delete;
		Worker& operator=(Worker&&) = delete;

		void Start() {
			thread_ = std::thread([this] { Run();});
		}

		void Stop() {
			stop_.store(true, std::memory_order_release);
			if (thread_.joinable())thread_.join();
		}

		bool TrySubmitTask(Task<R>&& task) {
			return taskQueue_.TryPush(std::move(task));
		}

		bool TryPopResult(Result<R>& out) {
			return resultQueue_.TryPop(out);
		}

		size_t GetSubmitTaskCount() const {
			return taskQueue_.Size();
		}

		size_t GetResultCount() const {
			return resultQueue_.Size();
		}
	private:
		std::string name_;
		std::thread thread_;
		SpscQueue<Task<R>> taskQueue_;
		SpscQueue<Result<R>> resultQueue_;
		std::atomic<bool> stop_{ false };

		void Run() {
			while (!stop_.load(std::memory_order_acquire)){
				Task<R> t;
				if (!taskQueue_.TryPop(t)) {
					std::this_thread::sleep_for(std::chrono::microseconds(50));
					continue;
				}

				TaskContext<R> ctx;

				ctx.id = t.id;
				ctx.cancel = t.cancel;
				ctx.emit = [this, id = t.id](R&& r) -> bool {
					Result<R> res{ id, std::move(r) };
					if (!resultQueue_.TryPush(std::move(res))) {
						NE_CORE_WARN(name_.c_str(), "result queue full, drop result id={}", id);
						return false;
					}
					return true;
				};

				if (ctx.cancelled()) continue;
				try {
					t.fn(ctx);
				}
				catch (const std::exception& e) {
					NE_CORE_ERROR(name_.c_str(), "Internal error: {} , task_id = {}", e.what(),t.id);
				}
				catch (...) {
					NE_CORE_ERROR(name_.c_str(), "Unknown internal error , task_id = {}", t.id);
				}
			}

			size_t dropped = 0;
			Task<R> t;
			while (taskQueue_.TryPop(t)) ++dropped;
			if (dropped) NE_CORE_WARN(name_, "worker stopped, {} tasks dropped", dropped);
		}
	};
}