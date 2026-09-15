#include <world/world.hpp>
#include <world/world_load.hpp>
#include <world/object.hpp>
#include <render/camera.hpp>
#include <render/pipelinestate.hpp>
#include <system/gui.hpp>
#include <system/input.hpp>

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

void world::instanceCulling()
{
	mainCamera->updateView();

	for (uint i = 0; i < objectNum; ++i)
	{
		object* obj = objects + i;

		DirectX::XMVECTOR* frustum = mainCamera->getFrustum();

		bool vis = false;

		if (obj->instanceCulling(frustum))
		{
			//if drawn previous frame, we will put it on the first pass
			//TODO : some obj will be drawn first pass no matter if they are drawn previous frame
			if (obj->wasVisible())
			{
				cameraObjectIndex[cameraObjNum[0]] = i;
				++cameraObjNum[0];
			}
			//second pass
			else
			{
				cameraObjectIndex[MAX_OBJECTS - 1 - cameraObjNum[1]] = i;
				++cameraObjNum[1];
			}

			vis = true;

			TC_ASSERT((cameraObjNum[0] + cameraObjNum[1]) < MAX_OBJECTS);
		}

		obj->updateVisibility(vis);
	}
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

void world::guiSetting()
{
	static uint objectGUIIndex;
	ImGui::BeginChild("left pane", ImVec2(50, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);

	for (uint i = 0; i < objectNum; ++i)
	{
		if(ImGui::Button(("objects##" + std::to_string(i)).c_str()))
		{
			objectGUIIndex = i;
		}
	}

	if (ImGui::Button("Save Level"))
	{
		saveCurrentMap(worldload::DEFAULT_LEVEL_PATH);
	}

	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild("object view pane", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));

	objects[objectGUIIndex].guiSetting();

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

	setupScene();

	for (uint i = 0; i < DRAW_PASS_NUM; ++i)
	{
		cameraObjNum[i] = 0;
	}

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

	for (uint i = 0; i < DRAW_PASS_NUM; ++i)
	{
		cameraObjNum[i] = 0;
	}

	mainCamera->update(dt);
#if ENGINE_DEBUG_DEBUGCAM
	debugCamera->update(dt);
#endif // #if ENGINE_DEBUG_DEBUGCAM
}

void world::close()
{
	for (uint i = 0; i < objectNum; ++i)
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

uint world::submitObjects(void* cbvLoc)
{
	uint* location = static_cast<uint*>(cbvLoc);
	for (uint i = 0; i < cameraObjNum[0]; ++i)
	{
		objects[cameraObjectIndex[i]].submit(static_cast<void*>(location), i);
		location += 1;
	}

	for (uint j = 0; j < cameraObjNum[1]; ++j)
	{
		objects[cameraObjectIndex[MAX_OBJECTS - 1 - j]].submit(static_cast<void*>(location), cameraObjNum[0] + j);
		location += 1;
	}

	return cameraObjNum[0];
}

void world::uploadObjectInfo(void* viewInfoLoc, void* materialLoc)
{
	unsigned char* viewInfoGpuAddress = reinterpret_cast<unsigned char*>(viewInfoLoc);
	unsigned char* materialGpuAddress = reinterpret_cast<unsigned char*>(materialLoc);
	for (uint i = 0; i < cameraObjNum[0]; ++i)
	{
		objects[cameraObjectIndex[i]].uploadViewInfo(viewInfoGpuAddress);
		objects[cameraObjectIndex[i]].uploadMaterial(materialGpuAddress);
		viewInfoGpuAddress += sizeof(uint) * 10;
		materialGpuAddress += sizeof(uint) * 5;
	}

	for (uint j = 0; j < cameraObjNum[1]; ++j)
	{
		objects[cameraObjectIndex[MAX_OBJECTS - 1 - j]].uploadViewInfo(viewInfoGpuAddress);
		objects[cameraObjectIndex[MAX_OBJECTS - 1 - j]].uploadMaterial(materialGpuAddress);
		viewInfoGpuAddress += sizeof(uint) * 10;
		materialGpuAddress += sizeof(uint) * 5;
	}
}

void world::boundData(void* cbvLoc)
{
	unsigned char* gpuAddress = reinterpret_cast<unsigned char*>(cbvLoc);
	for (uint i = 0; i < cameraObjNum[0]; ++i)
	{
		objects[cameraObjectIndex[i]].boundData(gpuAddress);
		gpuAddress += 6 * sizeof(float);
	}

	for (uint j = 0; j < cameraObjNum[1]; ++j)
	{
		objects[cameraObjectIndex[MAX_OBJECTS - 1 - j]].boundData(gpuAddress);
		gpuAddress += 6 * sizeof(float);
	}
}

void world::setupScene()
{
	if (!worldload::loadMap(worldload::DEFAULT_LEVEL_PATH, *this))
	{
		TC_LOG_ERROR("Failed to load map from data/level.json");
	}
}

bool world::saveCurrentMap(const std::string& filePath)
{
	return worldload::saveMap(filePath, *this);
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
