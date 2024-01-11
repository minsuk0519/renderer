#pragma once

#include <string>

#include <wrl.h>

#include <d3d12.h>

#include <vector>

class shader;

namespace shaders
{
	enum SHADER_TYPE
	{
		SHADER_VS = 0,
		SHADER_PS,
	};

	enum SHADER_INDEX
	{
		PBR_VS = 0,
		PBR_PS,
		SHADER_END,
	};

	bool loadResources();
	void cleanup();

	shader* getShader(const SHADER_INDEX index);
};

class shader
{
public:
	struct inputDesc
	{
		LPCSTR name;
		DXGI_FORMAT format;
	};

	void load(std::wstring filename);
	void close();

	//only for the vertex shader
	void setInput(std::vector<inputDesc> input);

	std::vector<D3D12_INPUT_ELEMENT_DESC> inputs;
	D3D12_SHADER_BYTECODE getByteCode() const;
private:
	Microsoft::WRL::ComPtr<ID3DBlob> shaderSource;
	shaders::SHADER_TYPE type;

};