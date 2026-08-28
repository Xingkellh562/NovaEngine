#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <memory>

namespace nova_engine::log {
	class LogManager {
	public:
		static void Init();

		inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

	private:
		static std::shared_ptr<spdlog::logger> s_CoreLogger;
		static std::shared_ptr<spdlog::logger> s_ClientLogger;
	};

	template<typename... Args>
	inline void CoreLog(spdlog::level::level_enum level,
		const std::string& sysname,
		const std::string& fmt,
		Args&&... args) {
		auto logger = LogManager::GetCoreLogger();
		if (logger) {
			// 拼接系统名前缀 + 用户格式化的消息
			std::string full_msg = "[" + sysname + "] " + fmt::format(fmt::runtime(fmt), std::forward<Args>(args)...);
			logger->log(level, full_msg);
		}
	}

	template<typename... Args>
	inline void ClientLog(spdlog::level::level_enum level,
		const std::string& sysname,
		const std::string& fmt,
		Args&&... args) {
		auto logger = LogManager::GetClientLogger();
		if (logger) {
			// 拼接系统名前缀 + 用户格式化的消息
			std::string full_msg = "[" + sysname + "] " + fmt::format(fmt::runtime(fmt), std::forward<Args>(args)...);
			logger->log(level, full_msg);
		}
	}
}





#define NE_CORE_TRACE(sysname, fmt, ...) \
    ::nova_engine::log::CoreLog(spdlog::level::trace, sysname, fmt, ##__VA_ARGS__)

#define NE_CORE_DEBUG(sysname, fmt, ...) \
    ::nova_engine::log::CoreLog(spdlog::level::debug, sysname, fmt, ##__VA_ARGS__)

#define NE_CORE_INFO(sysname, fmt, ...) \
    ::nova_engine::log::CoreLog(spdlog::level::info, sysname, fmt, ##__VA_ARGS__)

#define NE_CORE_WARN(sysname, fmt, ...) \
    ::nova_engine::log::CoreLog(spdlog::level::warn, sysname, fmt, ##__VA_ARGS__)

#define NE_CORE_ERROR(sysname, fmt, ...) \
    ::nova_engine::log::CoreLog(spdlog::level::err, sysname, fmt, ##__VA_ARGS__)

#define NE_CORE_CRITICAL(sysname, fmt, ...) \
    ::nova_engine::log::CoreLog(spdlog::level::critical, sysname, fmt, ##__VA_ARGS__)

#define NE_CLIENT_TRACE(sysname, fmt, ...) \
    ::nova_engine::log::ClientLog(spdlog::level::trace, sysname, fmt, ##__VA_ARGS__)

#define NE_CLIENT_DEBUG(sysname, fmt, ...) \
    ::nova_engine::log::ClientLog(spdlog::level::debug, sysname, fmt, ##__VA_ARGS__)

#define NE_CLIENT_INFO(sysname, fmt, ...) \
    ::nova_engine::log::ClientLog(spdlog::level::info, sysname, fmt, ##__VA_ARGS__)

#define NE_CLIENT_WARN(sysname, fmt, ...) \
    ::nova_engine::log::ClientLog(spdlog::level::warn, sysname, fmt, ##__VA_ARGS__)

#define NE_CLIENT_ERROR(sysname, fmt, ...) \
    ::nova_engine::log::ClientLog(spdlog::level::err, sysname, fmt, ##__VA_ARGS__)

#define NE_CLIENT_CRITICAL(sysname, fmt, ...) \
    ::nova_engine::log::ClientLog(spdlog::level::critical, sysname, fmt, ##__VA_ARGS__)