#include "logger.h"

namespace nova_engine::log {
	std::shared_ptr<spdlog::logger> LogManager::s_CoreLogger;
	std::shared_ptr<spdlog::logger> LogManager::s_ClientLogger;

	void LogManager::Init() {
		spdlog::set_level(spdlog::level::trace);
		const char* pattern = "[%Y-%m-%d %H:%M:%S.%e] [%t] [%^%l%$] [%n] %v";
		spdlog::set_pattern(pattern);

		spdlog::init_thread_pool(8192, 2);

		std::vector<spdlog::sink_ptr> sinks;
		auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		console_sink->set_level(spdlog::level::trace);
		console_sink->set_pattern(pattern);

		auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
			"logs/NovaEngine.log", 1024 * 1024 * 10, 5);

		file_sink->set_level(spdlog::level::trace);
		file_sink->set_pattern(pattern);

		sinks.push_back(console_sink);
		sinks.push_back(file_sink);

		s_CoreLogger = std::make_shared<spdlog::async_logger>(
			"CORE", sinks.begin(), sinks.end(), spdlog::thread_pool()
		);
		s_CoreLogger->set_level(spdlog::level::trace);
		s_CoreLogger->set_pattern(pattern);

		s_ClientLogger = std::make_shared<spdlog::async_logger>(
			"CLIENT", sinks.begin(), sinks.end(), spdlog::thread_pool()
		);
		s_ClientLogger->set_level(spdlog::level::trace);
		s_ClientLogger->set_pattern(pattern);

		spdlog::set_default_logger(s_CoreLogger);


		NE_CORE_INFO("LogSystem", "Log system initialized successfully");
	}

}