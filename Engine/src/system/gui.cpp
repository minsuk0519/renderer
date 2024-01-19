#include <system\gui.hpp>

#include <imgui\imgui.h>
#include <imgui\imgui_impl_dx12.h>
#include <imgui\imgui_impl_win32.h>

#include <dxgi1_6.h>

#include <filesystem>
#include <fstream>

namespace gui
{
    constexpr uint GUI_FRAMES_NUM = 3;

    struct FrameContext
    {
        ID3D12CommandAllocator* CommandAllocator;
        UINT64                  FenceValue;
    };

    //allocated from other interface
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> guiHeap;
};


bool gui::init(void* hwnd, ID3D12Device* device, Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> allocatedGuiHeap)
{
    gui::guiHeap = allocatedGuiHeap;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX12_Init(device, GUI_FRAMES_NUM,
        DXGI_FORMAT_R8G8B8A8_UNORM, gui::guiHeap.Get(),
        gui::guiHeap->GetCPUDescriptorHandleForHeapStart(),
        gui::guiHeap->GetGPUDescriptorHandleForHeapStart());

    return true;
}

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

std::string basePath = "data/shader/source";
char bytes[1024 * 16];
std::ifstream::pos_type fileSize;

void gui::render(ID3D12GraphicsCommandList* cmdList)
{
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Window");

    ImGui::BeginTabBar("Category");

    if (ImGui::BeginTabItem("Shader"))
    {
        //render::guiSetting();
        {
            uint32_t count = 0;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(basePath)) count++;

            static uint selection_mask = 0;
            static uint prev_selection_mask = 0;
            static std::filesystem::path selectedPath;

            ImGui::BeginChild("left pane", ImVec2(150, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);

            DirectoryTreeViewRecursive(basePath, &count, selection_mask, selectedPath);

            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("item view", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));

            if (!selectedPath.empty())
            {
                if(prev_selection_mask != selection_mask)
                {
                    std::ifstream ifs(selectedPath.c_str(), std::ios::in | std::ios::binary | std::ios::ate);

                    fileSize = ifs.tellg();
                    ifs.seekg(0, std::ios::beg);

                    ifs.read(bytes, fileSize);

                    prev_selection_mask = selection_mask;
                }

                static ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;
                ImGui::InputTextMultiline("##source", bytes, IM_ARRAYSIZE(bytes), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 32), flags);
            }

            ImGui::EndChild();
        }

        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();

    ImGui::End();

    ImGui::Render();

    cmdList->SetDescriptorHeaps(1, gui::guiHeap.GetAddressOf());

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);
}

void gui::close()
{
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT gui::guiInputHandle(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
}

void gui::text(std::string text)
{
    ImGui::Text(text.c_str());
}

void gui::vector4(std::string str, float* data)
{
    ImGui::DragFloat4(str.c_str(), data);
}

void gui::boolean(std::string str, bool& data)
{
    ImGui::Checkbox(str.c_str(), &data);
}

void gui::color(std::string str, float* data)
{
    ImGui::ColorEdit4(str.c_str(), data);
}

bool gui::collapsingHeader(std::string str)
{
    return ImGui::CollapsingHeader(str.c_str());
}

void gui::editfloat(std::string str, uint floatNum, float* data, float min, float max)
{
    float velocity = (min == max) ? 0.01f : (max - min) * 0.001f;

    if (floatNum == 1) ImGui::DragFloat(str.c_str(), data, velocity, min, max);
    else if (floatNum == 2) ImGui::DragFloat2(str.c_str(), data, velocity, min, max);
    else if (floatNum == 3) ImGui::DragFloat3(str.c_str(), data, velocity, min, max);
    else if (floatNum == 4) ImGui::DragFloat4(str.c_str(), data, velocity, min, max);
}

void gui::edituint(std::string str, uint* data)
{
    int integer = *data;
    if (ImGui::InputInt(str.c_str(), &integer))
    {
        *data = integer;
    }
}

void gui::comboBox(std::string name, const char* const items[], uint size, uint& index)
{
    if (ImGui::BeginCombo(name.c_str(), items[index]))
    {
        for (uint n = 0; n < size; ++n)
        {
            bool is_selected = (index == n);
            if (ImGui::Selectable(items[n], is_selected))
            {
                index = n;
                if (is_selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
        }
        ImGui::EndCombo();
    }
}

bool gui::button(std::string str)
{
    return ImGui::Button(str.c_str());
}