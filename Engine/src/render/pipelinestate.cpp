#include <render/pipelinestate.hpp>
#include <render/rootsignature.hpp>
#include <render/shader.hpp>
#include <render/renderer.hpp>
#include <render/commandqueue.hpp>
#include <render/descriptorheap.hpp>
#include <system/jsonhelper.hpp>
#include <system/gui.hpp>

#include <array>

#include <d3dx12.h>

namespace render
{
	std::array<pipelinestate*, PSO_END> pipelineStateObjects;
	
	bool initPSO()
	{
		std::vector<psoJson> psoJsons;

		readJsonBuffer(psoJsons, JSON_FILE_NAME::PSO_FILE);

		for (auto psoData : psoJsons)
		{
			pipelinestate* newObject = new pipelinestate();

			if (!psoData.cs) newObject->init(psoData.psoName, psoData.vertexIndex, psoData.pixelIndex, psoData.formats, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE, D3D12_CULL_MODE_NONE, psoData.depth);
			//else newObject->initCS(psoData.vertexIndex);
			
			pipelineStateObjects[psoData.psoIndex] = newObject;
		}

		return true;
	}

	void cleanUpPSO()
	{
		for (uint i = 0; i < PSO_END; ++i)
		{
			pipelineStateObjects[i]->close();
			delete pipelineStateObjects[i];
		}
	}
	
	pipelinestate* getpipelinestate(PSO_INDEX index)
	{
		return pipelineStateObjects[index];
	}

	void guiPSOSetting()
	{
		for (uint i = 0; i < PSO_END; ++i)
		{
			pipelinestate* pso = pipelineStateObjects[i];

			pso->guiSetting();
		}
	}
}

bool pipelinestate::init(std::string psoName, uint VS, uint PS, std::vector<uint> formats, D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveType, D3D12_CULL_MODE cull, bool depth)
{
	shader* vs = shaders::getShader(VS);
	shader* ps = shaders::getShader(PS);

	rootsig = new rootsignature();

	rootsig->initFromShader({ VS, PS }, hlslLoc);
	rootsig->setDescriptorHeap({ render::DESCRIPTORHEAP_BUFFER });

	data.vertexIndex = VS;
	data.pixelIndex = PS;
	data.psoName = psoName;
	data.formats.assign(formats.begin(), formats.end());
	data.cs = false;
	data.depth = depth;
	
	// Describe and create the graphics pipeline state object (PSO).
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout.NumElements = static_cast<uint>(vs->inputs.size());
	psoDesc.InputLayout.pInputElementDescs = vs->inputs.data();
	psoDesc.pRootSignature = rootsig->getrootSignature();
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
		psoDesc.RTVFormats[i] = (DXGI_FORMAT)(formats[i]);
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
	e_globRenderer.device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineStateObject));

 	return true;
}

bool pipelinestate::initCS(uint CS, uint root)
{
	shader* cs = shaders::getShader(CS);

	struct PipelineStateStream
	{
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_CS CS;
	} pipelineStateStream;

	//pipelineStateStream.pRootSignature = render::getRootSignature(render::ROOT_INDEX(root))->getrootSignature();
	pipelineStateStream.CS = cs->getByteCode();

	D3D12_PIPELINE_STATE_STREAM_DESC psoDesc = { sizeof(PipelineStateStream), &pipelineStateStream };

	e_globRenderer.device->CreatePipelineState(&psoDesc, IID_PPV_ARGS(&pipelineStateObject));

	return true;
}

void pipelinestate::sendGraphicsData(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, uint loc, D3D12_GPU_DESCRIPTOR_HANDLE descLoc)
{
	if (hlslLoc.find(loc) != hlslLoc.end())
	{
		cmdList->SetGraphicsRootDescriptorTable(hlslLoc.at(loc), descLoc);
	}
	else
	{
		std::string warnMsg = std::format("You send {}, but there is no hlsl location", loc);
		TC_LOG_WARNING(warnMsg.c_str());
	}
}

void pipelinestate::close()
{
	delete rootsig;
}

ID3D12PipelineState* pipelinestate::getPSO() const
{
	return pipelineStateObject.Get();
}

rootsignature* pipelinestate::getRootSig() const
{
	return rootsig;
}

void pipelinestate::guiSetting()
{
	ImGui::BulletText(data.psoName.c_str());
	if (!data.cs)
	{
		shader* vs = shaders::getShader(data.vertexIndex);
		shader* ps = shaders::getShader(data.pixelIndex);
		ImGui::Text(("VertexShader : " + vs->getName()).c_str());
		ImGui::Text(("PixelShader : " + ps->getName()).c_str());
	}
	std::string depthText = (data.depth ? "enabled" : "disabled");
	ImGui::Text(("Depth is " + depthText).c_str());

	std::string numFormat = std::to_string(static_cast<uint>(data.formats.size()));

	ImGui::Text(("Num Formats : " + numFormat).c_str());

	for (auto form : data.formats)
	{
		uint size;
		uint channel;

		assert(form > 0);

		if (form <= 12) size = ((12 - form) / 4) * 32 + 64;
		else if (form <= 22) size = 64;
		else if (form <= 47) size = 32;
		else if (form <= 59) size = 16;
		else if (form <= 65) size = 8;
		else if (form <= 66) size = 1;
		else assert(0);

		if (form <= 4) channel = 4;
		else if (form <= 8) channel = 3;
		else if (form <= 14) channel = 4;
		else if (form <= 20) channel = 2;
		else if (form <= 22) channel = 1;
		else if (form <= 25) channel = 4;
		else if (form <= 26) channel = 3;
		else if (form <= 32) channel = 4;
		else if (form <= 38) channel = 2;
		else if (form <= 43) channel = 1;
		else if (form <= 45) channel = 2;
		else if (form <= 47) channel = 1;
		else if (form <= 52) channel = 2;
		else if (form <= 66) channel = 1;
		else assert(0);

		std::string text = "size : " + std::to_string(size) + ", channel : " + std::to_string(channel);

		ImGui::Text(text.c_str());
	}
}
