#pragma once

#include <system/defines.hpp>

#include <vector>
#include <string>

class camera;
class object;
class world;

constexpr uint MAX_OBJECTS = 256;

namespace game
{
	world* getWorld();
}

enum ENVTEXTUREINDEX
{
	ENVTEXTURE_BRIDGE,
	ENVTEXTURE_SIERRA,
	ENVTEXTURE_APT,
	ENVTEXTURE_ARCHES,
	ENVTEXTURE_ROOFTOP,
	ENVTEXTURE_FACTORY,
	ENVTEXTURE_MONVALLEY,
	ENVTEXTURE_NATURELAB,
	ENVTEXTURE_NEWPORT,
	ENVTEXTURE_SUNRISE,
	ENVTEXTURE_MONUMENTVALLEY,
	ENVTEXTURE_SUMMIPOOL,
	ENVTEXTURE_BEACH,
	ENVTEXTURE_END,
};

class world
{
public:
	bool init();
	void update(float dt);
	void close();

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

protected:
	void setMainCamera(camera* cam);

private:
	std::vector<camera*> cameras;
	camera* mainCamera = nullptr;

	bool active = false;

	void addObject(object* obj);
};