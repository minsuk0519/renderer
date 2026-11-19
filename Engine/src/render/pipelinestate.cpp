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

		TC_CONDITIONB(psoJsons.size() == render::PSO_END, "initPSO: pso.json entry count does not match PSO_END");

		for (const auto& psoData : psoJsons)
		{
			if (psoData.psoIndex >= render::PSO_END || pipelineStateObjects[psoData.psoIndex] != nullptr)
			{
				TC_LOG_ERROR(std::format("initPSO: psoIndex {} for '{}' is out of range or duplicated", psoData.psoIndex, psoData.psoName).c_str());
				cleanUpPSO();
				return false;
			}

			pipelinestate* newObject = new pipelinestate();

			const bool created = psoData.cs
				? newObject->initCS(psoData.psoName, psoData.vertexIndex)
				: newObject->init(psoData.psoName, psoData.vertexIndex, psoData.pixelIndex, psoData.formats, D3D12_CULL_MODE_NONE, psoData.wireframe, psoData.depth);

			if (!created)
			{
				TC_LOG_ERROR(std::format("initPSO: failed to create PSO '{}' (index {})", psoData.psoName, psoData.psoIndex).c_str());
				newObject->close();
				delete newObject;
				cleanUpPSO();
				return false;
			}

			pipelineStateObjects[psoData.psoIndex] = newObject;
		}

		return true;
	}

	void cleanUpPSO()
	{
		for (uint i = 0; i < PSO_END; ++i)
		{
			if (pipelineStateObjects[i] == nullptr)
			{
				continue;
			}

			pipelineStateObjects[i]->close();
			delete pipelineStateObjects[i];
			pipelineStateObjects[i] = nullptr;
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

			if (pso == nullptr)
			{
				continue;
			}

			pso->guiSetting();
		}
	}
}

bool pipelinestate::init(std::string psoName, uint VS, uint PS, std::vector<uint> formats, D3D12_CULL_MODE cull, bool wireframe, bool depth)
{
	shader* vs = shaders::getShader(VS);
	const shader* ps = shaders::getShader(PS);
	if (vs == nullptr || ps == nullptr)
	{
		TC_LOG_ERROR(std::format("PSO '{}': vertex shader {} or pixel shader {} is missing", psoName, VS, PS).c_str());
		return false;
	}

	D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	if (wireframe)
	{
		primitiveType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	}

	isCompute = false;

	rootsig = new rootsignature();

	if (!rootsig->initFromShader({VS, PS}, hlslLoc, false))
	{
		delete rootsig;
		rootsig = nullptr;
		hlslLoc.clear();
		return false;
	}
	rootsig->setDescriptorHeap({ render::DESCRIPTORHEAP_BUFFER });

	data.vertexIndex = VS;
	data.pixelIndex = PS;
	data.psoName = std::move(psoName);
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
	psoDesc.RasterizerState.FillMode = wireframe ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
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
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
		const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp =
		{ D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
		psoDesc.DepthStencilState.FrontFace = defaultStencilOp;
		psoDesc.DepthStencilState.BackFace = defaultStencilOp;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
	}
	psoDesc.SampleDesc.Count = 1;
	const HRESULT psoResult = e_globRenderer.device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineStateObject));
	TC_CONDITIONB(SUCCEEDED(psoResult), "Failed to create graphics pipeline state");

 	return true;
}

bool pipelinestate::initCS(std::string psoName, uint CS)
{
	const shader* cs = shaders::getShader(CS);
	if (cs == nullptr)
	{
		TC_LOG_ERROR(std::format("PSO '{}': compute shader {} is missing", psoName, CS).c_str());
		return false;
	}

	isCompute = true;

	data.psoName = std::move(psoName);
	data.vertexIndex = CS;
	data.cs = true;

	//TODO : creating rootsig on pso creation is not suitable for bindless engine design
	//this is inefficient when we need multiple psos in a frame
	rootsig = new rootsignature();

	if (!rootsig->initFromShader({ CS }, hlslLoc, true))
	{
		delete rootsig;
		rootsig = nullptr;
		hlslLoc.clear();
		return false;
	}
	rootsig->setDescriptorHeap({ render::DESCRIPTORHEAP_BUFFER });

	struct PipelineStateStream
	{
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_CS CS;
	} pipelineStateStream;

	pipelineStateStream.pRootSignature = rootsig->getrootSignature();
	pipelineStateStream.CS = cs->getByteCode();

	const D3D12_PIPELINE_STATE_STREAM_DESC psoDesc ={ sizeof(PipelineStateStream), &pipelineStateStream };

	const HRESULT pipelineStreamResult = e_globRenderer.device->CreatePipelineState(&psoDesc, IID_PPV_ARGS(&pipelineStateObject));
	TC_CONDITIONB(SUCCEEDED(pipelineStreamResult), "Failed to create compute pipeline state");

	return true;
}

void pipelinestate::sendGraphicsData(const Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& cmdList, uint loc, D3D12_GPU_DESCRIPTOR_HANDLE descLoc)
{
	if (hlslLoc.find(loc) != hlslLoc.end())
	{
		if (isCompute) cmdList->SetComputeRootDescriptorTable(hlslLoc.at(loc), descLoc);
		else cmdList->SetGraphicsRootDescriptorTable(hlslLoc.at(loc), descLoc);
	}
	else
	{
		const std::string warnMsg = std::format("You send {}, but there is no hlsl location", loc);
		TC_LOG_WARNING(warnMsg.c_str());
	}
}

void pipelinestate::sendConstantData(const Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& cmdList, uint loc, uint dataSize, void* dataPtr)
{
	if (hlslLoc.find(loc) != hlslLoc.end())
	{
		if (isCompute) cmdList->SetComputeRoot32BitConstants(hlslLoc.at(loc), dataSize, dataPtr, 0);
		else cmdList->SetGraphicsRoot32BitConstants(hlslLoc.at(loc), dataSize, dataPtr, 0);
	}
	else
	{
		const std::string warnMsg = std::format("You send {}, but there is no hlsl location", loc);
		TC_LOG_WARNING(warnMsg.c_str());
	}
}

void pipelinestate::setCommandSignature(const std::vector<render::cmdSigData>& cmdSigs)
{
	std::vector<D3D12_INDIRECT_ARGUMENT_DESC> argumentDescs;

	argumentDescs.resize(cmdSigs.size() + 1);

	uint byteStride = 0;

	if (isCompute)
	{
		argumentDescs[cmdSigs.size()].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
		byteStride += sizeof(D3D12_DISPATCH_ARGUMENTS);
	}
	else
	{
		argumentDescs[cmdSigs.size()].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
		byteStride += sizeof(D3D12_DRAW_ARGUMENTS);
	}

	for (uint i = 0; i < cmdSigs.size(); ++i)
	{
		argumentDescs[i].Type = cmdSigs[i].type;
		if (cmdSigs[i].type == D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT)
		{
			argumentDescs[i].Constant.RootParameterIndex = hlslLoc[cmdSigs[i].loc];
			argumentDescs[i].Constant.DestOffsetIn32BitValues = 0;
			argumentDescs[i].Constant.Num32BitValuesToSet = cmdSigs[i].num;
			byteStride += cmdSigs[i].num * sizeof(uint);
		}
	}

	D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
	commandSignatureDesc.pArgumentDescs = argumentDescs.data();
	commandSignatureDesc.NumArgumentDescs = (uint)cmdSigs.size() + 1;
	commandSignatureDesc.ByteStride = byteStride;

	const HRESULT sigResult = cmdSigs.empty()
		? e_globRenderer.device->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(&cmdSignature))
	: e_globRenderer.device->CreateCommandSignature(&commandSignatureDesc, rootsig->getrootSignature(), IID_PPV_ARGS(&cmdSignature));
	TC_CONDITION(SUCCEEDED(sigResult), "Failed to create command signature");
}

void pipelinestate::close()
{
	delete rootsig;
	rootsig = nullptr;
}

ID3D12PipelineState* pipelinestate::getPSO() const
{
	return pipelineStateObject.Get();
}

rootsignature* pipelinestate::getRootSig() const
{
	return rootsig;
}

ID3D12CommandSignature* pipelinestate::getCmdSignature() const
{
	return cmdSignature.Get();
}

bool pipelinestate::usesHlslLoc(uint loc) const
{
	return hlslLoc.find(loc) != hlslLoc.end();
}

void pipelinestate::guiSetting()
{
	ImGui::BulletText(data.psoName.c_str());
	if (!data.cs)
	{
		const shader* vs = shaders::getShader(data.vertexIndex);
		const shader* ps = shaders::getShader(data.pixelIndex);
		const std::string vsName = vs != nullptr ? vs->getName() : std::string("<missing>");
		const std::string psName = ps != nullptr ? ps->getName() : std::string("<missing>");
		ImGui::Text(("VertexShader : " + vsName).c_str());
		ImGui::Text(("PixelShader : " + psName).c_str());

		const std::string depthText =(data.depth ? "enabled" : "disabled");
		ImGui::Text(("Depth is " + depthText).c_str());

		const std::string numFormat =std::to_string(static_cast<uint>(data.formats.size()));

		ImGui::Text(("Num Formats : " + numFormat).c_str());

		for (auto form : data.formats)
		{
			uint size = 0;
			uint channel = 0;

			TC_ASSERT(form > 0);

			if (form <= 12) size = ((12 - form) / 4) * 32 + 64;
			else if (form <= 22) size = 64;
			else if (form <= 47) size = 32;
			else if (form <= 59) size = 16;
			else if (form <= 65) size = 8;
			else if (form <= 66) size = 1;
			else
			{
				size = 0;
				TC_BREAK();
			}

			// NOLINTNEXTLINE(bugprone-branch-clone) -- channel table keyed by DXGI format family (enum ranges); the same channel count legitimately recurs across distinct format ranges
				if (form <= 4) channel = 4;
			// NOLINTNEXTLINE(bugprone-branch-clone) -- channel table keyed by DXGI format family (RGB32 range); the 3-channel body recurs for R11G11B10 (form 26) on purpose
				else if (form <= 8) channel = 3;
			else if (form <= 14) channel = 4;
			// NOLINTNEXTLINE(bugprone-branch-clone) -- channel table keyed by DXGI format family; the 2-channel body recurs for the RG16/RG8/R24G8 ranges on purpose
				else if (form <= 20) channel = 2;
			// NOLINTNEXTLINE(bugprone-branch-clone) -- channel table keyed by DXGI format family; the 1-channel body recurs for the R32/R16/R8 ranges on purpose
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
			else
			{
				channel = 0;
				TC_BREAK();
			}

			const std::string text = "size : " + std::to_string(size) + ", channel : " + std::to_string(channel);

			ImGui::Text(text.c_str());
		}
	}
	else
	{
		const shader* cs = shaders::getShader(data.vertexIndex);
		const std::string csName = cs != nullptr ? cs->getName() : std::string("<missing>");
		ImGui::Text(("ComputeShader : " + csName).c_str());
	}
}
