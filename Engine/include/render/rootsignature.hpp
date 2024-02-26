#pragma once

#include <wrl.h>

#include <d3d12.h>

#include <vector>
#include <map>

#include <render\descriptorheap.hpp>

class rootsignature;

namespace render
{
	struct root_init_param
	{
		D3D12_DESCRIPTOR_RANGE_TYPE type;
		D3D12_SHADER_VISIBILITY vis;
		uint num = 1;
	};
};

class rootsignature
{
private:
	//will be changed later
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	std::vector<render::descriptorHeapIndex> descHeaps;

	bool compute = false;
public:
	bool init(std::vector<render::root_init_param> descriptors, std::vector<uint> constantNums, bool CS = false);
	bool initFromShader(std::vector<uint> shaderIDs, std::map<uint, uint>& hlslPos, bool cs);
	void setDescriptorHeap(std::vector<render::descriptorHeapIndex> descHeapsIdx);

	void setRootSignature(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList);
	void registerDescHeap(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList);

	ID3D12RootSignature* getrootSignature();
};