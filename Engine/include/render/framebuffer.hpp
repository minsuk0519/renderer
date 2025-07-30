#pragma once
#include <render/descriptorheap.hpp>

#include <vector>

#include <dxgi1_6.h>
#include <DirectXMath.h>

struct buffer;

class framebuffer
{
private:
	std::vector<buffer*> FBOs;

	DirectX::XMFLOAT4 clearColor;
	bool isDepth = false;
	float clearDepth = 1.0f;

public:
	bool createAddFBO(uint width, uint height, DXGI_FORMAT format, DirectX::XMFLOAT4 clearColor = DirectX::XMFLOAT4(0.8f, 0.9f, 0.9f, 1.0f));
	bool attachResource(ID3D12Resource* resource, uint width, uint height, DXGI_FORMAT format, DirectX::XMFLOAT4 clearColor = DirectX::XMFLOAT4(0.8f, 0.9f, 0.9f, 1.0f));

	void openFB(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, bool clear);
	void closeFB(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList);

	void setDepthClear(float depth);

	D3D12_GPU_DESCRIPTOR_HANDLE getDescHandle(uint FBOIndex);
};