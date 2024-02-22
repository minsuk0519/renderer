#pragma once

#include <render\buffer.hpp>

#include <wrl.h>
#include <d3d12.h>

class mesh;

namespace msh
{
	enum MESH_INDEX
	{
		MESH_CUBE = 0,
		MESH_SPHERE,
		MESH_BUNNY,
		MESH_END,
		MESH_SCENE_TRIANGLE = MESH_END,
		MESH_SIZE,
	};

	static constexpr const char* MESHNAME[] = {
		"Cube",
		"Sphere",
		"Bunny",
	};

	bool loadResources();
	void cleanUp();

	mesh* getMesh(const MESH_INDEX idx);

	enum EDGE_ENUM
	{
		EDGE_XMAX = 0,
		EDGE_XMIN,
		EDGE_YMAX,
		EDGE_YMIN,
		EDGE_ZMAX,
		EDGE_ZMIN,
		EDGE_MAX,
	};

	void guiMeshSetting();
}

struct meshData
{
	vertexbuffer* vbs;
	//can be null
	vertexbuffer* norm;

	indexbuffer* idx;
	indexbuffer* idxLine;

	float AABB[msh::EDGE_MAX] = {
		0.5f, -0.5f, 
		0.5f, -0.5f, 
		0.5f, -0.5f, 
	};
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