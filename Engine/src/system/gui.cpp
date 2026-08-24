#include <system\gui.hpp>
#include <system/eventProf.hpp>
#include <render/shader.hpp>
#include <render/renderer.hpp>
#include <render/framebuffer.hpp>
#include <render/pipelinestate.hpp>
#include <render/mesh.hpp>
#include <world/world.hpp>

#include <dxgi1_6.h>

#include <algorithm>
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

bool gui::init(void* hwnd, ID3D12Device* device, Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> allocatedGuiHeap, const descriptor& fontDesc)
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
        fontDesc.getCPUHandle(),
        fontDesc.getHandle());

    return true;
}

static bool showWindow;
static bool showShadersWindow = false;
static bool showDebugWindow = false;
static bool showEventViewerWindow = false;

#include <imgui/imgui_internal.h>

void gui::render(ID3D12GraphicsCommandList* cmdList)
{
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Window", &showWindow, ImGuiWindowFlags_MenuBar);

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Tools"))
        {
            ImGui::MenuItem("ShaderViewer", NULL, &showShadersWindow);
            ImGui::MenuItem("DebugWindow", NULL, &showDebugWindow);
#if ENGINE_DEBUG_GPUPROF
            ImGui::MenuItem("EventViewer", NULL, &showEventViewerWindow);
#endif

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    ImGui::BeginTabBar("Category");

    if (ImGui::BeginTabItem("Render"))
    {
        e_globRenderer.guiSetting();

        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Shader"))
    {
        shaders::guiShaderSetting();

        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("PSO"))
    {
        render::guiPSOSetting();

        ImGui::EndTabItem();
    }

#if ENGINE_DEBUG_MESH
    if (ImGui::BeginTabItem("Mesh"))
    {
        bool openDebugClick = false;
        static uint meshID;

        ImGui::BeginChild("left pane", ImVec2(250, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);

        msh::guiMeshSetting(openDebugClick, meshID);

        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("Mesh view pane", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));

        msh::guiMeshViewSetting(openDebugClick, meshID);

        ImGui::EndChild();

        ImGui::EndTabItem();
    }
    else
    {
        msh::closeMeshView();
    }
#endif // #if ENGINE_DEBUG_MESH

    if (ImGui::BeginTabItem("World"))
    {
        e_globWorld.guiSetting();

        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();

    ImGui::End();

    if (showShadersWindow)
    {
        ImGui::Begin("Shader", &showShadersWindow);

        shaders::guiShaderSourceSetting();

        ImGui::End();
    }

    if (showDebugWindow)
    {
        ImGui::Begin("Debug", &showDebugWindow);

        if (ImGui::BeginTabBar("DebugTabs"))
        {
            if (ImGui::BeginTabItem("GBuffer"))
            {
                e_globRenderer.guiGBufferSetting();

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Culling"))
            {
                e_globRenderer.guiCullingSetting();

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("HZB"))
            {
                e_globRenderer.guiHZBSetting();

                ImGui::EndTabItem();
            }

#if ENGINE_DEBUG_GPUPROF || ENGINE_DEBUG_CPUPROF
            if (ImGui::BeginTabItem("Profiler"))
            {
                prof::guiProfilerSetting();

                ImGui::EndTabItem();
            }
#endif // ENGINE_DEBUG_GPUPROF || ENGINE_DEBUG_CPUPROF

            ImGui::EndTabBar();
        }

        ImGui::End();
    }

#if ENGINE_DEBUG_GPUPROF
    if (showEventViewerWindow)
    {
        ImGui::Begin("EventViewer", &showEventViewerWindow);

        prof::guiEventViewerSetting();

        ImGui::End();
    }
#endif // ENGINE_DEBUG_GPUPROF

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

void gui::editintwithrange(std::string str, int* data, int min, int max)
{
    int integer = *data;
    if (ImGui::InputInt(str.c_str(), &integer))
    {
        *data = std::clamp(integer, min, max);
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