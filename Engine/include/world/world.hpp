#pragma once

#include <system/defines.hpp>

#include <vector>
#include <string>
#include <d3dx12.h>
#include <wrl.h>

class camera;
class object;
class world;

constexpr uint THREADS_NUM_CLUSTERS = 64;
constexpr uint MAX_OBJECTS = 256;
constexpr uint MAX_CLUSTERS = 1024 * MAX_OBJECTS;
constexpr uint MAX_LODS = 8;

namespace game
{
	world* getWorld();
}

class world
{
public:
	bool init();
	void update(float dt);
	void close();

	void drawWorld(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, bool wireframe);
	uint drawObject(void* cbvLoc);

	void setupScene();

	void guiSetting();

	uint envIndex = 0;
	uint preEnvIndex = envIndex;

	object* getObjects();

	void sendInfo(unsigned char* cbv);

	uint objectNum = 0;

	//index of object that is in camera frustum
	uint cameraObjectIndex[MAX_OBJECTS] = { 0 };
	uint cameraObjNum = 0;

	object* objects;

	camera* getMainCam() const;

	void submitTerrainData(unsigned char* loc) const;
protected:
	void setMainCamera(camera* cam);

private:
	std::vector<camera*> cameras;
	camera* mainCamera = nullptr;

	//this camera will not be actually used
	//is for rendering gui
	camera* debugCamera = nullptr;

	bool active = false;

	void addObject(object* obj);
};

extern world e_globWorld;
