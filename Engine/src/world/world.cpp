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
	uint objNum = cameraObjNum;
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

		if (obj->instanceCulling(frustum))
		{
			cameraObjectIndex[cameraObjNum] = i;
			++cameraObjNum;
		}
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
	debugCamera = debugCamPtr;
#endif // #if ENGINE_DEBUG_DEBUGCAM

	objects = new object[MAX_OBJECTS];

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

	cameraObjNum = 0;

	mainCamera->update(dt);
	//debugCamera->update(dt);
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
	uint offset = 0;

	uint* location = static_cast<uint*>(cbvLoc);
	for (uint i = 0; i < cameraObjNum; ++i)
	{
		objects[cameraObjectIndex[i]].submit(static_cast<void*>(location), offset, i);
		location += 1;
	}

	return cameraObjNum;
}

void world::uploadObjectViewInfo(void* cbvLoc)
{
	unsigned char* gpuAddress = reinterpret_cast<unsigned char*>(cbvLoc);
	for (uint i = 0; i < cameraObjNum; ++i)
	{
		objects[cameraObjectIndex[i]].uploadViewInfo(gpuAddress);
		gpuAddress += sizeof(uint) * 10;
	}
}

void world::boundData(void* cbvLoc)
{
	unsigned char* gpuAddress = reinterpret_cast<unsigned char*>(cbvLoc);
	for (uint i = 0; i < cameraObjNum; ++i)
	{
		objects[cameraObjectIndex[i]].boundData(gpuAddress);
		gpuAddress += 6 * sizeof(float);
	}
}

void world::setupScene()
{
	//TODO
	{
		objects[objectNum].init(msh::MESH_CUBE, render::PSO_PBR, true);
		objects[objectNum].getTransform()->setPosition(DirectX::XMVECTOR{ -1.0f,-0.5f,0.0f });
		objects[objectNum].getTransform()->setScale(DirectX::XMVECTOR{ 0.2f,0.2f,0.2f });

		++objectNum;
	}

	{
		objects[objectNum].init(msh::MESH_BUNNY, render::PSO_PBR, true);
		objects[objectNum].getTransform()->setPosition(DirectX::XMVECTOR{ 0.0f,-0.5f,0.0f });
		objects[objectNum].getTransform()->setScale(DirectX::XMVECTOR{ 5.0f,5.0f,5.0f });

		++objectNum;
	}

	{
		objects[objectNum].init(msh::MESH_SPHERE, render::PSO_PBR, true);
		objects[objectNum].getTransform()->setPosition(DirectX::XMVECTOR{ 1.0f,-0.5f,0.0f });
		objects[objectNum].getTransform()->setScale(DirectX::XMVECTOR{ 0.005f,0.005f,0.005f });

		++objectNum;
	}

	{
		objects[objectNum].init(msh::MESH_TERRAIN, render::PSO_PBR, true);
		objects[objectNum].getTransform()->setPosition(DirectX::XMVECTOR{ 0.0f,-2.0f,0.0f });
		objects[objectNum].getTransform()->setScale(DirectX::XMVECTOR{ 0.2f,0.2f,0.2f });

		++objectNum;
	}
}

void world::setupCam(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, bool forceMain)
{
	if(forceMain) mainCamera->preDraw(cmdList);
}
