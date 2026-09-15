#pragma once

#include <string>
#include <vector>
#include <array>
#include <system/defines.hpp>

class world;

struct mapTransformJson
{
	std::array<float, 3> position{ 0.0f, 0.0f, 0.0f };
	std::array<float, 3> rotation{ 0.0f, 0.0f, 0.0f };
	std::array<float, 3> scale{ 1.0f, 1.0f, 1.0f };
};

struct mapMaterialJson
{
	std::array<float, 3> albedo{ 1.0f, 1.0f, 1.0f };
	float metal = 0.5f;
	float roughness = 0.5f;
};

struct mapEntityJson
{
	uint id = 0;
	std::string name;
	mapTransformJson transform;
	uint mesh = 0;
	uint pso = 0;
	mapMaterialJson material;
	uint lod = 0;
};

struct worldMapJson
{
	uint version = 1;
	std::string name;
	std::vector<mapEntityJson> entities;
};

namespace worldload
{
	constexpr uint MAP_VERSION = 1;
	const std::string DEFAULT_LEVEL_PATH = "data/level.json";

	bool parseMapFile(const std::string& filePath, worldMapJson& outMap);
	bool writeMapFile(const std::string& filePath, const worldMapJson& map);

	bool loadMap(const std::string& filePath, world& target);
	bool saveMap(const std::string& filePath, const world& source);
}
