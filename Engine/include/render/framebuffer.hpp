#pragma once
#include <render/descriptorheap.hpp>

#include <vector>

#include <dxgi1_6.h>
#include <DirectXMath.h>

class imagebuffer;

struct framebufferObject
{
	imagebuffer* imageBuffer;
	descriptor desc;
	descriptor textureDesc;

	DirectX::XMFLOAT4 clearColor = DirectX::XMFLOAT4(0.8f, 0.9f, 0.9f, 1.0f);
};

class framebuffer
{
private:
	std::vector<framebufferObject*> FBOs;

	bool isDepth = false;
	float clearDepth = 1.0f;

public:
	bool createAddFBO(uint width, uint height, DXGI_FORMAT format, DirectX::XMFLOAT4 clearColor = DirectX::XMFLOAT4(0.8f, 0.9f, 0.9f, 1.0f));
	void addFBOfromBuf(Microsoft::WRL::ComPtr<ID3D12Resource>& resource, DirectX::XMFLOAT4 clearColor = DirectX::XMFLOAT4(0.8f, 0.9f, 0.9f, 1.0f));

	void openFB(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList);
	void closeFB(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList);

	void setDepthClear(float depth);

	void setgraphicsDescHandle(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, uint pos, uint FBOIndex);
};