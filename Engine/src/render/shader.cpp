#include <render/shader.hpp>
#include <system\defines.hpp>
#include <system/logger.hpp>

#include <d3d12shader.h>
#include <d3dx12.h>

#include <array>

namespace shaders
{
	std::array<shader*, SHADER_END> shaders;

	Microsoft::WRL::ComPtr<IDxcUtils> pUtils;
	Microsoft::WRL::ComPtr<IDxcCompiler3> pCompiler;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> pIncludeHandler;

	void loadShaderSource(DxcBuffer& source, LPCWSTR filePath)
	{
		Microsoft::WRL::ComPtr<IDxcBlobEncoding> pSource = nullptr;
		pUtils->LoadFile(filePath, nullptr, &pSource);
		source.Ptr = pSource->GetBufferPointer();
		source.Size = pSource->GetBufferSize();
		source.Encoding = DXC_CP_ACP; // Assume BOM says UTF8 or UTF16 or this is ANSI text.
	}

	bool compileShader(std::wstring filePath, LPCWSTR entry, LPCWSTR target, DxcBuffer source, Microsoft::WRL::ComPtr<IDxcResult>& pResults)
	{
		std::wstring path = filePath + L".cso";
		LPCWSTR pszArgs[] =
		{
			path.c_str(),
			L"-E", entry,
			L"-T", target,
			L"-Zs",                      // Enable debug information (slim format)
			L"-Qstrip_reflect",          // Strip reflection into a separate blob. 
		};

		pCompiler->Compile(
			&source,                // Source buffer.
			pszArgs,                // Array of pointers to arguments.
			_countof(pszArgs),      // Number of arguments.
			pIncludeHandler.Get(),        // User-provided interface to handle #include directives (optional).
			IID_PPV_ARGS(&pResults) // Compiler output status, buffer, and errors.
		);

		Microsoft::WRL::ComPtr<IDxcBlobUtf8> pErrors = nullptr;
		pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);

		std::string fileStr = std::string(filePath.begin(), filePath.end());
		if (pErrors != nullptr && pErrors->GetStringLength() != 0)
		{
			std::string warning = std::format("Warning on compile shader : %s : \n%s", fileStr.c_str(), pErrors->GetStringPointer());
			TC_LOG_ERROR(warning.c_str());
		}

		HRESULT hrStatus;
		pResults->GetStatus(&hrStatus);
		if (FAILED(hrStatus))
		{
			std::string errorMsg = std::format("Failed to compile Shader! : %s", fileStr.c_str());
			TC_LOG_ERROR(errorMsg.c_str());
			return false;
		}
	}

	bool loadResources()
	{
		DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils));
		DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler));
		pUtils->CreateDefaultIncludeHandler(&pIncludeHandler);
		
		{
			std::wstring filePath = L"data/shader/source/pbr.vs";

			DxcBuffer Source;
			loadShaderSource(Source, filePath.c_str());
			Microsoft::WRL::ComPtr<IDxcResult> pResults;
			compileShader(filePath, L"VSMain", L"vs_6_5", Source, pResults);

			shader* newShader = new shader();

			newShader->setshaderSource(pResults);
			newShader->setInput({
				{"POSITION", DXGI_FORMAT_R32G32B32_FLOAT},
				{"NORMAL", DXGI_FORMAT_R32G32B32_FLOAT}
				});

			shaders[PBR_VS] = newShader;
		}

		{
 		//	shader* newShader = new shader();

			//newShader->load(L"data/shader/pbr.vs.cso");
			//newShader->setInput({
			//	{"POSITION", DXGI_FORMAT_R32G32B32_FLOAT},
			//	{"NORMAL", DXGI_FORMAT_R32G32B32_FLOAT}
			//	});

			//shaders[PBR_VS] = newShader;
		}

		{
			shader* newShader = new shader();

			newShader->load(L"data/shader/pbr.ps.cso");

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
	Microsoft::WRL::ComPtr<IDxcUtils> pUtils;
	DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils));

	Microsoft::WRL::ComPtr<IDxcIncludeHandler> pIncludeHandler;
	pUtils->CreateDefaultIncludeHandler(&pIncludeHandler);

	pIncludeHandler->LoadSource(filename.c_str(), &shaderSource);
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

void shader::setshaderSource(Microsoft::WRL::ComPtr<IDxcResult> result)
{
	Microsoft::WRL::ComPtr<IDxcBlobUtf16> pShaderName = nullptr;
	result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderSource), &pShaderName);
}

D3D12_SHADER_BYTECODE shader::getByteCode() const
{
	return CD3DX12_SHADER_BYTECODE(shaderSource->GetBufferPointer(), shaderSource->GetBufferSize());
}
