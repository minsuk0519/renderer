#pragma once
#include <system\defines.hpp>
#include <render\descriptorheap.hpp>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui\imgui.h>
#include <imgui\imgui_impl_dx12.h>
#include <imgui\imgui_impl_win32.h>

#include <wrl.h>
#include <d3d12.h>

#include <string>
#include <filesystem>

namespace gui
{
	bool init(void* hwnd, ID3D12Device* device, Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> allocatedGuiHeap, const descriptor& fontDesc);

	void render(ID3D12GraphicsCommandList* cmdList);

	void close();

	LRESULT guiInputHandle(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	void text(std::string text);
	void vector4(std::string str, float* data);
	void boolean(std::string str, bool& data);
	void color(std::string str, float* data);

	bool collapsingHeader(std::string str);

	void comboBox(std::string name, const char* const items[], uint size, uint& index);

	void editfloat(std::string str, uint floatNum, float* data, float min, float max);
	void edituint(std::string str, uint* data);

	bool button(std::string str);
}