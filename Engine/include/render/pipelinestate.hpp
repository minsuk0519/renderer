#pragma once

#include <system\defines.hpp>

#include <vector>
#include <wrl.h>

#include <d3d12.h>

class pipelinestate;

namespace render
{
	enum PSO_INDEX
	{
		PSO_PBR,
		PSO_END,
	};

	bool initPSO();
	void cleanUpPSO();

	pipelinestate* getpipelinestate(PSO_INDEX index);
}

class pipelinestate
{
private:
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStateObject;
public:
	bool init(uint VS, uint PS, uint root, std::vector<DXGI_FORMAT> formats, D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveType, D3D12_CULL_MODE cull, bool depth);
	bool initCS(uint CS, uint root);

	ID3D12PipelineState* getPSO() const;
};