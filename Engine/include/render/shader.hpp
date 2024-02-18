#pragma once

#include <system\defines.hpp>

#include <string>
#include <vector>

#include <wrl.h>
#include <d3d12.h>
#include <dxcapi.h>

class shader;

namespace shaders
{
	enum SHADER_TYPE
	{
		SHADER_VS = 0,
		SHADER_PS,
	};

	bool loadResources();
	void cleanup();

	shader* getShader(const uint index);

	struct hlslBuf
	{
		std::string name;
		//size of data or number of channel
		uint data;
		uint loc;
	};

	struct hlslData
	{
		std::vector<hlslBuf> inputContainer;
		std::vector<hlslBuf> outputContainer;

		std::vector<hlslBuf> constantContainer;
		std::vector<hlslBuf> textureContainer;
		std::vector<hlslBuf> samplerContainer;
	};

	void guiSetting();

	uint getShaderLocFromName(std::string name);
};

class shader
{
public:
	void load(std::wstring filename);
	void close();

	void setshaderSource(Microsoft::WRL::ComPtr<IDxcResult> result, shaders::SHADER_TYPE shaderType);

	void decipherHLSL();

	std::vector<D3D12_INPUT_ELEMENT_DESC> inputs;
	D3D12_SHADER_BYTECODE getByteCode() const;

	shaders::SHADER_TYPE getType() const;

	shaders::hlslData bufData;

private:
	Microsoft::WRL::ComPtr<IDxcBlob> shaderSource;
	shaders::SHADER_TYPE type;
};