#include <world/world.hpp>
#include <world/world_load.hpp>
#include <world/object.hpp>
#include <render/camera.hpp>
#include <render/mesh.hpp>
#include <render/pipelinestate.hpp>
#include <render/shader_defines.hpp>
#include <system/gui.hpp>
#include <system/input.hpp>

#include <cstring>

world e_globWorld;

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
	for (uint i = 0; i < MAX_OBJECTS; ++i)
	{
		objects[i].close();
	}
	delete[]objects;

	mainCamera->close();
	delete mainCamera;

#if ENGINE_DEBUG_DEBUGCAM
	debugCamera->close();
	delete debugCamera;
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
		memcpy(dst + i * instlist::CULL_INSTANCE_BYTES, &rec, sizeof(rec));
	}

	return objectNum;
}

uint world::writeInstanceBounds(unsigned char* dst) const
{
	for (uint i = 0; i < objectNum; ++i)
	{
		objects[i].boundData(dst + i * 6 * sizeof(float));
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
	bool ok = worldload::loadMap(filePath, *this);
	selectedObjectIndex = 0;
	return ok;
}

bool world::saveCurrentMap(const std::string& filePath)
{
	return worldload::saveMap(filePath, *this);
}

void world::clearMap()
{
	objectNum = 0;
	mapName.clear();
	selectedObjectIndex = 0;
}

void world::setupCam(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, bool forceMain, bool forceFull)
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
