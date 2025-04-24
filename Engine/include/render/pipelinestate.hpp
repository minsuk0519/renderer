#pragma once

#include <system\defines.hpp>
#include <render\shader_defines.hpp>

#include <vector>
#include <string>
#include <map>
#include <wrl.h>

#include <d3d12.h>

class pipelinestate;
class commandqueue;
class rootsignature;

namespace render
{
	bool initPSO();
	void cleanUpPSO();

	pipelinestate* getpipelinestate(PSO_INDEX index);

	void guiPSOSetting();

	struct psoData
	{
		uint vertexIndex;
		uint pixelIndex;
		std::string psoName;
		std::vector<uint> formats;
		bool cs;
		bool depth;
	};

	struct cmdSigData
	{
		D3D12_INDIRECT_ARGUMENT_TYPE type;
		uint loc;
		uint num;
	};

}

class pipelinestate
{
private:
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStateObject;

	rootsignature* rootsig = nullptr;

	std::map<uint, uint> hlslLoc;
	
	render::psoData data;
	bool isCompute;

	Microsoft::WRL::ComPtr<ID3D12CommandSignature> cmdSignature;

public:
	bool init(std::string psoName, uint VS, uint PS, std::vector<uint> formats, D3D12_CULL_MODE cull, bool wireframe, bool depth);
	bool initCS(std::string psoName, uint CS);

	void sendGraphicsData(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, uint loc, D3D12_GPU_DESCRIPTOR_HANDLE descLoc);
	void sendConstantData(Microsoft::WRL::ComPtr< ID3D12GraphicsCommandList> cmdList, uint loc, uint dataSize, void* data);

	void setCommandSignature(const std::vector<render::cmdSigData>& data);

	void close();

	ID3D12PipelineState* getPSO() const;
	rootsignature* getRootSig() const;
	ID3D12CommandSignature* getCmdSignature() const;

	void guiSetting();
};