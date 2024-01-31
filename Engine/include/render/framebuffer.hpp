#pragma once
#include <render/descriptorheap.hpp>

#include <dxgi1_6.h>

class imagebuffer;

class framebuffer
{
private:
	descriptor desc;
	imagebuffer* imageBuffer;

public:

	bool createFB(uint width, uint height, uint frameNum, Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain);

	void openFB(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList);
	void closeFB(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList);
};