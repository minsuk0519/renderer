#pragma once
#include <array>
#include <system/defines.hpp>
#include <render/descriptorheap.hpp>
#include <render/buffer.hpp>

#include <dxgi1_6.h>
#include <d3d12.h>
#include <wrl.h>

#include <DirectXMath.h>

constexpr uint FRAME_COUNT = 2;

class renderer
{
public:
	bool init(Microsoft::WRL::ComPtr<IDXGIFactory4> dxFactory, Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter);
	void draw(float dt);
	void close();

	void preDraw(float dt);

	Microsoft::WRL::ComPtr<ID3D12Device2> device;
private:
	bool createDevice(Microsoft::WRL::ComPtr<IDXGIFactory4> dxFactory, Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter);
	bool checkFeatureSupport(DXGI_FEATURE feature);
	bool createSwapChain();
	bool createFrameResources();

private:
	void renderScene();

public:

private:
	//created from engine
	Microsoft::WRL::ComPtr<IDXGIFactory4> factory;

	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain;

	uint frameIndex = 0;

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> copyCmdList;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> computeCmdList;

	bool eject = false;

	Microsoft::WRL::ComPtr<ID3D12CommandSignature> cmdSignature;
	Microsoft::WRL::ComPtr<ID3D12CommandSignature> cmdprepassSignature;

	descriptor swapchainDesc[FRAME_COUNT];
	imagebuffer* swapchainBuffer[FRAME_COUNT];
};

extern renderer e_GlobRenderer;