#include <render/shader.hpp>
#include <system\defines.hpp>

#include <d3dcompiler.h>
#include <d3dx12.h>

#include <array>

namespace shaders
{
	std::array<shader*, SHADER_END> shaders;

	bool loadResources()
	{
		{
			shader* newShader = new shader();

			newShader->load(L"shader/pbr.vs.cso");
			newShader->setInput({
				{"POSITION", DXGI_FORMAT_R32G32B32_FLOAT},
				{"NORMAL", DXGI_FORMAT_R32G32B32_FLOAT}
				});

			shaders[PBR_VS] = newShader;
		}

		{
			shader* newShader = new shader();

			newShader->load(L"shader/pbr.ps.cso");

			shaders[PBR_PS] = newShader;
		}

		return true;
	}

	void cleanup()
	{
		for (uint i = 0; i < SHADER_END; ++i)
		{
			shaders[i]->close();
			delete shaders[i];
		}
	}

	shader* getShader(const SHADER_INDEX index)
	{
		return shaders[index];
	}
}

void shader::load(std::wstring filename)
{
	D3DReadFileToBlob(filename.c_str(), &shaderSource);
}

void shader::close()
{
	inputs.clear();

	shaderSource->Release();
}

void shader::setInput(std::vector<inputDesc> input)
{
	uint index = 0;
	for (auto desc : input)
	{
		inputs.push_back({ desc.name, 0, desc.format, index, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
		++index;
	}
}

D3D12_SHADER_BYTECODE shader::getByteCode() const
{
	return CD3DX12_SHADER_BYTECODE(shaderSource.Get());
}
