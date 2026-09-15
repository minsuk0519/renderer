#pragma once

#include <render\buffer.hpp>
#include <render\transform.hpp>
#include <render\descriptorheap.hpp>
#include <render\mesh.hpp>

#include <vector>
#include <string>

#include <wrl.h>

class object;
class mesh;
struct descriptor;
class camera;

namespace obj
{
	object* getObject();
}

class object
{
private:
	transform* trans = nullptr;
	buffer* cbv = nullptr;

	mesh* meshPtr = nullptr;
	uint meshEnumIndex;

	float metal = 0.5f;
	float roughness = 0.5f;

	DirectX::XMFLOAT4 albedo = DirectX::XMFLOAT4{ 1,1,1,1 };

	uint lod = 0;
	uint id = 0;
	uint pso = 0;

	std::string name;

	bool visibility = false;
public:
	transform* getTransform() const;

	bool init(const msh::MESH_INDEX meshIdx, const uint psoIndex);
	void update(float dt);
	void submit(void* cbvLoc, uint localID);

	void uploadViewInfo(unsigned char* dataLoc);
	void uploadMaterial(unsigned char* dataLoc);
	void boundData(unsigned char* data);

	void close();

	void guiSetting();

	mesh* getMesh() const;

	void setMaterial(float m, float r);
	void setAlbedo(float r, float g, float b);

	void setName(const std::string& n);
	const std::string& getName() const;

	void setLOD(uint l);
	uint getLOD() const;

	uint getPSO() const;

	float getMetal() const;
	float getRoughness() const;
	const DirectX::XMFLOAT4& getAlbedo() const;

	uint64_t getCBVLoc() const;
	uint getMeshIdx() const;

	uint getObjID() const;

	bool instanceCulling(DirectX::XMVECTOR* frustum);

	bool wasVisible() const;
	void updateVisibility(bool vis);
};