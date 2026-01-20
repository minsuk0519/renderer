#pragma once

#include <system/defines.hpp>
#include <system/config.hpp>

#include <vector>
#include <string>
#include <d3dx12.h>
#include <wrl.h>

class camera;
class object;
class world;

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

	uint submitObjects(void* cbvLoc);
	void uploadObjectViewInfo(void* cbvLoc);
	void boundData(void* cbvLoc);

	void setupScene();
	void setupCam(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, bool forceMain, bool forceFull);

	void guiSetting();

	uint envIndex = 0;
	uint preEnvIndex = envIndex;

	object* getObjects();

	void sendInfo(unsigned char* cbv);

	uint objectNum = 0;

	//index of object that is in camera frustum
	uint cameraObjectIndex[MAX_OBJECTS] = { 0 };
	uint cameraObjNum[DRAW_PASS_NUM];

	object* objects;

	camera* getMainCam() const;

	void instanceCulling();
#if ENGINE_DEBUG_DEBUGCAM
	void updateDebugCamera(float dt);
#endif // #if ENGINE_DEBUG_DEBUGCAM
protected:
	void setMainCamera(camera* cam);

private:
	camera* mainCamera = nullptr;

#if ENGINE_DEBUG_DEBUGCAM
	camera* debugCamera = nullptr;
#endif // #if ENGINE_DEBUG_DEBUGCAM
};

extern world e_globWorld;
