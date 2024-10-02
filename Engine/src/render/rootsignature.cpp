#include <render\rootsignature.hpp>
#include <system\defines.hpp>
#include <system/logger.hpp>
#include <render/renderer.hpp>
#include <render/shader.hpp>
#include <render/shader_defines.hpp>

#include <string>
#include <iterator>
#include <array>

#include <d3dx12.h>

namespace render
{
	enum RANGE_TYPE
	{
		D3D12_RANGE_TYPE_CONSTANT	= D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER + 1,
		D3D12_RANGE_TYPE_SIZE		= D3D12_RANGE_TYPE_CONSTANT + 1,
	};
}

struct dataInfo
{
	uint vis;
	uint hlslLocation;
	uint size;
};

bool rootsignature::initFromShader(std::vector<uint> shaderIDs, std::map<uint, uint>& hlslPos, bool cs)
{
	compute = cs;

	std::array<std::map<uint, dataInfo>, render::D3D12_RANGE_TYPE_SIZE> typeMaps;

	for (auto id : shaderIDs)
	{
		shader* pShader = shaders::getShader(id);

		for (auto constant : pShader->bufData.constantContainer)
		{
			uint descType;

			if (constant.name.find("cb_") != std::string::npos)
			{
				descType = render::D3D12_RANGE_TYPE_CONSTANT;
				if (constant.data > 64)
				{
					descType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
				}
				else
				{
					typeMaps[descType][constant.loc].size = (constant.data);
				}
			}
			else descType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;

			typeMaps[descType][constant.loc].vis |= (1 << pShader->getType());

			uint hlslLoc = GET_HLSL_LOC_CBV(constant.loc);

			typeMaps[descType][constant.loc].hlslLocation = hlslLoc;
		}

		for (auto texture : pShader->bufData.textureContainer)
		{
			typeMaps[D3D12_DESCRIPTOR_RANGE_TYPE_SRV][texture.loc].vis |= (1 << pShader->getType());

			uint hlslLoc = GET_HLSL_LOC_SRV(texture.loc);

			typeMaps[D3D12_DESCRIPTOR_RANGE_TYPE_SRV][texture.loc].hlslLocation = hlslLoc;
		}

		if (cs)
		{
			for (auto uav : pShader->bufData.outputContainer)
			{
				typeMaps[D3D12_DESCRIPTOR_RANGE_TYPE_UAV][uav.loc].vis |= (1 << pShader->getType());

				uint hlslLoc = GET_HLSL_LOC_UAV(uav.loc);

				typeMaps[D3D12_DESCRIPTOR_RANGE_TYPE_UAV][uav.loc].hlslLocation = hlslLoc;
			}
		}
	}

	std::vector<CD3DX12_ROOT_PARAMETER> rootParameters;
	std::vector<CD3DX12_DESCRIPTOR_RANGE> ranges;

	uint rootParametersSize = 0;

	for (uint i = 0; i < render::D3D12_RANGE_TYPE_SIZE; ++i)
	{
		rootParametersSize += typeMaps[i].size();
	}

	rootParameters.resize(rootParametersSize);
	ranges.resize(rootParametersSize);

	uint index = 0;

	for (uint i = 0; i < render::D3D12_RANGE_TYPE_SIZE; ++i)
	{
		for (auto typeData : typeMaps[i])
		{
			D3D12_SHADER_VISIBILITY vis;
			
			if (cs)
			{
				vis = D3D12_SHADER_VISIBILITY_ALL;
			}
			else
			{
				if (typeData.second.vis == 1)
				{
					vis = D3D12_SHADER_VISIBILITY_VERTEX;
				}
				else if (typeData.second.vis == 2)
				{
					vis = D3D12_SHADER_VISIBILITY_PIXEL;
				}
				else
				{
					vis = D3D12_SHADER_VISIBILITY_ALL;
				}
			}

			if (i == render::D3D12_RANGE_TYPE_CONSTANT)
			{
				rootParameters[index].InitAsConstants(typeData.second.size, typeData.first);
			}
			else
			{
				ranges[index].Init((D3D12_DESCRIPTOR_RANGE_TYPE)i, 1, typeData.first, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);
				rootParameters[index].InitAsDescriptorTable(1, &ranges[index], vis);
			}

			hlslPos[typeData.second.hlslLocation] = index;

			++index;
		}
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

	e_globRenderer.device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));

	return true;
}

bool rootsignature::init(std::vector<render::root_init_param> descriptors, std::vector<uint> constantNums, bool CS)
{
	compute = CS;

	std::vector<CD3DX12_DESCRIPTOR_RANGE> ranges;
	std::vector<CD3DX12_ROOT_PARAMETER> rootParameters;

	ranges.resize(descriptors.size());
	rootParameters.resize(descriptors.size() + constantNums.size());

	D3D12_DESCRIPTOR_RANGE_TYPE currentType = descriptors[0].type;
	uint index[render::D3D12_RANGE_TYPE_CONSTANT] = { 0 };
	for (int i = 0; i < descriptors.size(); ++i)
	{
		D3D12_DESCRIPTOR_RANGE_TYPE type = descriptors[i].type;
		D3D12_SHADER_VISIBILITY visibility = descriptors[i].vis;
		uint descNum = descriptors[i].num;

		if (type == (D3D12_DESCRIPTOR_RANGE_TYPE)render::D3D12_RANGE_TYPE_CONSTANT)
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

	e_globRenderer.device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));

	return true;
}

void rootsignature::setDescriptorHeap(std::vector<render::descriptorHeapIndex> descHeapsIdx)
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
		heaps.push_back(render::getHeap(descheap)->getHeap());
	}

	cmdList->SetDescriptorHeaps(static_cast<UINT>(heaps.size()), heaps.data());

	uint offset = 0;
	for (auto descheap : descHeaps)
	{
		offset = render::getHeap(descheap)->setRootTable(cmdList, offset);
	}
}

ID3D12RootSignature* rootsignature::getrootSignature()
{
	return rootSignature.Get();
}
