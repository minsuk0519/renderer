#pragma once

#include <wrl.h>

#include <d3d12.h>

#include <vector>
#include <map>

#include <render\descriptorheap.hpp>

class rootsignature;

class rootsignature
{
private:
	//will be changed later
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	std::vector<render::descriptorHeapIndex> descHeaps;

	bool compute = false;
public:
	bool initFromShader(const std::vector<uint>& shaderIDs, std::map<uint, uint>& hlslPos, bool cs);
	void setDescriptorHeap(std::vector<render::descriptorHeapIndex> descHeapsIdx);

	void setRootSignature(const Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& cmdList);
	void registerDescHeap(const Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& cmdList);

	ID3D12RootSignature* getrootSignature();
};