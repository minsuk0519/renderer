#pragma once

#include <system\defines.hpp>

#include <wrl.h>
#include <d3d12.h>

#include <array>
#include <string>

struct buffer;
struct vertexbuffer;
struct indexbuffer;
struct constantbuffer;
struct depthbuffer;
struct imagebuffer;
struct uavbuffer;

struct meshData;

namespace buf
{
	enum BUFFER_TYPE
	{
		BUFFER_VERTEX_TYPE,
		BUFFER_CONSTANT_TYPE,
		BUFFER_UAV_TYPE,
		BUFFER_IMAGE_TYPE,
		BUFFER_RT_TYPE,
		BUFFER_INDEX_TYPE,
		BUFFER_DEPTH_TYPE,
	};

	enum VERTEX_TYPE
	{
		VERTEX_START = 0,
		VERTEX_TRIANGLE = VERTEX_START,
		VERTEX_CUBE,
		VERTEX_CUBE_NORM,
		VERTEX_OBJ,
		VERTEX_OBJ_NORM,
		VERTEX_END,
	};

	enum CONSTANT_TYPE
	{
		CONSTANT_START = VERTEX_END,
		CONSTANT_PROJECTION = CONSTANT_START,
		CONSTANT_OBJECT,
		CONSTANT_OBJECT2,
		CONSTANT_SUN,
		CONSTANT_HAMRAN,
		CONSTANT_END,
	};

	enum UAV_TYPE
	{
		UAV_START = CONSTANT_END,
		UAV_END,
	};

	enum IMAGE_TYPE
	{
		IMAGE_START = UAV_END,
		IMAGE_IRRADIANCE = IMAGE_START,
		IMAGE_ENVIRONMENT,
		IMAGE_DEPTH,
		IMAGE_END,
	};

	enum INDEX_TYPE
	{
		INDEX_START = IMAGE_END,
		INDEX_CUBE = INDEX_START,
		INDEX_OBJ,
		INDEX_END,
	};

	enum DEPTH_TYPE
	{
		DEPTH_START = INDEX_END,
		DEPTH_SWAPCHAIN = DEPTH_START,
		DEPTH_END,
	};

	bool loadResources();
	void cleanUp();

	void loadFiletoMesh(std::string fileName, meshData* meshdata);

	vertexbuffer* createVertexBuffer(void* data, const uint size, const uint stride);
	vertexbuffer* createVertexBufferFromUAV(uavbuffer* uav, const uint stride);
	vertexbuffer* getVertexBuffer(int index);

	constantbuffer* createConstantBuffer(const uint size);
	constantbuffer* createConstantBufferFromVertex(vertexbuffer* vertex);
	constantbuffer* createConstantBufferFromIndex(indexbuffer* index);
	constantbuffer* getConstantBuffer(int index);

	indexbuffer* createIndexBuffer(void* data, const uint size);
	indexbuffer* createIndexBufferFromUAV(uavbuffer* uav);
	indexbuffer* getIndexBuffer(int index);

	depthbuffer* createDepthBuffer(const uint width, const uint height);
	depthbuffer* getDepthBuffer(int index);

	imagebuffer* createImageBuffer(const uint width, const uint height, uint mipLevel, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
	imagebuffer* createImageBufferFromBuffer(buffer* targetBuffer, D3D12_BUFFER_SRV desc);
	imagebuffer* getImageBuffer(int index);

	uavbuffer* createUAVBuffer(const uint size);
	uavbuffer* getUAVBuffer(int index);

	imagebuffer* loadTextureFromFile(std::wstring filename, bool mip);

	BUFFER_TYPE typeFromIndex(const uint index);

	buffer* createReadBackBuffer(uint size);
}

struct buffer
{
	Microsoft::WRL::ComPtr<ID3D12Resource> resource;
	virtual ~buffer() {}

	void uploadBuffer(uint size, void* data);

	void mapBuffer(unsigned char** dataPtr);
	void unmapBuffer();
};

struct vertexbuffer : public buffer
{
	D3D12_VERTEX_BUFFER_VIEW view = {};
};

struct indexbuffer : public buffer
{
	D3D12_INDEX_BUFFER_VIEW view = {};
};

struct uavbuffer : public buffer
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC view = {};
};

struct constantbuffer : public buffer
{
	struct cbvInfo
	{
		unsigned char* cbvDataBegin = 0;
		uint size = 0;
	};

	cbvInfo info;
	D3D12_CONSTANT_BUFFER_VIEW_DESC view = {};
};

struct depthbuffer : public buffer
{
	D3D12_DEPTH_STENCIL_VIEW_DESC view = {};
};

struct imagebuffer : public buffer
{
	D3D12_SHADER_RESOURCE_VIEW_DESC view = {};
};
