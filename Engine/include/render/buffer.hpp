#pragma once

#include <system\defines.hpp>
#include <system\config.hpp>

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

	uint estimateBufferSize(uint_8 flags);

	enum graphicBufferFlags
	{
		GBF_UAV,
		GBF_CBV,
		GBF_SRV,
		GBF_VERTEX_VIEW,
		GBF_INDEX_VIEW,
		GBF_DEPTH_STENCIL,
		GBF_COUNT,
		GBF_FBO = GBF_COUNT,
	};

	enum resourceFlags
	{
		RESOURCE_UPLOAD		= (1 << 0),
		RESOURCE_READBACK	= (1 << 1),
		RESOURCE_COPY		= (1 << 2),
		RESOURCE_DEPTH		= (1 << 3),
	};

	class viewAllocator
	{
	public:
		typedef void (*allocateViewFunc)(char* viewPos, buffer* buf);

		static void allocateUAVs(char* viewPos, buffer* buf);
		static void allocateCBVs(char* viewPos, buffer* buf);
		static void allocateSRVs(char* viewPos, buffer* buf);
		static void allocateVertViews(char* viewPos, buffer* buf);
		static void allocateIndexViews(char* viewPos, buffer* buf);
		static void allocateDepthViews(char* viewPos, buffer* buf);
	};


	viewAllocator::allocateViewFunc allocateViews[graphicBufferFlags::GBF_COUNT] = {
		viewAllocator::allocateUAVs,
		viewAllocator::allocateCBVs,
		viewAllocator::allocateSRVs,
		viewAllocator::allocateVertViews,
		viewAllocator::allocateIndexViews,
		viewAllocator::allocateDepthViews,
	};
}

struct buffer_allocator
{
private:
	char* data;

	uint16_t nextID = 0;

	std::vector<std::pair<uint, uint>> freedMem;
	std::vector<buffer*> memBlocks;

	void freeInternal(char* data, uint index);
public:

	void init();
	void update();
	char* alloc(char* bufferData = nullptr, uint size = 0, uint stride = 1, uint texture = 0, uint8_t flags = 0, uint lifeTime = 0, buf::resourceFlags = 0);
	void free(char* data);
	void free(uint index);
};

struct buffer_header
{
	uint dataSize;
	uint totalSize;

	union packed_data
	{
		uint bufferId : 16;
		uint lifetime : 2;
		uint allocated : 1;
		uint viewFlags : 8;
		uint stride : 2;
		uint texture : 1;
		uint depth : 1;
		uint pad : 1;
	} packedData;

	//D3D12_VERTEX_BUFFER_VIEW
	//D3D12_INDEX_BUFFER_VIEW
	//D3D12_UNORDERED_ACCESS_VIEW_DESC
	//D3D12_CONSTANT_BUFFER_VIEW_DESC
	//D3D12_DEPTH_STENCIL_VIEW_DESC
	//D3D12_SHADER_RESOURCE_VIEW_DESC
};

struct buffer
{
public:
	uint getElemSize() const;

private:
	friend struct buffer_allocator;
	friend class buf::viewAllocator;

	char* baseLoc;

	buffer_header header;

	Microsoft::WRL::ComPtr<ID3D12Resource> resource;

	virtual ~buffer() {}

	void uploadBuffer(uint size, uint offset, void* data);

	void mapBuffer(unsigned char** dataPtr);
	void unmapBuffer();
};

//buffer_header + buffer
#define BUFFER_HEADER_SIZE ((4 + 4 + 4) + (4 + 8))
