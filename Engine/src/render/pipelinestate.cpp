#include <render/pipelinestate.hpp>
#include <render/rootsignature.hpp>
#include <render/shader.hpp>

#include <array>

#include <d3dx12.h>

namespace pso
{
	Microsoft::WRL::ComPtr<ID3D12Device2> device;

	std::array<pipelinestate*, PSO_END> pipelineStateObjects;
	
	bool loadResources(Microsoft::WRL::ComPtr<ID3D12Device2> dxdevice)
	{
		device = dxdevice;

		{
			pipelinestate* newObject = new pipelinestate();

			newObject->init(shaders::PBR_VS, shaders::PBR_PS, root::ROOT_PBR, { DXGI_FORMAT_R8G8B8A8_UNORM }, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE, D3D12_CULL_MODE_NONE, true);

			pipelineStateObjects[PSO_PBR] = newObject;
		}

		return true;
	}

	void cleanUp()
	{
		for (uint i = 0; i < PSO_END; ++i)
		{
			delete pipelineStateObjects[i];
		}
	}
	
	pipelinestate* getpipelinestate(PSO_INDEX index)
	{
		return pipelineStateObjects[index];
	}
}

bool pipelinestate::init(uint VS, uint PS, uint root, std::vector<DXGI_FORMAT> formats, D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveType, D3D12_CULL_MODE cull, bool depth)
{
	shader* vs = shaders::getShader(shaders::SHADER_INDEX(VS));
	shader* ps = shaders::getShader(shaders::SHADER_INDEX(PS));
	
	// Describe and create the graphics pipeline state object (PSO).
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout.NumElements = static_cast<uint>(vs->inputs.size());
	psoDesc.InputLayout.pInputElementDescs = vs->inputs.data();
	psoDesc.pRootSignature = root::getRootSignature(root::ROOT_INDEX(root))->getrootSignature();
	psoDesc.VS = vs->getByteCode();
	psoDesc.PS = ps->getByteCode();
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	//disable the cullmode
	psoDesc.RasterizerState.CullMode = cull;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = primitiveType;
	psoDesc.NumRenderTargets = static_cast<uint>(formats.size());
	for (int i = 0; i < formats.size(); ++i)
	{
		psoDesc.RTVFormats[i] = formats[i];// DXGI_FORMAT_R8G8B8A8_UNORM;
	}

	if (depth)
	{
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = true;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp =
		{ D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
		psoDesc.DepthStencilState.FrontFace = defaultStencilOp;
		psoDesc.DepthStencilState.BackFace = defaultStencilOp;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
	}
	psoDesc.SampleDesc.Count = 1;
	pso::device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineStateObject));

	return true;
}

bool pipelinestate::initCS(uint CS, uint root)
{
	shader* cs = shaders::getShader(shaders::SHADER_INDEX(CS));

	struct PipelineStateStream
	{
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_CS CS;
	} pipelineStateStream;

	pipelineStateStream.pRootSignature = root::getRootSignature(root::ROOT_INDEX(root))->getrootSignature();
	pipelineStateStream.CS = cs->getByteCode();

	D3D12_PIPELINE_STATE_STREAM_DESC psoDesc = { sizeof(PipelineStateStream), &pipelineStateStream };

	pso::device->CreatePipelineState(&psoDesc, IID_PPV_ARGS(&pipelineStateObject));

	return true;
}

ID3D12PipelineState* pipelinestate::getPSO() const
{
	return pipelineStateObject.Get();
}
