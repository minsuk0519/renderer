#pragma once

#include <system\defines.hpp>
#include <system\config.hpp>
#include <system/logger.hpp>

#include <wrl.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <DirectXMath.h>

#include <array>
#include <string>
#include <source_location>

struct buffer;

struct meshData;
struct descriptor;

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

	bool loadResources();
	void cleanUp();

	void loadFiletoMesh(std::string fileName, meshData* meshdata, uint id);
	void uploadLoadedMesh(meshData* meshdata, float* p, float* n, uint* i, uint vNum, uint iNum, uint id);

	buffer* loadTextureFromFile(std::wstring filename, bool mip);

	buffer* createReadBackBuffer(uint size);

	uint estimateBufferSize(uint_8 flags);

	enum graphicBufferFlags : uint_8
	{
		GBF_NONE,
		GBF_UAV					= (1 << 0),
		GBF_CBV					= (1 << 1),
		GBF_SRV					= (1 << 2),
		GBF_DEPTH_STENCIL		= (1 << 3),
		GBF_RT					= (1 << 4),
		GBF_FBO					= (1 << 5),
		GBF_COUNT = 6,
	};

	enum resourceFlags : uint
	{
		RESOURCE_NONE		= 0,
		RESOURCE_UPLOAD		= (1 << 0),
		RESOURCE_READBACK	= (1 << 1),
		RESOURCE_COPY		= (1 << 2),
		RESOURCE_DEPTH		= (1 << 3),
		RESOURCE_TEXTURE	= (1 << 4),
		//only reside one frame will be deallocated next frame
		RESOURCE_ONETIME	= (1 << 5),
		RESOURCE_CLEAR		= (1 << 6),
		RESOURCE_SHARED		= (1 << 7), // attach to an existing resource owned by another buffer; takes its own AddRef'd reference instead of transferring ownership
	};

	class viewAllocator
	{
	public:
		typedef char* (*allocateViewFunc)(char* viewPos, buffer* buf, const CD3DX12_RESOURCE_DESC& bufDesc, UINT16 mipSlice, UINT16 mipViewCount);

		static char* allocateUAVs(char* viewPos, buffer* buf, const CD3DX12_RESOURCE_DESC& bufDesc, UINT16 mipSlice, UINT16 mipViewCount);
		static char* allocateCBVs(char* viewPos, buffer* buf, const CD3DX12_RESOURCE_DESC& bufDesc, UINT16 mipSlice, UINT16 mipViewCount);
		static char* allocateSRVs(char* viewPos, buffer* buf, const CD3DX12_RESOURCE_DESC& bufDesc, UINT16 mipSlice, UINT16 mipViewCount);
		static char* allocateDepthViews(char* viewPos, buffer* buf, const CD3DX12_RESOURCE_DESC& bufDesc, UINT16 mipSlice, UINT16 mipViewCount);
		static char* allocateRenderTarget(char* viewPos, buffer* buf, const CD3DX12_RESOURCE_DESC& bufDesc, UINT16 mipSlice, UINT16 mipViewCount);
	};


	static viewAllocator::allocateViewFunc allocateViews[graphicBufferFlags::GBF_COUNT] = {
		viewAllocator::allocateUAVs,
		viewAllocator::allocateCBVs,
		viewAllocator::allocateSRVs,
		viewAllocator::allocateDepthViews,
		viewAllocator::allocateRenderTarget,
	};
}

struct buffer_allocator
{
private:
	char* data;

	uint16_t nextID = 0;

	std::vector<std::pair<uint, uint>> freedMem;
	std::vector<buffer*> memBlocks;

	void freeInternal(uint startPos, uint size, uint bufferId);
public:

	void init();
	void update();
	buffer* alloc(char* bufferData = nullptr, uint size = 0, uint stride = sizeof(float), uint_8 viewFlags = buf::GBF_NONE, uint flag = buf::RESOURCE_NONE,
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN, UINT64 width = 0, UINT height = 0, UINT16 mipLevels = 1, DirectX::XMFLOAT4 clearColor = {}, ID3D12Resource* resource = nullptr,
		UINT16 mipSlice = 0, UINT16 mipViewCount = 0, const char* debugName = nullptr, std::source_location debugLoc = std::source_location::current());
	void free(char* bufferData);
	void free(uint index);

#if ENGINE_DEBUG_RESOURCEVIEW
	uint getArenaCapacity() const;
	uint getArenaUsed() const;
	uint getFreeBlockCount() const;
	void getFreeBlock(uint i, uint& start, uint& size) const;
#endif // ENGINE_DEBUG_RESOURCEVIEW
};

struct buffer_header
{
	uint dataSize;
	uint totalSize;

	struct packed_data
	{
		uint bufferId	: 16;
		uint allocated	: 1;
		uint viewFlags	: 8;
		uint stride		: 4;
		uint texture	: 1;
		uint lifetime	: 1;
		uint external	: 1;
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
	
	ID3D12Resource* getResource() const;
	buffer_header* getHeader();
	const buffer_header* getHeader() const;

	void uploadBuffer(uint size, uint offset, void* data);

	descriptor* getDesc(buf::graphicBufferFlags flag);

	CD3DX12_RESOURCE_BARRIER getTransition(D3D12_RESOURCE_STATES after);
	D3D12_RESOURCE_STATES getCurResourceState() const;

	void mapBuffer(unsigned char** dataPtr);
	void unmapBuffer();

	bool mapReadbackBuffer(unsigned char** dataPtr, uint readSize);
	void unmapReadbackBuffer();
private:
	friend struct buffer_allocator;
	friend class buf::viewAllocator;

	char* baseLoc;
	Microsoft::WRL::ComPtr<ID3D12Resource> resource;

	buffer_header header;
	D3D12_RESOURCE_STATES curState;
};

#if ENGINE_DEBUG_RESOURCEVIEW
namespace buf
{
	const char* getResourceDisplayName(uint bufferId);
	void recordResourceView(buffer* buf, const descriptor& desc, BUFFER_TYPE type);
	void guiResourceViewerSetting();
	void guiMemoryViewerSetting();
}
#endif // ENGINE_DEBUG_RESOURCEVIEW

extern buffer_allocator e_globBufAllocator;