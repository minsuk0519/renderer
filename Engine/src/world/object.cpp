#include <world\object.hpp>
#include <render\mesh.hpp>
#include <render/pipelinestate.hpp>
#include <render/camera.hpp>
#include <render/descriptorheap.hpp>
#include <render/commandqueue.hpp>
#include <system/gui.hpp>

namespace obj
{
	uint remainID = 0;
}

transform* object::getTransform() const
{
	return trans;
}

bool object::init(const msh::MESH_INDEX meshIdx, const uint psoIndex, bool gui)
{
	pso = psoIndex;
	trans = new transform();

	meshEnumIndex = meshIdx;
	meshPtr = msh::getMesh(meshIdx);

	cbv = buf::createConstantBuffer(consts::CONST_OBJ_SIZE);

	desc = (render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_CONSTANT_TYPE, cbv));

	id = obj::remainID++;

	displayUI = gui;

	return true;
}



void object::draw(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, commandqueue* cmdQueue, bool debugDraw)
{
	cmdQueue->sendData(CBV_OBJECT, desc.getHandle());

	//if (debugDraw)
	//{
	//	float a[6] = { 0.5f, -0.5f, 0.5f, -0.5f, 0.5f, -0.5f };
	//	cmdList->SetGraphicsRoot32BitConstants(2, 6, a, 0);
	//}

	//meshPtr->setBuffer(cmdList, debugDraw);
	//meshPtr->draw(cmdList);

	//if (debugDraw)
	//{
	//	cmdList->SetGraphicsRoot32BitConstants(2, 6, meshPtr->getData()->AABB, 0);

	//	mesh* AABBMesh = msh::getMesh(msh::MESH_CUBE);
	//	AABBMesh->setBuffer(cmdList, true);
	//	AABBMesh->draw(cmdList);
	//}
}

void object::update(float dt)
{
	float* matPointer = trans->getMatPointer();

	float a[] = {
		albedo.x, albedo.y, albedo.z,
		metal,
		roughness,
	};

	memcpy(cbv->info.cbvDataBegin, matPointer, cbv->info.size);
	memcpy(cbv->info.cbvDataBegin + sizeof(float) * 16, &a, cbv->info.size);
}

void object::submit(void* cbvLoc, uint& offset)
{
	uint data;

	//TODO : LOD is set to 0 for now but need to make lod selection
	data = (id << 16) | (getMeshIdx() << 3) | 0;

	memcpy(cbvLoc, &data, 4);

	offset += 1 + (getMesh()->getData()->idx->view.SizeInBytes / (sizeof(uint) * 3)) / 64;
}

void object::sendMat(unsigned char* cbvdata)
{
	float* matPointer = trans->getMatPointer();

	float a[] = {
		albedo.x, albedo.y, albedo.z,
		metal,
		roughness,
	};

	unsigned char* dataLoc;
		
	if(cbvdata == nullptr) dataLoc = cbv->info.cbvDataBegin;
	else dataLoc = cbvdata + consts::CONST_OBJ_SIZE_ALLIGNMENT * id;

	memcpy(dataLoc, matPointer, cbv->info.size);
	memcpy(dataLoc + sizeof(float) * 16, &a, cbv->info.size);
}

void object::uploadViewInfo(unsigned char* dataLoc)
{
	DirectX::XMVECTOR quat = trans->getQuaternion();
	//translate
	memcpy(dataLoc, trans->getPosPointer(), sizeof(uint) * 3);
	//scale
	memcpy(dataLoc + sizeof(uint) * 3, trans->getScalePointer(), sizeof(uint) * 3);
	//rotate
	memcpy(dataLoc + sizeof(uint) * 6, &quat, sizeof(uint) * 4);
}

void object::aabbData(unsigned char* data)
{
	float* aabb = getMesh()->getData()->AABB;

	float aabbData[6];

	float xScale = aabb[msh::EDGE_XMAX] - aabb[msh::EDGE_XMIN];
	float yScale = aabb[msh::EDGE_YMAX] - aabb[msh::EDGE_YMIN];
	float zScale = aabb[msh::EDGE_ZMAX] - aabb[msh::EDGE_ZMIN];

	float xOffset = (aabb[msh::EDGE_XMAX] + aabb[msh::EDGE_XMIN]) * 0.5f;
	float yOffset = (aabb[msh::EDGE_YMAX] + aabb[msh::EDGE_YMIN]) * 0.5f;
	float zOffset = (aabb[msh::EDGE_ZMAX] + aabb[msh::EDGE_ZMIN]) * 0.5f;

	aabbData[0] = xScale * 0.5f * trans->getScale().m128_f32[0];
	aabbData[1] = yScale * 0.5f * trans->getScale().m128_f32[1];
	aabbData[2] = zScale * 0.5f * trans->getScale().m128_f32[2];

	aabbData[3] = xOffset / aabbData[0] + trans->getPosition().m128_f32[0];
	aabbData[4] = yOffset / aabbData[1] + trans->getPosition().m128_f32[1];
	aabbData[5] = zOffset / aabbData[2] + trans->getPosition().m128_f32[2];

	memcpy(data, aabbData, sizeof(float) * 6);
}

void object::close()
{
	delete trans;
}

void object::guiSetting()
{
	gui::color("Albedo##" + std::to_string(id), &albedo.x);
	gui::editfloat("Metal##" + std::to_string(id), 1, &metal, 0.0f, 1.0f);
	gui::editfloat("Roughness##" + std::to_string(id), 1, &roughness, 0.0f, 1.0f);

	gui::editfloat("Position##" + std::to_string(id), 3, trans->getPosPointer(), 0.0f, 0.0f);
	gui::editfloat("Scale##" + std::to_string(id), 3, trans->getScalePointer(), 0.0f, 0.0f);

	meshEnumIndex = meshPtr->getId();

	if (meshEnumIndex != -1)
	{
		uint currentMeshId = meshEnumIndex;
				
		gui::comboBox("Mesh##" + std::to_string(id), msh::MESHNAME, sizeof(msh::MESHNAME) / sizeof(const char*), currentMeshId);
				
		if (meshEnumIndex != currentMeshId)
		{
			meshPtr = msh::getMesh(static_cast<msh::MESH_INDEX>(currentMeshId));
			meshEnumIndex = currentMeshId;
		}
	}
}

void object::disableWire()
{
	displayWire = false;
}

mesh* object::getMesh() const
{
	return meshPtr;
}

void object::setMaterial(float m, float r)
{
	metal = m;
	roughness = r;
}

void object::setAlbedo(float r, float g, float b)
{
	albedo = DirectX::XMFLOAT4{ r, g, b, 1 };
}

uint64_t object::getCBVLoc() const
{
	return cbv->resource->GetGPUVirtualAddress();
}

uint object::getMeshIdx() const
{
	return meshEnumIndex;
}

bool object::frustumCulling(camera* cam)
{
	frstumCulled = true;

	DirectX::XMMATRIX objMat = trans->getMat();
	DirectX::XMMATRIX camMat = cam->getMat();

	DirectX::XMMATRIX mat = objMat * camMat;

	float XMAX = getMesh()->getData()->AABB[msh::EDGE_XMAX];
	float XMIN = getMesh()->getData()->AABB[msh::EDGE_XMIN];
	float YMAX = getMesh()->getData()->AABB[msh::EDGE_YMAX];
	float YMIN = getMesh()->getData()->AABB[msh::EDGE_YMIN];
	float ZMAX = getMesh()->getData()->AABB[msh::EDGE_ZMAX];
	float ZMIN = getMesh()->getData()->AABB[msh::EDGE_ZMIN];

	DirectX::XMVECTOR cubeVert[] =
	{
		{ XMAX,  YMAX,  ZMAX, 1.0f},
		{ XMAX,  YMIN,  ZMAX, 1.0f},
		{ XMIN,  YMAX,  ZMAX, 1.0f},
		{ XMIN,  YMIN,  ZMAX, 1.0f},
		{ XMAX,  YMAX,  ZMIN, 1.0f},
		{ XMAX,  YMIN,  ZMIN, 1.0f},
		{ XMIN,  YMAX,  ZMIN, 1.0f},
		{ XMIN,  YMIN,  ZMIN, 1.0f},
	};

	float x[8];
	float y[8];
	float z[8];

	for (int i = 0; i < 8; ++i)
	{
		DirectX::XMVECTOR result = DirectX::XMVector4Transform(cubeVert[i], mat);

		x[i] = result.m128_f32[0] / abs(result.m128_f32[3]);
		y[i] = result.m128_f32[1] / abs(result.m128_f32[3]);
		z[i] = result.m128_f32[2] / abs(result.m128_f32[3]);
	}

	int count = 0;
	for (int i = 0; i < 8; ++i)
	{
		if (x[i] < -1) ++count;
	}
	if (count == 8) return true;

	count = 0;
	for (int i = 0; i < 8; ++i)
	{
		if (x[i] > 1) ++count;
	}
	if (count == 8) return true;

	count = 0;
	for (int i = 0; i < 8; ++i)
	{
		if (y[i] < -1) ++count;
	}
	if (count == 8) return true;

	count = 0;
	for (int i = 0; i < 8; ++i)
	{
		if (y[i] > 1) ++count;
	}
	if (count == 8) return true;

	count = 0;
	for (int i = 0; i < 8; ++i)
	{
		if (z[i] < -1) ++count;
	}
	if (count == 8) return true;

	count = 0;
	for (int i = 0; i < 8; ++i)
	{
		if (z[i] > 1) ++count;
	}
	if (count == 8) return true;

	frstumCulled = false;

	return false;
}

uint object::getObjID() const
{
	return id;
}
