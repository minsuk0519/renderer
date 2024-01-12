#include <render\rootsignature.hpp>
#include <system\defines.hpp>
#include <system/logger.hpp>

#include <string>
#include <iterator>
#include <array>

#include <d3dx12.h>

namespace root
{
	enum RANGE_TYPE
	{
		D3D12_RANGE_TYPE_CONSTANT = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER + 1
	};

	Microsoft::WRL::ComPtr<ID3D12Device2> device;

	std::array<rootsignature*, ROOT_END> rootsignatures;

	bool initRootSignatures(Microsoft::WRL::ComPtr<ID3D12Device2> dxdevice)
	{
		device = dxdevice;

		{
			rootsignature* rootsig = new rootsignature;

			rootsig->init({
				//projection
				{D3D12_DESCRIPTOR_RANGE_TYPE_CBV, D3D12_SHADER_VISIBILITY_ALL},
				//object
				{D3D12_DESCRIPTOR_RANGE_TYPE_CBV, D3D12_SHADER_VISIBILITY_ALL},
				}, {});
			rootsig->setDescriptorHeap({ descheap::DESCRIPTORHEAP_BUFFER });

			rootsignatures[ROOT_PBR] = rootsig;
		}

		return true;
	}

	void cleanUp()
	{
		for (uint i = 0; i < ROOT_END; ++i)
		{
			delete rootsignatures[i];
		}
	}

	rootsignature* getRootSignature(const ROOT_INDEX index)
	{
		return rootsignatures[index];
	}

}

bool rootsignature::init(std::vector<root::root_init_param> descriptors, std::vector<uint> constantNums, bool CS)
{
	compute = CS;

	std::vector<CD3DX12_DESCRIPTOR_RANGE> ranges;
	std::vector<CD3DX12_ROOT_PARAMETER> rootParameters;

	ranges.resize(descriptors.size());
	rootParameters.resize(descriptors.size() + constantNums.size());

	D3D12_DESCRIPTOR_RANGE_TYPE currentType = descriptors[0].type;
	uint index[root::D3D12_RANGE_TYPE_CONSTANT] = { 0 };
	for (int i = 0; i < descriptors.size(); ++i)
	{
		D3D12_DESCRIPTOR_RANGE_TYPE type = descriptors[i].type;
		D3D12_SHADER_VISIBILITY visibility = descriptors[i].vis;
		uint descNum = descriptors[i].num;

		if (type == (D3D12_DESCRIPTOR_RANGE_TYPE)root::D3D12_RANGE_TYPE_CONSTANT)
		{
			rootParameters[i].InitAsConstantBufferView(index[D3D12_DESCRIPTOR_RANGE_TYPE_CBV], 0, visibility);
			++index[D3D12_DESCRIPTOR_RANGE_TYPE_CBV];
		}
		else
		{
			ranges[i].Init(type, descNum, index[type], 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);
			//++index[type];
			index[type] += descNum;
			rootParameters[i].InitAsDescriptorTable(1, &ranges[i], visibility);
		}
	}

	uint rootParamIndex = descriptors.size();

	for (auto cons : constantNums)
	{
		rootParameters[rootParamIndex].InitAsConstants(cons, index[D3D12_DESCRIPTOR_RANGE_TYPE_CBV]);
		++index[D3D12_DESCRIPTOR_RANGE_TYPE_CBV];
		++rootParamIndex;
	}

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	CD3DX12_STATIC_SAMPLER_DESC samplerDesc = CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);

	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init(static_cast<UINT>(rootParameters.size()), rootParameters.data(), 1, &samplerDesc, rootSignatureFlags);

	Microsoft::WRL::ComPtr<ID3DBlob> signature;
	Microsoft::WRL::ComPtr<ID3DBlob> error;

	D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);

	std::string errorMsg;
	if (error != nullptr)
	{
		auto* p = reinterpret_cast<unsigned char*>(error->GetBufferPointer());
		auto  n = error->GetBufferSize();
		std::vector<unsigned char> buff;

		buff.reserve(n);
		std::copy(p, p + n, std::back_inserter(buff));

		for (auto c : buff) errorMsg += c;

		TC_LOG_ERROR("Cannot serialize root signature!");

		return false;
	}

	root::device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));

	return true;
}

void rootsignature::setDescriptorHeap(std::vector<descheap::descriptorHeapIndex> descHeapsIdx)
{
	descHeaps = descHeapsIdx;
}

void rootsignature::setRootSignature(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList)
{
	if(compute) cmdList->SetComputeRootSignature(rootSignature.Get());
	else cmdList->SetGraphicsRootSignature(rootSignature.Get());
}

void rootsignature::registerDescHeap(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList)
{
	std::vector<ID3D12DescriptorHeap*> heaps;
	for (auto descheap : descHeaps)
	{
		heaps.push_back(descheap::getHeap(descheap)->getHeap());
	}

	cmdList->SetDescriptorHeaps(static_cast<UINT>(heaps.size()), heaps.data());

	uint offset = 0;
	for (auto descheap : descHeaps)
	{
		offset = descheap::getHeap(descheap)->setRootTable(cmdList, offset);
	}
}

ID3D12RootSignature* rootsignature::getrootSignature()
{
	return rootSignature.Get();
}
