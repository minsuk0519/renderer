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

void object::submit(void* cbvLoc, uint& offset, uint localID)
{
	uint data;

	data = (localID << 16) | (getMeshIdx() << 3) | lod;

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

void object::boundData(unsigned char* data)
{
	meshData* mshData = getMesh()->getData();

	float aabbData[6];

	float xScale = mshData->boundData.halfExtent[msh::AXIS_X];
	float yScale = mshData->boundData.halfExtent[msh::AXIS_Y];
	float zScale = mshData->boundData.halfExtent[msh::AXIS_Z];

	aabbData[0] = xScale * trans->getScale().m128_f32[0];
	aabbData[1] = yScale * trans->getScale().m128_f32[1];
	aabbData[2] = zScale * trans->getScale().m128_f32[2];

	aabbData[3] = trans->getPosition().m128_f32[0];
	aabbData[4] = trans->getPosition().m128_f32[1];
	aabbData[5] = trans->getPosition().m128_f32[2];

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

	const int maxLOD = meshPtr->getData()->lodNum;
	gui::editintwithrange("ForceLOD##" + std::to_string(id), reinterpret_cast<int *>(&lod), 0, maxLOD - 1);

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

uint object::getObjID() const
{
	return id;
}

bool object::instanceCulling(DirectX::XMVECTOR* frustum)
{
	DirectX::XMVECTOR scale = trans->getScale();

	float xExtent = scale.m128_f32[0] * meshPtr->getData()->boundData.halfExtent[msh::AXIS_X];
	float yExtent = scale.m128_f32[1] * meshPtr->getData()->boundData.halfExtent[msh::AXIS_Y];
	float zExtent = scale.m128_f32[2] * meshPtr->getData()->boundData.halfExtent[msh::AXIS_Z];

	float r = std::sqrt(xExtent * xExtent + yExtent * yExtent + zExtent * zExtent);

	DirectX::XMVECTOR objCenter = trans->getPosition();
	objCenter.m128_f32[3] = 1.0f;

	//bound sphere vs frustum test
	for (uint i = 0; i < 6; ++i)
	{
		bool isIn = DirectX::XMPlaneDot(frustum[i], objCenter).m128_f32[0] <= r;

		if (!isIn)
		{
			return false;
		}
	}

	return true;
}
