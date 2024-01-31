#include <world/world.hpp>
#include <world/object.hpp>
#include <render/camera.hpp>

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
	world* world = game::getWorld();
	uint* index = world->cameraObjectIndex;
	uint objNum = world->cameraObjNum;
	for (uint i = 0; i < objNum; ++i)
	{
		object* obj = world->objects + index[i];
		obj->sendMat(cbv);
	}

}

camera* world::getMainCam() const
{
	return mainCamera;
}

void world::setMainCamera(camera* cam)
{
	cam->setCamAsMain();
	this->mainCamera = cam;
}

void world::guiSetting()
{
	for (uint i = 0; i < objectNum; ++i)
	{
		objects[i].guiSetting();
	}
}


bool world::init()
{
	camera* camPtr = new camera();
	camPtr->init();
	setMainCamera(camPtr);
	cameras.push_back(camPtr);

	camera* debugCamPtr = new camera();
	debugCamPtr->init();
	cameras.push_back(debugCamPtr);

	objects = new object[MAX_OBJECTS];

	setupScene();

	return true;
}

void world::update(float dt)
{
	for (uint i = 0; i < objectNum; ++i)
	{
		objects[i].update(dt);
	}

	//if (input::isTriggered(input::KEY_SPACE))
	//{
	//	for (auto cam : cameras)
	//	{
	//		cam->toggleDebugMode();
	//	}
	//}

	cameraObjNum = 0;

	//{
	//	for (uint i = 0; i < objectNum; ++i)
	//	{
	//		if ((objects[i].drawThisPSO(pso::PSO_PBR) == false) || (objects[i].frustumCulling(mainCamera) == true)) continue;

	//		cameraObjectIndex[cameraObjNum] = i;
	//		++cameraObjNum;
	//	}
	//}

	for (auto cam : cameras)
	{
		//debug camera should not be updated when it is not debug mode
		cam->update(dt);
	}
}

void world::close()
{
	for (uint i = 0; i < objectNum; ++i)
	{
		objects[i].close();
	}
	delete[]objects;

	for (auto cam : cameras)
	{
		cam->close();
		delete cam;
	}
}

void world::setupScene()
{
	//{
	//	objects[objectNum].init(msh::MESH_SPHERE, pso::PSO_PBR, true);
	//	objects[objectNum].getTransform()->setPosition(DirectX::XMVECTOR{ 0.0f,-0.0f,0.0f });
	//	objects[objectNum].getTransform()->setScale(DirectX::XMVECTOR{ 0.2f,0.2f,0.2f });

	//	++objectNum;
	//}

	//{
	//	objects[objectNum].init(msh::MESH_BUNNY, pso::PSO_PBR, true);
	//	objects[objectNum].getTransform()->setPosition(DirectX::XMVECTOR{ -0.4f,0.1f,0.0f });
	//	objects[objectNum].getTransform()->setScale(DirectX::XMVECTOR{ 0.4f,0.4f,0.4f });
	//
	//	++objectNum;
	//}

	//for (int i = 0; i < 49; ++i)
	//{
	//	{
	//		objects[objectNum].init(msh::MESH_BUNNY, pso::PSO_PBR, true);
	//		objects[objectNum].getTransform()->setPosition(DirectX::XMVECTOR{ -1.5f + (i / 7) * 0.6f,0.1f + (i % 7) * 0.6f,-1.5f });
	//		objects[objectNum].getTransform()->setScale(DirectX::XMVECTOR{ 0.5f,0.5f,0.5f });

	//		++objectNum;
	//	}
	//}

	//{
	//	objects[objectNum].init(msh::MESH_ARMADILLO, pso::PSO_PBR, true);
	//	objects[objectNum].getTransform()->setPosition(DirectX::XMVECTOR{ 0.4f,0.1f,0.0f });
	//	objects[objectNum].getTransform()->setScale(DirectX::XMVECTOR{ 0.4f,0.4f,0.4f });
	//	objects[objectNum].getTransform()->setRotation(DirectX::XMVECTOR{ 0.0f,3.141592f,0.0f });
	//		
	//	++objectNum;
	//}
	//
	//{
	//	objects[objectNum].init(msh::MESH_ARMADILLO, pso::PSO_PBR, true);
	//	objects[objectNum].getTransform()->setPosition(DirectX::XMVECTOR{ 0.0f,0.5f,-0.5f });
	//	objects[objectNum].getTransform()->setScale(DirectX::XMVECTOR{ 1.2f,1.2f,1.2f });
	//	objects[objectNum].getTransform()->setRotation(DirectX::XMVECTOR{ 0.0f,3.141592f,0.0f });
	//	
	//	++objectNum;
	//}

	//{
	//	objects[objectNum].init(msh::MESH_CUBE_NONORM, pso::PSO_SKYBOX, false);
	//	objects[objectNum].disableWire();
	//	
	//	++objectNum;
	//}
}
