#include <system\gui.hpp>
#include <render/shader.hpp>
#include <render/renderer.hpp>
#include <render/framebuffer.hpp>
#include <render/pipelinestate.hpp>
#include <render/mesh.hpp>
#include <world/world.hpp>

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

constantbuffer* debugProjectionBuffer;
descriptor debugProjectionDesc;
float meshDebugDrawCamArmLength_Default = 2.5f;
DirectX::XMVECTOR meshDebugDrawCamPos_Default = DirectX::XMVECTOR{ 0.0f, 0.0f, meshDebugDrawCamArmLength_Default };
float meshDebugDrawCamArmLength = meshDebugDrawCamArmLength_Default;
DirectX::XMVECTOR meshDebugDrawCamPos = meshDebugDrawCamPos_Default;

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

    debugProjectionBuffer = buf::createConstantBuffer(consts::CONST_PROJ_SIZE);
    debugProjectionDesc = (render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_CONSTANT_TYPE, debugProjectionBuffer));

    DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovRH(DirectX::XMConvertToRadians(45.0f), 1.0f, 0.1f, 10.0f);

    memcpy(debugProjectionBuffer->info.cbvDataBegin, &projection, sizeof(float) * 4 * 4);

    return true;
}

static bool showWindow;
static bool showShadersWindow = false;
static bool showDebugWindow = false;

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

    static bool openDebugWindow = false;
    if (ImGui::BeginTabItem("Mesh"))
    {
        bool openDebugClick = false;
        static uint meshID;

        ImGui::BeginChild("left pane", ImVec2(250, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);

        msh::guiMeshSetting(openDebugClick, meshID);

        if (openDebugClick == true)
        {
            e_globRenderer.debugFrameBufferRequest(meshID, debugProjectionDesc.getHandle().ptr);
            openDebugWindow = true;
        }

        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("Mesh view pane", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));

        if (openDebugWindow)
        {
            framebuffer* fbo = e_globRenderer.getDebugFrameBuffer();
            ImGui::Image((ImTextureID)(fbo->getDescHandle(0).ptr), ImVec2(256.0f, 256.0f), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImGui::GetStyleColorVec4(ImGuiCol_Border));

            static float x = 0;
            static float y = PI / 2.0f;

            bool changed = false;

            ImGui::SameLine();
            ImGui::BeginChild("ArrowButtons", ImVec2(70.0f, 70.0f));
            ImGui::Columns(3, nullptr, false);
            ImGui::PushButtonRepeat(true);
            for (int i = 0; i < 9; i++)
            {
                if (i == 0) if (ImGui::Button("+##ZoomIn")) { meshDebugDrawCamArmLength -= 0.1f; changed = true; }
                if (i == 1) if (ImGui::ArrowButton("meshView##Up", ImGuiDir_Up)) { y -= 0.1f; changed = true; }
                if (i == 2) if (ImGui::Button("-##ZoomOut")) { meshDebugDrawCamArmLength += 0.1f; changed = true; }
                if (i == 3) if (ImGui::ArrowButton("meshView##Left", ImGuiDir_Left)) { x -= 0.1f; changed = true; }
                if (i == 5) if (ImGui::ArrowButton("meshView##Right", ImGuiDir_Right)) { x += 0.1f; changed = true; }
                if (i == 7) if (ImGui::ArrowButton("meshView##Down", ImGuiDir_Down)) { y += 0.1f; changed = true; }
                ImGui::NextColumn();
            }
            ImGui::PopButtonRepeat();

            ImGui::EndChild();

            if (ImGui::Button("Reset##MeshView"))
            {
                meshDebugDrawCamPos = meshDebugDrawCamPos_Default;
                x = 0;
                y = PI / 2.0f;
            }

            if (y > PI) y = PI - 0.01f;
            if (y < 0.0f) y = 0.01f;

            if(ImGui::Button("Close##MeshView"))
            {
                openDebugWindow = false;
            }

            if (changed)
            {
                float xPos = std::sinf(x) * std::sinf(y) * meshDebugDrawCamArmLength;
                float yPos = std::cosf(y) * meshDebugDrawCamArmLength;
                float zPos = std::cosf(x) * std::sinf(y) * meshDebugDrawCamArmLength;
                meshDebugDrawCamPos = DirectX::XMVECTOR{ xPos, yPos, zPos };
                e_globRenderer.debugFrameBufferRequest(meshID, debugProjectionDesc.getHandle().ptr);
            }

            DirectX::XMVECTOR forward = DirectX::XMVector3Normalize(DirectX::XMVectorNegate(meshDebugDrawCamPos));
            DirectX::XMVECTOR globUp = DirectX::XMVECTOR{ 0.0f, 1.0f, 0.0f };

            DirectX::XMVECTOR right = DirectX::XMVector3Cross(forward, globUp);
            DirectX::XMVECTOR up = DirectX::XMVector3Cross(right, forward);

            DirectX::XMMATRIX view = DirectX::XMMatrixLookToRH(meshDebugDrawCamPos, forward, up);

            memcpy(debugProjectionBuffer->info.cbvDataBegin + sizeof(float) * 4 * 4, &view, sizeof(float) * 4 * 4);
        }

        ImGui::EndChild();

        ImGui::EndTabItem();
    }
    else
    {
        openDebugWindow = false;
    }

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

        ImGui::Text("GbufferPosTex");
        framebuffer* fbo = e_globRenderer.getFrameBuffer();
        ImGui::Image((ImTextureID)(fbo->getDescHandle(0).ptr), ImVec2(160.0f, 90.0f), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImGui::GetStyleColorVec4(ImGuiCol_Border));
        ImGui::Text("GbufferNormTex");
        ImGui::Image((ImTextureID)(fbo->getDescHandle(1).ptr), ImVec2(160.0f, 90.0f), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImGui::GetStyleColorVec4(ImGuiCol_Border));
        ImGui::Text("ObjectID");
        ImGui::Image((ImTextureID)(fbo->getDescHandle(2).ptr), ImVec2(160.0f, 90.0f), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImGui::GetStyleColorVec4(ImGuiCol_Border));
        ImGui::Text("SSAOTex");
        ImGui::Image((ImTextureID)(e_globRenderer.ssaoDesc[0].getHandle().ptr), ImVec2(160.0f, 90.0f), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImGui::GetStyleColorVec4(ImGuiCol_Border));
        ImGui::Image((ImTextureID)(e_globRenderer.ssaoDesc[2].getHandle().ptr), ImVec2(160.0f, 90.0f), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImGui::GetStyleColorVec4(ImGuiCol_Border));

        ImGui::End();
    }

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