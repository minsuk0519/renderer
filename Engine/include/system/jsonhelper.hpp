#pragma once

#include <string>

#include <glaze/glaze.hpp>

#include "defines.hpp"
#include "logger.hpp"

struct configJson
{
	uint width;
	uint height;
};

struct shaderJson
{
	std::string shaderFile;
	std::string entryPoint;
	std::string target;
	uint shaderType;
	uint shaderIndex;
};

struct psoJson
{
	uint psoIndex;
	std::string psoName;
	uint vertexIndex;
	uint pixelIndex;
	std::vector<uint> formats;
	bool cs;
	bool depth;
	bool wireframe;
};

enum JSON_FILE_NAME
{
	CONFIG_FILE = 0,
	SHADER_FILE,
	PSO_FILE,
	MAX_FILE,
};

const std::string JSON_FILE_PATHS[MAX_FILE] = {
	"data/config.json",
	"data/shader.json",
	"data/pso.json",
};

//arbitrary buffersize
constexpr uint BUFFERSIZE = 1024 * 1024 * 64;

int rawFileRead(std::string fileName, void* data, uint bufferSize = 0);

template <typename Buffer>
void readJsonBuffer(Buffer& buf, const JSON_FILE_NAME& fileIndex)
{
	std::string str{};
	auto error = glz::read_file_json(buf, JSON_FILE_PATHS[fileIndex], str);

	if (error.ec != glz::error_code::none)
	{
		std::string errorMessage = "Failed to read file : " + JSON_FILE_PATHS[fileIndex];
		TC_LOG_ERROR(errorMessage.c_str());
	}
}

template <typename Buffer>
void writeJsonBuffer(const Buffer& buf, const JSON_FILE_NAME& fileIndex)
{
	std::string str{};
	auto error = glz::write_file_json(buf, JSON_FILE_PATHS[fileIndex], str);

	if (error.ec != glz::error_code::none)
	{
		std::string errorMessage = "Failed to write file : " + JSON_FILE_PATHS[fileIndex];
		TC_LOG_ERROR(errorMessage.c_str());
	}
}