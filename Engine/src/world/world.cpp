#include <world/world.hpp>
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

void world::sendInfo(unsigned char* cbv)
{
	uint* index = cameraObjectIndex;
	uint objNum = cameraObjNum[0];
	for (uint i = 0; i < objNum; ++i)
	{
		object* obj = objects + index[i];
		obj->sendMat(cbv);
	}

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
			//if (obj->wasVisible())
			{
				cameraObjectIndex[cameraObjNum[0]] = i;
				++cameraObjNum[0];
			}
			//second pass
			//else
			//{
			//	cameraObjectIndex[MAX_OBJECTS - 1 - cameraObjNum[1]] = i;
			//	++cameraObjNum[1];
			//}

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
	debugCamera->update(dt);
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
}

void world::boundData(void* cbvLoc)
{
	unsigned char* gpuAddress = reinterpret_cast<unsigned char*>(cbvLoc);
	for (uint i = 0; i < cameraObjNum[0]; ++i)
	{
		objects[cameraObjectIndex[i]].boundData(gpuAddress);
		gpuAddress += 6 * sizeof(float);
	}
}

void world::setupScene()
{
	//TODO
	{
		objects[objectNum].init(msh::MESH_CUBE, render::PSO_PBR);
		objects[objectNum].getTransform()->setPosition(DirectX::XMVECTOR{ -1.0f,-0.5f,0.0f });
		objects[objectNum].getTransform()->setScale(DirectX::XMVECTOR{ 0.2f,0.2f,0.2f });

		++objectNum;
	}

	{
		objects[objectNum].init(msh::MESH_BUNNY, render::PSO_PBR);
		objects[objectNum].getTransform()->setPosition(DirectX::XMVECTOR{ 0.0f,-0.5f,0.0f });
		objects[objectNum].getTransform()->setScale(DirectX::XMVECTOR{ 5.0f,5.0f,5.0f });

		++objectNum;
	}

	{
		objects[objectNum].init(msh::MESH_SPHERE, render::PSO_PBR);
		objects[objectNum].getTransform()->setPosition(DirectX::XMVECTOR{ 1.0f,-0.5f,0.0f });
		objects[objectNum].getTransform()->setScale(DirectX::XMVECTOR{ 0.005f,0.005f,0.005f });

		++objectNum;
	}

	{
		objects[objectNum].init(msh::MESH_TERRAIN, render::PSO_PBR);
		objects[objectNum].getTransform()->setPosition(DirectX::XMVECTOR{ 0.0f,-2.0f,0.0f });
		objects[objectNum].getTransform()->setScale(DirectX::XMVECTOR{ 0.2f,0.2f,0.2f });

		++objectNum;
	}
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
