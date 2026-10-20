#pragma once

#include <system/defines.hpp>
#include <system/config.hpp>

#include <string>
#include <d3dx12.h>
#include <wrl.h>
#include <DirectXMath.h>

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

	uint writeCullInstanceList(unsigned char* dst) const;
	void uploadObjectInfo(void* viewInfoLoc, void* materialLoc);
	uint writeInstanceBounds(unsigned char* dst) const;

	void setupScene();
	void setupCam(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, bool forceMain, bool forceFull);

	void guiLevelEditor();
	bool loadCurrentMap(const std::string& filePath);
	bool saveCurrentMap(const std::string& filePath);
	void clearMap();

	uint envIndex = 0;
	uint preEnvIndex = envIndex;

	object* getObjects();

	uint objectNum = 0;
	std::string mapName;

	object* objects;

	camera* getMainCam() const;

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

	uint selectedObjectIndex = 0;
};

extern world e_globWorld;
