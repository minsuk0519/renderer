#pragma once

#include <string>

#ifdef _DEBUG
#define CONFIG_CAPTURE_ENABLED 1
#define CONFIG_LOG_ENABLED 1
#endif

namespace config
{
	const std::string shaderBasePath = "data/shader/source";
}

#define BUFFER_MAX_SIZE		1048576
#define BUFFER_MAX_COUNT	16384
