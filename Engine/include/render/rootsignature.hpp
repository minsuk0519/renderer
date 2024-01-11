#pragma once

#include <wrl.h>

#include <d3d12.h>

#include <vector>

#include <render\descriptorheap.hpp>

class rootsignature;

namespace root
{
	enum ROOT_INDEX
	{
		ROOT_PBR = 0,
		ROOT_END,
	};

	struct root_init_param
	{
		D3D12_DESCRIPTOR_RANGE_TYPE type;
		D3D12_SHADER_VISIBILITY vis;
		uint num = 1;
	};

	bool initRootSignatures(Microsoft::WRL::ComPtr<ID3D12Device2> dxdevice);
	void cleanUp();

	rootsignature* getRootSignature(const ROOT_INDEX index);
};

class rootsignature
{
private:
	//will be changed later
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	std::vector<descheap::descriptorHeapIndex> descHeaps;

	bool compute = false;
public:

	bool init(std::vector<root::root_init_param> descriptors, std::vector<uint> constantNums, bool CS = false);
	void setDescriptorHeap(std::vector<descheap::descriptorHeapIndex> descHeapsIdx);

	void setRootSignature(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList);
	void registerDescHeap(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList);

	ID3D12RootSignature* getrootSignature();
};