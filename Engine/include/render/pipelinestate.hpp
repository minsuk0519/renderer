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
}

class pipelinestate
{
private:
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStateObject;

	rootsignature* rootsig = nullptr;

	std::map<uint, uint> hlslLoc;
	
	render::psoData data;

public:
	bool init(std::string psoName, uint VS, uint PS, std::vector<uint> formats, D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveType, D3D12_CULL_MODE cull, bool wireframe, bool depth);
	bool initCS(uint CS, uint root);

	void sendGraphicsData(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, uint loc, D3D12_GPU_DESCRIPTOR_HANDLE descLoc);

	void close();

	ID3D12PipelineState* getPSO() const;
	rootsignature* getRootSig() const;

	void guiSetting();
};