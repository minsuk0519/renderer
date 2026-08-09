#pragma once

#include <string>
#include <system/defines.hpp>

#ifdef _DEBUG
#define CONFIG_CAPTURE_ENABLED 1
#define CONFIG_LOG_ENABLED 1
#endif

namespace config
{
	const std::string shaderBasePath = "data/shader/source";
}

#define BUFFER_MAX_SIZE		1073741824
#define BUFFER_MAX_COUNT	16384


#define UVB_MAX_SIZE		402653184
#define UIB_MAX_SIZE		251658240
#define VERTEXID_MAX_SIZE	402653184

#define DRAW_PASS_NUM	2

constexpr uint THREADS_NUM_CLUSTERS = 64;
constexpr uint MAX_OBJECTS = 256;
constexpr uint MAX_CLUSTERS = MAX_OBJECTS * 1024;
constexpr uint MAX_VERTICES = 16777216;

static_assert(MAX_OBJECTS <= (1u << 8), "visID encoding allots 8 bits to objID");
static_assert(MAX_CLUSTERS <= (1u << 18), "visID encoding allots 18 bits to clusterSlot");
static_assert(THREADS_NUM_CLUSTERS <= (1u << 6), "visID encoding allots 6 bits to localTri");

constexpr uint MAX_MESHES = 16;
constexpr uint MAX_MESHES_LOD = 128;
constexpr uint MAX_MESHES_CLUSTERS = 8096;

constexpr uint MAX_LODS = 8;
