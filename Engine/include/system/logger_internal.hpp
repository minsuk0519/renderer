#pragma once

#include <iostream>
#include <array>
#include <thread>
#include <semaphore>
#include <Windows.h>
#include <chrono>

#include <system\defines.hpp>

namespace logger
{
	enum LOG_LEVEL
	{
		LOG_INFO = 0,			//log level with generate information
		LOG_WARNING = 1,		//log level that potentially cause undefined behavior
		LOG_ERROR = 2,			//log level that must cause the error
		LOG_FATAL = 3,			//log level that will break the entire engine
		LOG_MAX,
	};

	void init_logger();

	std::string generate_log_msg(const char* name, uint lineNum, const char* fileName, const LOG_LEVEL& logLevel);

	void add_msg(const std::string msg, const LOG_LEVEL& logLevel);

	void print_log(const char* name, uint lineNum, const char* fileName, const LOG_LEVEL& logLevel);

	void flush(const LOG_LEVEL& logLevel);

	void write();

	void logger_end();

};