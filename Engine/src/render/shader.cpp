#include <render/shader.hpp>
#include <system/logger.hpp>
#include <system/jsonhelper.hpp>
#include <system/gui.hpp>

#include <d3d12shader.h>
#include <d3dx12.h>

#include <unordered_set>

#include <array>
#include <filesystem>

namespace shaders
{
	std::vector<shader*> shaders;

	Microsoft::WRL::ComPtr<IDxcUtils> pUtils;
	Microsoft::WRL::ComPtr<IDxcCompiler3> pCompiler;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> pIncludeHandler;
	Microsoft::WRL::ComPtr<IDxcBlobEncoding> pSource;

	void loadShaderSource(DxcBuffer& source, LPCWSTR filePath)
	{
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

		HRESULT compileStatus = pCompiler->Compile(
			&source,                // Source buffer.
			pszArgs,                // Array of pointers to arguments.
			_countof(pszArgs),      // Number of arguments.
			pIncludeHandler.Get(),  // User-provided interface to handle #include directives (optional).
			IID_PPV_ARGS(&pResults) // Compiler output status, buffer, and errors.
		);

		std::string fileStr = std::string(filePath.begin(), filePath.end());
		if (compileStatus != S_OK)
		{
			std::string warning = std::format("Failed to compile shader : {} : {}", fileStr.c_str(), compileStatus);
			TC_LOG_ERROR(warning.c_str());
		}

		Microsoft::WRL::ComPtr<IDxcBlobUtf8> pErrors = nullptr;
		pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);

		if (pErrors != nullptr && pErrors->GetStringLength() != 0)
		{
			std::string warning = std::format("Warning on compile shader : {} : \n{}", fileStr.c_str(), pErrors->GetStringPointer());
			TC_LOG_WARNING(warning.c_str());
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
		std::vector<shaderJson> shaderJsons;

		readJsonBuffer(shaderJsons, JSON_FILE_NAME::SHADER_FILE);

		DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils));
		DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler));
		pUtils->CreateDefaultIncludeHandler(&pIncludeHandler);

		shaders.resize(shaderJsons.size());
		
		for (auto shaderData : shaderJsons)
		{
			std::wstring filePath = std::wstring(shaderData.shaderFile.begin(), shaderData.shaderFile.end());
			std::wstring entryPoint = std::wstring(shaderData.entryPoint.begin(), shaderData.entryPoint.end());
			std::wstring target = std::wstring(shaderData.target.begin(), shaderData.target.end());

			DxcBuffer Source;
			loadShaderSource(Source, filePath.c_str());
			Microsoft::WRL::ComPtr<IDxcResult> pResults;
			compileShader(filePath, entryPoint.c_str(), target.c_str(), Source, pResults);

			shader* newShader = new shader();

			newShader->setshaderSource(pResults, (shaders::SHADER_TYPE)shaderData.shaderType);
			newShader->decipherHLSL();

			shaders[shaderData.shaderIndex] = newShader;
		}

		return true;
	}

	void cleanup()
	{
		for (uint i = 0; i < shaders.size(); ++i)
		{
			shaders[i]->close();
			delete shaders[i];
		}

		shaders.clear();
	}

	shader* getShader(const uint index)
	{
		return shaders[index];
	}

	static char bytes[1024 * 16];
	std::ifstream::pos_type endFile;

	void DirectoryTreeViewRecursive(const std::filesystem::path& path, uint* count, uint& selection_mask, std::filesystem::path& selectedPath)
	{
		ImGuiTreeNodeFlags base_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_SpanFullWidth;

		bool any_node_clicked = false;

		for (const auto& entry : std::filesystem::directory_iterator(path))
		{
			ImGuiTreeNodeFlags node_flags = base_flags;
			const bool is_selected = (selection_mask == *count);
			if (is_selected) node_flags |= ImGuiTreeNodeFlags_Selected;

			std::string name = entry.path().string();

			auto lastSlash = name.find_last_of("/\\");
			lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
			name = name.substr(lastSlash, name.size() - lastSlash);

			bool entryIsFile = !std::filesystem::is_directory(entry.path());
			if (entryIsFile) node_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

			bool node_open = ImGui::TreeNodeEx((void*)(count), node_flags, name.c_str());

			if (ImGui::IsItemClicked())
			{
				selection_mask = *count;
				any_node_clicked = true;

				if (entryIsFile)
				{
					selectedPath = entry.path();
				}
				else
				{
					selectedPath.clear();
				}
			}

			(*count)--;

			if (!entryIsFile)
			{
				if (node_open)
				{
					DirectoryTreeViewRecursive(entry.path(), count, selection_mask, selectedPath);

					ImGui::TreePop();
				}
				else
				{
					for (const auto& e : std::filesystem::recursive_directory_iterator(entry.path())) (*count)--;
				}
			}
		}
	}

	void guiSetting()
	{
		uint32_t count = 0;
		for (const auto& entry : std::filesystem::recursive_directory_iterator(config::shaderBasePath)) count++;

		static uint selection_mask = 0;
		static uint prev_selection_mask = 0;
		static std::filesystem::path selectedPath;

		ImGui::BeginChild("left pane", ImVec2(150, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);

		DirectoryTreeViewRecursive(config::shaderBasePath, &count, selection_mask, selectedPath);

		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginChild("item view", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));

		if (!selectedPath.empty())
		{
			if (prev_selection_mask != selection_mask)
			{
				memset(bytes, '\0', endFile);

				std::ifstream ifs(selectedPath.c_str(), std::ios::in | std::ios::binary | std::ios::ate);

				endFile = ifs.tellg();
				ifs.seekg(0, std::ios::beg);

				ifs.read(bytes, endFile);

				prev_selection_mask = selection_mask;
			}

			static ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;
			ImGui::InputTextMultiline("##source", bytes, IM_ARRAYSIZE(bytes), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 32), flags);

			if (ImGui::Button("Save"))
			{
				std::ofstream ofs(selectedPath.c_str(), std::ios::out | std::ios::binary | std::ios::ate);

				ofs.write(bytes, (int)strlen(bytes));
			}

			ImGui::SameLine();
		}

		ImGui::EndChild();
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

void shader::setshaderSource(Microsoft::WRL::ComPtr<IDxcResult> result, shaders::SHADER_TYPE shaderType)
{
	Microsoft::WRL::ComPtr<IDxcBlobUtf16> pShaderName = nullptr;
	result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderSource), &pShaderName);

	type = shaderType;
}

void shader::decipherHLSL()
{
	//get desaseembly from program
	DxcBuffer source;
	source.Ptr = shaderSource->GetBufferPointer();
	source.Size = shaderSource->GetBufferSize();

	Microsoft::WRL::ComPtr<IDxcResult> pResults;
	shaders::pCompiler->Disassemble(&source, IID_PPV_ARGS(&pResults));
	Microsoft::WRL::ComPtr<IDxcBlobUtf8> pText = nullptr;
	pResults->GetOutput(DXC_OUT_DISASSEMBLY, IID_PPV_ARGS(&pText), nullptr);
	std::string sourceString;
	sourceString.assign(reinterpret_cast<const char*>(pText->GetBufferPointer()), pText->GetBufferSize());

	//input signature
	{
		auto find = sourceString.find("Input signature:") + 1;
		find = sourceString.find("-------------------- ----- ------ -------- -------- ------- ------", find);
		find = sourceString.find("; ", find) + 1;

		while (true)
		{
			auto find2 = sourceString.find("\n", find) + 1;
			std::string line = sourceString.substr(find, find2 - find);
			find = find2;

			if (line.find(";\n") != std::string::npos)
			{
				break;
			}

			shaders::hlslBuf hlslbuf;

			uint variableIndex = 0;

			find2 = 0;

			while (true)
			{
				find2 = line.find_first_not_of("; ", find2);

				if (find2 == std::string::npos)
				{
					break;
				}

				auto find3 = line.find_first_of(' ', find2);
				std::string str = line.substr(find2, find3 - find2);

				if (variableIndex == 0)
				{
					hlslbuf.name = str;
				}
				if (variableIndex == 2)
				{
					hlslbuf.data = str.size();
				}
				if (variableIndex == 3)
				{
					hlslbuf.loc = std::stoi(str);
				}
				find2 = find3;

				++variableIndex;
			}

			bufData.inputContainer.push_back(hlslbuf);
		}
	}

	//output signature
	{
		auto find = sourceString.find("Output signature:") + 1;
		find = sourceString.find("-------------------- ----- ------ -------- -------- ------- ------", find);
		find = sourceString.find("; ", find) + 1;

		while (true)
		{
			auto find2 = sourceString.find("\n", find) + 1;
			std::string line = sourceString.substr(find, find2 - find);
			find = find2;

			if (line.find(";\n") != std::string::npos)
			{
				break;
			}

			shaders::hlslBuf hlslbuf;

			uint variableIndex = 0;

			find2 = 0;

			while (true)
			{
				find2 = line.find_first_not_of("; ", find2);

				if (find2 == std::string::npos)
				{
					break;
				}

				auto find3 = line.find_first_of(' ', find2);
				std::string str = line.substr(find2, find3 - find2);

				if (variableIndex == 0)
				{
					hlslbuf.name = str;
				}
				if (variableIndex == 2)
				{
					hlslbuf.data = str.size();
				}
				if (variableIndex == 3)
				{
					hlslbuf.loc = std::stoi(str);
				}
				find2 = find3;

				++variableIndex;
			}

			bufData.outputContainer.push_back(hlslbuf);
		}
	}

	//constant buffer
	{
		auto find = sourceString.find("Resource Bindings:") + 1;
		find = sourceString.find("------------------------------ ---------- ------- ----------- ------- -------------- ------", find);
		find = sourceString.find("; ", find) + 1;

		bool skipConstant = false;

		while (true)
		{
			auto find2 = sourceString.find("\n", find) + 1;
			std::string line = sourceString.substr(find, find2 - find);
			find = find2;

			if (line.find(";\n") != std::string::npos)
			{
				break;
			}

			if (line.find("ViewId state:") != std::string::npos)
			{
				skipConstant = true;
				break;
			}

			shaders::hlslBuf hlslbuf;

			uint variableIndex = 0;

			find2 = 0;

			while (true)
			{
				find2 = line.find_first_not_of("; ", find2);

				if (find2 == std::string::npos)
				{
					break;
				}

				auto find3 = line.find_first_of(' ', find2);
				std::string str = line.substr(find2, find3 - find2);

				if (variableIndex == 4)
				{
					std::string cBufferLoc = str.substr(2);
					hlslbuf.loc = std::stoi(cBufferLoc);
					bufData.constantContainer.push_back(hlslbuf);
					break;
				}
				find2 = find3;

				++variableIndex;
			}
		}

		if (!skipConstant)
		{

			find = sourceString.find("\n%dx.alignment.legacy") + 1;

			uint cbufferNum = 0;
			while (true)
			{
				auto find2 = sourceString.find("\n", find) + 1;
				std::string line = sourceString.substr(find, find2 - find);
				find = find2;

				if (line.size() == 1)
				{
					break;
				}

				find2 = 0;

				{
					auto find3 = line.find("%dx.alignment.legacy") + 21;
					find2 = line.find(" ");

					std::string Name = line.substr(find3, find2 - find3);

					if (Name.find("struct") != std::string::npos)
					{
						continue;
					}

					bufData.constantContainer[cbufferNum].name = Name;

					uint size = 0;

					find2 = line.find("type { ") + 7;
					while (true)
					{
						line = line.substr(find2);
						find3 = line.find_first_of(", }");
						std::string constantName = line.substr(0, find3);

						if (constantName.find("i32") != std::string::npos || constantName.find("float") != std::string::npos)
						{
							size += 4;
							find2 = line.find_first_not_of(", }", find3);
							continue;
						}

						if (constantName.size() < 3)
						{
							break;
						}

						//get size from struct
						{
							auto constantSizePos = sourceString.find(constantName + " = type { ") + constantName.size() + 11;
							std::string constantSizeString = sourceString.substr(constantSizePos);

							uint stringIndex = 0;

							uint cumulate = 1;

							while (true)
							{
								char c = constantSizeString[stringIndex];

								if (c == '}') break;
								else if (c == 'f')
								{
									size += cumulate * 4;
									cumulate = 1;
									stringIndex += 5;
								}
								else if (c == 'i')
								{
									size += cumulate * 4;
									cumulate = 1;
									stringIndex += 3;
								}
								uint num = 0;
								while (c >= '0' && c <= '9')
								{
									uint i = static_cast<uint>(c - '0');
									num = num * 10 + i;
									++stringIndex;
									c = constantSizeString[stringIndex];
								}
								if (num != 0) cumulate *= num;

								++stringIndex;
							}
						}

						find2 = line.find_first_not_of(", }", find3);
					}
					bufData.constantContainer[cbufferNum].data = size;
				}
				++cbufferNum;
			}
		}
	}

	//set input signature
	if (type == shaders::SHADER_VS)
	{
		for (auto& input : bufData.inputContainer)
		{
			DXGI_FORMAT format;

			if (input.data == 4)
			{
				format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			}
			else if (input.data == 3)
			{
				format = DXGI_FORMAT_R32G32B32_FLOAT;
			}
			else if (input.data == 2)
			{
				format = DXGI_FORMAT_R32G32_FLOAT;
			}
			else if (input.data == 1)
			{
				format = DXGI_FORMAT_R32_FLOAT;
			}

			inputs.push_back({ input.name.c_str(), 0, format, input.loc, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});
		}
	}
}

D3D12_SHADER_BYTECODE shader::getByteCode() const
{
	return CD3DX12_SHADER_BYTECODE(shaderSource->GetBufferPointer(), shaderSource->GetBufferSize());
}

shaders::SHADER_TYPE shader::getType() const
{
	return type;
}
