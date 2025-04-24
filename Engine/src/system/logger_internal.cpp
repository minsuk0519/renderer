#include "system\logger_internal.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/cfg/env.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace logger
{
	void init_logger()
	{
		spdlog::cfg::load_env_levels();

		spdlog::info("Initialize logger...");
	}

	std::string logger::generate_log_msg(const char* name, uint lineNum, const char* fileName, const LOG_LEVEL& logLevel)
	{
		std::string result;

		if (logLevel > LOG_LEVEL::LOG_INFO) result = std::format("{} ({}, {})", name, fileName, lineNum);
		else result = std::format("{}", name);

		return result;
	}

	void logger::add_msg(const std::string msg, const LOG_LEVEL& logLevel)
	{
		if (logLevel == LOG_LEVEL::LOG_INFO) spdlog::info(msg);
		else if (logLevel == LOG_LEVEL::LOG_WARNING) spdlog::warn(msg);
		else if (logLevel == LOG_LEVEL::LOG_ERROR) spdlog::error(msg);
		//when loglevel is fatal
		else spdlog::critical(msg);
	}

	void logger::print_log(const char* name, uint lineNum, const char* fileName, const LOG_LEVEL& logLevel)
	{
		std::string log_msg = generate_log_msg(name, lineNum, fileName, logLevel);

		add_msg(log_msg, logLevel);
	}

	void logger::logger_end()
	{
		spdlog::shutdown();
	}
}