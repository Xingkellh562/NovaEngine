#pragma once

#include <vector>
#include <atomic>

namespace nova_engine::core::concurrency {
	template <class T>
	class SpscQueue{
	public:
		explicit SpscQueue(size_t capacity) :buffer_(capacity), capacity_(capacity) {}

		//存入队列
		bool TryPush(T&& value) {
			size_t w = write_.load(std::memory_order_relaxed);
			//当w = cap - 1时，next = 0呈环形
			size_t next = (w + 1) % capacity_;
			if (next == read_.load(std::memory_order_acquire))
				return false; //队列已满
			buffer_[w] = std::move(value);
			write_.store(next, std::memory_order_release);
			return true;
		}

		//从队列中取出
		bool TryPop(T& out) {
			size_t r = read_.load(std::memory_order_relaxed);
			if (r == write_.load(std::memory_order_acquire))
				return false; //队列为空
			out = std::move(buffer_[r]);
			read_.store((r + 1) % capacity_, std::memory_order_release);
			return true;
		}

		bool Empty() const {
			return read_.load(std::memory_order_acquire) ==
				write_.load(std::memory_order_acquire);
		}
	private:
		std::vector<T> buffer_; //环形缓冲区
		size_t capacity_;
		//读写头，用atomic保证操作原子化
		alignas(64) std::atomic<size_t> write_{ 0 };
		alignas(64) std::atomic<size_t> read_{ 0 };
	};
}