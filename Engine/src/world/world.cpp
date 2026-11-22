#include <world/world.hpp>
#include <world/world_load.hpp>
#include <world/object.hpp>
#include <render/camera.hpp>
#include <render/mesh.hpp>
#include <render/pipelinestate.hpp>
#include <render/shader_defines.hpp>
#include <system/gui.hpp>
#include <system/input.hpp>
#include <system/logger.hpp>
#include <world/spatial_hierarchy.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <fstream>
#include <limits>
#include <thread>
#include <vector>

world e_globWorld;

namespace
{
	void sghAssertHandler(const char* expr, const char* file, int line)
	{
		TC_LOG_ERROR(std::format("[SpatialHierarchy] invariant failed: {} ({}:{})", expr, file, line).c_str());
		TC_ASSERT(false);
	}

	void computeWorldAabb(const DirectX::XMFLOAT4X4& m, const float* h, double outMin[3], double outMax[3])
	{
		for (int c = 0; c < 3; ++c)
		{
			const double ext = std::fabs(static_cast<double>(m.m[0][c])) * static_cast<double>(h[msh::AXIS_X])
				+ std::fabs(static_cast<double>(m.m[1][c])) * static_cast<double>(h[msh::AXIS_Y])
				+ std::fabs(static_cast<double>(m.m[2][c])) * static_cast<double>(h[msh::AXIS_Z]);
			outMin[c] = static_cast<double>(m.m[3][c]) - ext;
			outMax[c] = static_cast<double>(m.m[3][c]) + ext;
		}
	}

	std::vector<sgh::InstanceIn> gatherInstances(object* objects, uint objectNum)
	{
		std::vector<sgh::InstanceIn> instances(objectNum);
		for (uint i = 0; i < objectNum; ++i)
		{
			sgh::InstanceIn& in = instances[i];
			in.key = i;

			DirectX::XMFLOAT4X4 m{};
			DirectX::XMStoreFloat4x4(&m, objects[i].getTransform()->getMat());
			computeWorldAabb(m, objects[i].getMesh()->getData()->boundData.halfExtent, in.worldMin, in.worldMax);
		}
		return instances;
	}

	bool writeHierarchyBytes(const std::string& filePath, const std::vector<uint8_t>& bytes)
	{
		std::ofstream f(filePath, std::ios::binary | std::ios::trunc);
		if (!f.is_open())
		{
			TC_LOG_ERROR(std::format("[SpatialHierarchy] failed to open '{}' for writing", filePath).c_str());
			return false;
		}
		f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
		return true;
	}

	void logBuildStats(const sgh::BuildResult& r)
	{
		uint64_t totalLeaves = 0;
		uint64_t totalAreaSum = 0;
		uint64_t totalGroupAreaSum = 0;
		for (const sgh::LevelStats& ls : r.stats.levels)
		{
			const uint32_t poolSize = ls.roundStats.empty() ? 0u : ls.roundStats.front().poolSize;
			uint32_t maxBlockPop = 0;
			uint64_t poolSum = 0;
			uint64_t blockSum = 0;
			uint64_t aabbTests = 0;
			uint64_t pairEvals = 0;
			for (const sgh::RoundStats& rs : ls.roundStats)
			{
				maxBlockPop = (std::max)(maxBlockPop, rs.maxBlockPop);
				poolSum += rs.poolSize;
				blockSum += rs.blockCount;
				aabbTests += rs.aabbTests;
				pairEvals += rs.pairEvals;
			}

			std::string occupancy;
			if (blockSum > 0)
			{
				occupancy = std::format(", block occupancy max {} mean {:.2f}", maxBlockPop, static_cast<double>(poolSum) / static_cast<double>(blockSum));
			}

			std::string fill;
			for (uint32_t k = 0; k <= sgh::GROUP_MAX; ++k)
			{
				if (ls.fill[k] != 0)
				{
					if (!fill.empty())
					{
						fill += ' ';
					}
					fill += std::format("{}:{}", k, ls.fill[k]);
				}
			}

			TC_LOG(std::format("[SpatialHierarchy] level exp {}: pool {}, {} groups, {} leaves, {} rounds{}, aabb tests {}, pair evals {}, area sum {}, group area sum {}, fill [{}], merge {:.2f} ms",
				ls.exp, poolSize, ls.groupCount, ls.leafCount, ls.rounds, occupancy, aabbTests, pairEvals, ls.areaSum, ls.groupAreaSum, fill, ls.mergeMs).c_str());

			totalLeaves += ls.leafCount;
			totalAreaSum += ls.areaSum;
			totalGroupAreaSum += ls.groupAreaSum;
		}

		TC_LOG(std::format("[SpatialHierarchy] threads {}: groups {}, leaves {}, area sum {}, group area sum {}, sweep {:.2f} ms, bvh {:.2f} ms, total {:.2f} ms, hash {:016x}",
			r.stats.threadCount, r.stats.groupCount, totalLeaves, totalAreaSum, totalGroupAreaSum, r.stats.sweepMs, r.stats.bvhMs, r.stats.totalMs, r.contentHash).c_str());
	}
}

namespace game
{
	static world* worldPtr = nullptr;

	world* getWorld()
	{
		if (worldPtr == nullptr) worldPtr = new world();

		return worldPtr;
	}
}

object* world::getObjects()
{
	return objects;
}

camera* world::getMainCam() const
{
	return mainCamera;
}

#if ENGINE_DEBUG_DEBUGCAM
void world::updateDebugCamera(float dt)
{
	debugCamera->update(dt);
}
#endif // #if ENGINE_DEBUG_DEBUGCAM

void world::setMainCamera(camera* cam)
{
	cam->setCamAsMain();
	this->mainCamera = cam;
}

void world::guiLevelEditor()
{
	ImGui::BeginChild("left pane", ImVec2(250, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);

	if (ImGui::Button("Load Map"))
	{
		loadCurrentMap(worldload::DEFAULT_LEVEL_PATH);
	}

	ImGui::SameLine();

	if (ImGui::Button("Save Map"))
	{
		saveCurrentMap(worldload::DEFAULT_LEVEL_PATH);
	}

	ImGui::SameLine();

	if (ImGui::Button("Build Spatial Hierarchy"))
	{
		buildSpatialHierarchy(worldload::DEFAULT_SPATIAL_HIERARCHY_PATH);
	}

	ImGui::SameLine();

	if (ImGui::Button("Clear Map"))
	{
		clearMap();
	}

	ImGui::Separator();

	for (uint i = 0; i < objectNum; ++i)
	{
		std::string displayName = objects[i].getName().empty() ? "<unnamed>" : objects[i].getName();
		displayName += "##" + std::to_string(i);

		if (ImGui::Selectable(displayName.c_str(), i == selectedObjectIndex))
		{
			selectedObjectIndex = i;
		}
	}

	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild("object view pane", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));

	if (selectedObjectIndex < objectNum)
	{
		objects[selectedObjectIndex].guiSetting();
	}
	else
	{
		gui::text("No entity selected");
	}

	ImGui::EndChild();
}


bool world::init()
{
	camera* camPtr = new camera();
	camPtr->init();
	setMainCamera(camPtr);

#if ENGINE_DEBUG_DEBUGCAM
	camera* debugCamPtr = new camera();
	debugCamPtr->init();
	debugCamPtr->toggleDebugMode();
	debugCamera = debugCamPtr;
#endif // #if ENGINE_DEBUG_DEBUGCAM

	objects = new object[MAX_OBJECTS];

	for (uint i = 0; i < MAX_OBJECTS; ++i)
	{
		objects[i].allocate();
	}

	setupScene();

	return true;
}

void world::update(float dt)
{
	//for (uint i = 0; i < objectNum; ++i)
	//{
	//	objects[i].update(dt);
	//}

#if ENGINE_DEBUG_DEBUGCAM
	if (input::isTriggered(input::KEY_SPACE))
	{
		mainCamera->toggleDebugMode();
		debugCamera->toggleDebugMode();
	}
#endif // #if ENGINE_DEBUG_DEBUGCAM

	mainCamera->update(dt);
#if ENGINE_DEBUG_DEBUGCAM
	debugCamera->update(dt);
#endif // #if ENGINE_DEBUG_DEBUGCAM
}

void world::close()
{
	if (objects != nullptr)
	{
		for (uint i = 0; i < MAX_OBJECTS; ++i)
		{
			objects[i].close();
		}
		delete[] objects;
		objects = nullptr;
	}
	clearMap();

	if (mainCamera != nullptr)
	{
		mainCamera->close();
		delete mainCamera;
		mainCamera = nullptr;
	}

#if ENGINE_DEBUG_DEBUGCAM
	if (debugCamera != nullptr)
	{
		debugCamera->close();
		delete debugCamera;
		debugCamera = nullptr;
	}
#endif // #if ENGINE_DEBUG_DEBUGCAM
}

void world::uploadObjectInfo(void* viewInfoLoc, void* materialLoc)
{
	unsigned char* viewInfoGpuAddress = reinterpret_cast<unsigned char*>(viewInfoLoc);
	unsigned char* materialGpuAddress = reinterpret_cast<unsigned char*>(materialLoc);

	for (uint i = 0; i < objectNum; ++i)
	{
		objects[i].uploadViewInfo(viewInfoGpuAddress);
		objects[i].uploadMaterial(materialGpuAddress);

		viewInfoGpuAddress += sizeof(uint) * 10;
		materialGpuAddress += sizeof(uint) * 5;
	}
}

uint world::writeCullInstanceList(unsigned char* dst) const
{
	for (uint i = 0; i < objectNum; ++i)
	{
		instlist::cullInstance rec = {};
		memcpy(rec.halfExtent, objects[i].getMesh()->getData()->boundData.halfExtent, sizeof(rec.halfExtent));
		objects[i].submit(&rec.packedID, i);
		memcpy(dst + static_cast<size_t>(i) * instlist::CULL_INSTANCE_BYTES, &rec, sizeof(rec));
	}

	return objectNum;
}

uint world::writeInstanceBounds(unsigned char* dst) const
{
	for (uint i = 0; i < objectNum; ++i)
	{
		objects[i].boundData(dst + static_cast<size_t>(i) * 6 * sizeof(float));
	}

	return objectNum;
}

void world::setupScene()
{
	if (!worldload::loadMap(worldload::DEFAULT_LEVEL_PATH, *this))
	{
		TC_LOG_ERROR("Failed to load map from data/level.json");
	}
}

bool world::loadCurrentMap(const std::string& filePath)
{
	const bool ok = worldload::loadMap(filePath, *this);
	selectedObjectIndex = 0;
	return ok;
}

bool world::saveCurrentMap(const std::string& filePath)
{
	return worldload::saveMap(filePath, *this);
}

bool world::buildSpatialHierarchy(const std::string& filePath)
{
	if (objectNum == 0)
	{
		TC_LOG_WARNING("[SpatialHierarchy] no objects, nothing to build");
		return false;
	}

	const std::vector<sgh::InstanceIn> instances = gatherInstances(objects, objectNum);

	sgh::setAssertHandler(&sghAssertHandler);
	sgh::BuildParams params;
	params.threadCount = (std::max)(1u, std::thread::hardware_concurrency());

	const sgh::BuildResult r = sgh::build(instances, 0, params);
	if (!r.ok)
	{
		TC_LOG_ERROR(std::format("[SpatialHierarchy] build failed: {}", r.error).c_str());
		return false;
	}

	if (!writeHierarchyBytes(filePath, r.bytes))
	{
		return false;
	}

	TC_LOG(std::format("[SpatialHierarchy] wrote '{}': {} instances, {} levels, {} groups, {} nodes, hash {:016x}",
		filePath, r.stats.instanceCount, r.stats.levelCount, r.stats.groupCount, r.stats.nodeCount, r.contentHash).c_str());

	logBuildStats(r);

	return true;
}

void world::clearMap()
{
	objectNum = 0;
	mapName.clear();
	selectedObjectIndex = 0;
}

void world::setupCam(const Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& cmdList, bool forceMain, bool forceFull)
{
	if(forceMain) mainCamera->preDraw(cmdList, forceFull);
#if ENGINE_DEBUG_DEBUGCAM
	else
	{
		//debugCam will be always full
		debugCamera->preDraw(cmdList, true);
	}
#endif // #if ENGINE_DEBUG_DEBUGCAM
}
