#pragma once

#include <render\buffer.hpp>
#include <render\transform.hpp>
#include <render\descriptorheap.hpp>
#include <render\mesh.hpp>

#include <vector>

#include <wrl.h>

class object;
class mesh;
struct descriptor;
class camera;
class commandqueue;

namespace obj
{
	object* getObject();
}

class object
{
private:
	transform* trans = nullptr;
	constantbuffer* cbv = nullptr;
	descriptor desc = {};

	mesh* meshPtr = nullptr;
	uint meshEnumIndex;

	float metal = 0.5f;
	float roughness = 0.5f;

	DirectX::XMFLOAT4 albedo = DirectX::XMFLOAT4{ 1,1,1,1 };

	uint id = 0;
	uint pso = 0;

	bool displayUI = true;
	bool displayWire = true;
public:

	transform* getTransform() const;

	bool init(const msh::MESH_INDEX meshIdx, const uint psoIndex, bool gui);
	void draw(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, commandqueue* cmdQueue, bool debugDraw);
	void update(float dt);

	void sendMat(unsigned char* cbvdata);

	void close();

	void guiSetting();

	void disableWire();

	mesh* getMesh() const;

	void setMaterial(float m, float r);
	void setAlbedo(float r, float g, float b);

	uint64_t getCBVLoc() const;
	uint getMeshIdx() const;

	bool frustumCulling(camera* cam);

	bool frstumCulled = false;

	uint getObjID() const;
};