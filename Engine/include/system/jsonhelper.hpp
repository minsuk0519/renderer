#pragma once

#include <string>

#include <glaze/glaze.hpp>

#include "defines.hpp"

struct configJson
{
	uint width;
	uint height;
};

template <typename Buffer>
void readJsonBuffer(Buffer& buf, const std::string& fileName)
{
	std::string str{};
	auto error = glz::read_file_json(buf, "data/config.json", str);

	if (error.ec != glz::error_code::none)
	{
		TC_LOG_ERROR("Failed to read config file");
	}
}

template <typename Buffer>
void writeJsonBuffer(const Buffer& buf, const std::string& fileName)
{
	std::string str{};
	auto error = glz::write_file_json(buf, "data/config.json", str);

	if (error.ec != glz::error_code::none)
	{
		TC_LOG_ERROR("Failed to write config file");
	}
}