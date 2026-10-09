#include <world/world_load.hpp>
#include <world/world.hpp>
#include <world/object.hpp>
#include <render/transform.hpp>
#include <render/mesh.hpp>
#include <render/shader_defines.hpp>
#include <system/jsonhelper.hpp>
#include <system/logger.hpp>
#include <system/config.hpp>
#include <format>

bool worldload::parseMapFile(const std::string& filePath, worldMapJson& outMap)
{
	if (filePath.empty())
	{
		TC_LOG_ERROR("Failed to read file : empty file path");
		return false;
	}

	if (!readJsonFileOpts<glz::opts{.error_on_unknown_keys = false}>(outMap, filePath))
	{
		return false;
	}

	if (outMap.version != MAP_VERSION)
	{
		TC_LOG_WARNING(std::format("Map version {} does not match expected version {}", outMap.version, MAP_VERSION).c_str());
	}

	return true;
}

bool worldload::writeMapFile(const std::string& filePath, const worldMapJson& map)
{
	if (filePath.empty())
	{
		TC_LOG_ERROR("Failed to write file : empty file path");
		return false;
	}

	return writeJsonFileOpts<glz::opts{.prettify = true}>(map, filePath);
}

bool worldload::loadMap(const std::string& filePath, world& target)
{
	worldMapJson mapData;

	if (!parseMapFile(filePath, mapData))
	{
		return false;
	}

	if (mapData.entities.size() > MAX_OBJECTS)
	{
		TC_LOG_WARNING(std::format("Map entity count {} exceeds MAX_OBJECTS {}, clamping to maximum", mapData.entities.size(), MAX_OBJECTS).c_str());
	}

	target.objectNum = 0;

	for (size_t i = 0; i < mapData.entities.size() && i < MAX_OBJECTS; ++i)
	{
		const mapEntityJson& entity = mapData.entities[i];

		// Validate indices before use
		if (entity.mesh >= msh::MESH_END)
		{
			TC_LOG_ERROR(std::format("Entity '{}': mesh index {} out of range", entity.name, entity.mesh).c_str());
			continue;
		}

		if (entity.pso >= render::PSO_END)
		{
			TC_LOG_ERROR(std::format("Entity '{}': PSO index {} out of range", entity.name, entity.pso).c_str());
			continue;
		}

		object* obj = target.objects + target.objectNum;

		obj->reset(static_cast<msh::MESH_INDEX>(entity.mesh), entity.pso);
		obj->setName(entity.name);
		obj->setLOD(entity.lod);
		obj->setMaterial(entity.material.metal, entity.material.roughness);
		obj->setAlbedo(entity.material.albedo[0], entity.material.albedo[1], entity.material.albedo[2]);

		// Set transform
		obj->getTransform()->setPosition(DirectX::XMVECTOR{ entity.transform.position[0], entity.transform.position[1], entity.transform.position[2] });
		obj->getTransform()->setScale(DirectX::XMVECTOR{ entity.transform.scale[0], entity.transform.scale[1], entity.transform.scale[2] });
		obj->getTransform()->setRotation(DirectX::XMVECTOR{ entity.transform.rotation[0], entity.transform.rotation[1], entity.transform.rotation[2] });

		++target.objectNum;
	}

	target.mapName = mapData.name;

	TC_LOG(std::format("Loaded map '{}' with {} entities", mapData.name, target.objectNum).c_str());

	return target.objectNum > 0;
}

bool worldload::saveMap(const std::string& filePath, const world& source)
{
	worldMapJson mapData;
	mapData.version = MAP_VERSION;
	mapData.name = source.mapName;

	for (uint i = 0; i < source.objectNum; ++i)
	{
		const object* obj = source.objects + i;

		mapEntityJson entity;
		entity.id = i;
		entity.name = obj->getName();
		entity.mesh = obj->getMeshIdx();
		entity.pso = obj->getPSO();
		entity.lod = obj->getLOD();

		// Material
		entity.material.metal = obj->getMetal();
		entity.material.roughness = obj->getRoughness();
		const DirectX::XMFLOAT4& albedo = obj->getAlbedo();
		entity.material.albedo[0] = albedo.x;
		entity.material.albedo[1] = albedo.y;
		entity.material.albedo[2] = albedo.z;

		// Transform
		DirectX::XMVECTOR pos = obj->getTransform()->getPosition();
		entity.transform.position[0] = pos.m128_f32[0];
		entity.transform.position[1] = pos.m128_f32[1];
		entity.transform.position[2] = pos.m128_f32[2];

		DirectX::XMVECTOR scale = obj->getTransform()->getScale();
		entity.transform.scale[0] = scale.m128_f32[0];
		entity.transform.scale[1] = scale.m128_f32[1];
		entity.transform.scale[2] = scale.m128_f32[2];

		DirectX::XMVECTOR rot = obj->getTransform()->getRotation();
		entity.transform.rotation[0] = rot.m128_f32[0];
		entity.transform.rotation[1] = rot.m128_f32[1];
		entity.transform.rotation[2] = rot.m128_f32[2];

		mapData.entities.push_back(entity);
	}

	return writeMapFile(filePath, mapData);
}
