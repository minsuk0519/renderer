#pragma once

#include <render\buffer.hpp>

#include <wrl.h>
#include <d3d12.h>
#include <vector>

class mesh;

namespace msh
{
	enum MESH_INDEX
	{
		MESH_CUBE = 0,
		MESH_SPHERE,
		MESH_BUNNY,
		MESH_TERRAIN,
		MESH_END,
		MESH_SCENE_TRIANGLE = MESH_END,
		MESH_SIZE,
	};

	static constexpr const char* MESHNAME[] = {
		"Cube",
		"Sphere",
		"Bunny",
		"Terrain",
	};

	bool loadResources();
	void cleanUp();

	void setUpTerrain(vertexbuffer* vertex, vertexbuffer* n, indexbuffer* index);

	mesh* getMesh(const uint& idx);
	mesh* getMesh(const MESH_INDEX idx);

	enum AXIS_ENUM
	{
		AXIS_X = 0,
		AXIS_Y,
		AXIS_Z,
		AXIS_MAX,
	};

	void guiMeshSetting(bool& meshWindow, uint& meshID);
}

struct lodInfos
{
	//how many clusters in lod
	uint clusterNum;
	//number of indices in lods
	uint totalIndicesCount;
	std::vector<uint> indexSize;
};

struct aabbbound
{
	float center[3];
	float hExtent[3];
};

struct spherebound
{
	float radius;
	float center[3];
};

struct clusterbounddata
{
	spherebound sphere;
	aabbbound aabb;
};

struct bound
{
	float radius;
	//half extent
	float halfExtent[msh::AXIS_MAX];
};

struct meshData
{
	uint lodNum = 1;
	std::vector<lodInfos> lodData;

	bound boundData;

	std::vector<clusterbounddata> clusterBounds;
};

class mesh
{
private:
	meshData* data = nullptr;

	int ID = 0;
	uint indexCount = 0;

public:
	mesh(int index);

	bool init(meshData* meshdata);
	void close();

	int getId() const;

	meshData* getData() const;

	void setBuffer(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, bool lineDraw);
	void draw(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList);
};