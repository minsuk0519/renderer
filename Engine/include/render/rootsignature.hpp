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
		D3D12_DESCRIPTOR_RANGE_TYPE type = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		D3D12_SHADER_VISIBILITY vis = D3D12_SHADER_VISIBILITY_ALL;
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
	bool init(std::vector<render::root_init_param> descriptors, const std::vector<uint>& constantNums, bool CS = false);
	bool initFromShader(const std::vector<uint>& shaderIDs, std::map<uint, uint>& hlslPos, bool cs);
	void setDescriptorHeap(std::vector<render::descriptorHeapIndex> descHeapsIdx);

	void setRootSignature(const Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& cmdList);
	void registerDescHeap(const Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& cmdList);

	ID3D12RootSignature* getrootSignature();
};