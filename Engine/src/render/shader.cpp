#include <render/shader.hpp>
#include <system/logger.hpp>
#include <system/jsonhelper.hpp>
#include <system/gui.hpp>
#include <render/shader_defines.hpp>

#include <d3d12shader.h>
#include <d3dx12.h>

#include <unordered_set>

#include <array>
#include <filesystem>
#include <stack>

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

			newShader->setshaderSource(pResults, (shaders::SHADER_TYPE)shaderData.shaderType, shaderData.entryPoint);
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

	void guiShaderSourceSetting()
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

	void guiShaderSetting()
	{
		uint shaderIndex = 0;
		uint shaderNum = shaders.size();

		for (shaderIndex = 0; shaderIndex < shaderNum; ++shaderIndex)
		{
			shader* sh = shaders[shaderIndex];

			if (ImGui::TreeNode(sh->getName().c_str()))
			{
				if (!sh->bufData.constantContainer.empty())
				{
					ImGui::BulletText("Constant");
					for (auto constant : sh->bufData.constantContainer)
					{
						ImGui::Text(("Name : " + constant.name).c_str());
						ImGui::Text("size : %d, loc : %d", constant.data, constant.loc);
					}
				}

				if (!sh->bufData.inputContainer.empty())
				{
					ImGui::BulletText("Input");
					for (auto input : sh->bufData.inputContainer)
					{
						ImGui::Text(("Name : " + input.name).c_str());
						ImGui::Text("size : float%d, loc : %d", input.data, input.loc);
					}
				}

				if (!sh->bufData.outputContainer.empty())
				{
					ImGui::BulletText("Output");
					for (auto output : sh->bufData.outputContainer)
					{
						ImGui::Text(("Name : " + output.name).c_str());
						ImGui::Text("size : float%d, loc : %d", output.data, output.loc);
					}
				}

				if (!sh->bufData.samplerContainer.empty())
				{
					ImGui::BulletText("Sampler");
					for (auto sampler : sh->bufData.samplerContainer)
					{
						ImGui::Text(("Name : " + sampler.name).c_str());
						ImGui::Text("loc : %d", sampler.loc);
					}
				}

				if (!sh->bufData.textureContainer.empty())
				{
					ImGui::BulletText("Texture");
					for (auto texture : sh->bufData.textureContainer)
					{
						ImGui::Text(("Name : " + texture.name).c_str());
						ImGui::Text("loc : %d", texture.loc);
					}
				}

				ImGui::TreePop();
			}
		}
	}

	uint getShaderLocFromName(std::string name)
	{
		uint num = 0;
		if (name[0] == 'c' && name[1] == 'b')
		{
			num = std::stoi(name.substr(2));
			num = GET_HLSL_LOC_CBV(num);
		}
		else if (name[0] == 't')
		{
			num = std::stoi(name.substr(1));
			num = GET_HLSL_LOC_SRV(num);
		}
		else if (name[0] == 's')
		{
			num = std::stoi(name.substr(1));
			num = GET_HLSL_LOC_SAMP(num);
		}
		else if (name[0] == 'u')
		{
			num = std::stoi(name.substr(1));
			num = GET_HLSL_LOC_UAV(num);
		}
		else
		{
			TC_BREAK();
		}

		return num;
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

void shader::setshaderSource(Microsoft::WRL::ComPtr<IDxcResult> result, shaders::SHADER_TYPE shaderType, std::string shaderName)
{
	Microsoft::WRL::ComPtr<IDxcBlobUtf16> pShaderName = nullptr;
	result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderSource), &pShaderName);

	type = shaderType;
	name = shaderName;
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

	if (type != shaders::SHADER_CS)
	{
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
					if (line.find("no parameters") != std::string::npos)
					{
						break;
					}

					auto find3 = line.find_first_of(' ', find2);
					std::string str = line.substr(find2, find3 - find2);

					if (variableIndex == 0)
					{
						hlslbuf.name = str;
					}
					else if (variableIndex == 2)
					{
						hlslbuf.data = str.size();
					}
					else if (variableIndex == 3)
					{
						hlslbuf.loc = std::stoi(str);
					}
					else if (variableIndex == 5)
					{
						uint channelSize = hlslbuf.data;

						if (str == "uint")
						{
							if (channelSize == 4)
							{
								hlslbuf.data = DXGI_FORMAT_R32G32B32A32_UINT;
							}
							else if (channelSize == 3)
							{
								hlslbuf.data = DXGI_FORMAT_R32G32B32_UINT;
							}
							else if (channelSize == 2)
							{
								hlslbuf.data = DXGI_FORMAT_R32G32_UINT;
							}
							else if (channelSize == 1)
							{
								hlslbuf.data = DXGI_FORMAT_R32_UINT;
							}
						}
						else
						{
							if (channelSize == 4)
							{
								hlslbuf.data = DXGI_FORMAT_R32G32B32A32_FLOAT;
							}
							else if (channelSize == 3)
							{
								hlslbuf.data = DXGI_FORMAT_R32G32B32_FLOAT;
							}
							else if (channelSize == 2)
							{
								hlslbuf.data = DXGI_FORMAT_R32G32_FLOAT;
							}
							else if (channelSize == 1)
							{
								hlslbuf.data = DXGI_FORMAT_R32_FLOAT;
							}
						}
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
				if (line.find("no parameters") != std::string::npos)
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

				//if (variableIndex == 3)
				//{
				//	std::string cBufferLoc;

				//	if (str.find("CB") != std::string::npos)
				//	{
				//		cBufferLoc = str.substr(2);
				//	}
				//	else if(str.find("S") != std::string::npos)
				//	{
				//		cBufferLoc = str.substr(1);
				//	}
				//	else
				//	{
				//		cBufferLoc = str.substr(1);
				//	}
				//	hlslbuf.loc = std::stoi(cBufferLoc);
				//}

				if (variableIndex == 4)
				{
					std::string cBufferLoc;

					if (str.find("cb") != std::string::npos)
					{
						cBufferLoc = str.substr(2);
						hlslbuf.loc = std::stoi(cBufferLoc);
						bufData.constantContainer.push_back(hlslbuf);
					}
					else if (str.find("s") != std::string::npos)
					{
						cBufferLoc = str.substr(1);
						hlslbuf.loc = std::stoi(cBufferLoc);
						bufData.samplerContainer.push_back(hlslbuf);
					}
					else if(str.find("t") != std::string::npos)
					{
						cBufferLoc = str.substr(1);
						hlslbuf.loc = std::stoi(cBufferLoc);
						bufData.textureContainer.push_back(hlslbuf);
					}
					else
					{
						TC_ASSERT(type == shaders::SHADER_CS);
						cBufferLoc = str.substr(1);
						hlslbuf.loc = std::stoi(cBufferLoc);
						bufData.outputContainer.push_back(hlslbuf);
					}

					break;
				}

				find2 = find3;

				++variableIndex;
			}
		}

		uint cbufferNum = 0;
		if (bufData.constantContainer.size() != 0)
		{
			find = sourceString.find("\n%dx.alignment.legacy") + 1;

			while (true)
			{
				auto find2 = sourceString.find("\n", find) + 1;
				std::string line = sourceString.substr(find, find2 - find);

				if (auto findcb = line.find("%cb_"); findcb != std::string::npos)
				{
					find = findcb;
					break;
				}

				find = find2;

				if (line.size() == 1)
				{
					break;
				}

				find2 = 0;
				
				auto find3 = line.find("%dx.alignment.legacy");

				if (find3 == std::string::npos)
				{
					break;
				}
				
				find3 += 21;

				find2 = line.find(" ");

				std::string Name = line.substr(find3, find2 - find3);

				if (Name.find("struct") != std::string::npos)
				{
					continue;
				}

				if (Name.empty())
				{
					break;
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
				
				++cbufferNum;
			}

			while (bufData.constantContainer.size() != cbufferNum)
			{
				find = sourceString.find("\n%cb_", find) + 1;
				
				auto find2 = sourceString.find("\n", find) + 1;
				std::string line = sourceString.substr(find, find2 - find);
				find = find2 - 1;

				auto find3 = line.find("%cb_");

				find2 = line.find(" ");

				std::string Name = line.substr(1, find2 - 1);

				bufData.constantContainer[cbufferNum].name = Name;

				find2 = line.find("type { ") + 7;

				uint size = 0;

				line = line.substr(find2);

				{
					uint stringIndex = 0;

					std::stack<uint> stack;
					std::stack<uint> stack2;
					std::stack<uint> stack3;

					while (true)
					{
						char c = line[stringIndex];

						if (c == ' ')
						{
							++stringIndex;
							continue;
						}

						uint num = 0;
						if (c == '}') break;
						else if (c == 'f')
						{
							num = 1;
							stringIndex += 4;
						}
						else if (c == 'i')
						{
							num = 1;
							stringIndex += 2;
						}
						while (c >= '0' && c <= '9')
						{
							uint i = static_cast<uint>(c - '0');
							num = num * 10 + i;
							++stringIndex;
							c = line[stringIndex];
						}
						if(num != 0) stack.push(num);

						if (c == ']' || c == '>')
						{
							uint cumulate = stack.top();
							stack.pop();

							uint bracket = stack3.top();
							stack3.pop();

							while (stack.size() > bracket)
							{
								uint n = stack.top();
								stack.pop();

								uint op = stack2.top();
								stack2.pop();

								if (op == 0)
								{
									cumulate *= n;
								}
								else
								{
									cumulate += n;
								}
							}

							stack.push(cumulate);
						}
						else if (c == '[' || c == '<')
						{
							stack3.push(stack.size());
						}
						else if (c == ',')
						{
							stack2.push(1);
						}
						else if (c == 'x')
						{
							stack2.push(0);
						}

						++stringIndex;
					}

					uint cumulate = stack.top();
					stack.pop();

					while (!stack.empty())
					{
						uint n = stack.top();
						stack.pop();

						uint op = stack2.top();
						stack2.pop();

						if (op == 0)
						{
							cumulate *= n;
						}
						else
						{
							cumulate += n;
						}
					}

					size = cumulate;
				}

				bufData.constantContainer[cbufferNum].data = size;

				++cbufferNum;
			}
		}
		TC_ASSERT(bufData.constantContainer.size() == cbufferNum);
	}

	//set input signature
	if (type == shaders::SHADER_VS)
	{
		uint inputContainerSize = bufData.inputContainer.size();
		for (uint i = 0; i < inputContainerSize; ++i)
		{
			DXGI_FORMAT format = (DXGI_FORMAT)bufData.inputContainer[i].data;

			if (bufData.inputContainer[i].name[0] == 'V')
			{
				inputs.push_back({ bufData.inputContainer[i].name.c_str(), 0, format, bufData.inputContainer[i].loc, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});
			}
			else
			{
				uint allignOffset = 0;
				if (auto pos = bufData.inputContainer[i].name.find("_"); pos != std::string::npos)
				{
					std::string inputName = bufData.inputContainer[i].name.substr(0, pos);

					for (uint j = 0; j < i; ++j)
					{
						if (bufData.inputContainer[j].name.find(inputName) != std::string::npos)
						{
							bufData.inputContainer[i].loc = bufData.inputContainer[j].loc;

							if (format == DXGI_FORMAT_R32G32B32_FLOAT)
							{
								allignOffset += 12;
							}
							else if (format == DXGI_FORMAT_R32G32B32A32_FLOAT)
							{
								allignOffset += 16;
							}
							else if (format == DXGI_FORMAT_R32G32_FLOAT)
							{
								allignOffset += 8;
							}
							else if (format == DXGI_FORMAT_R32_FLOAT)
							{
								allignOffset += 4;
							}
						}
					}
				}
				inputs.push_back({ bufData.inputContainer[i].name.c_str(), 0, format, bufData.inputContainer[i].loc, allignOffset, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1});
			}
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

std::string shader::getName() const
{
	return name;
}
