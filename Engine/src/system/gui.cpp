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

#if ENGINE_DEBUG_MESH
buffer* debugProjectionBuffer;
float meshDebugDrawCamArmLength_Default = 2.5f;
DirectX::XMVECTOR meshDebugDrawCamPos_Default = DirectX::XMVECTOR{ 0.0f, 0.0f, meshDebugDrawCamArmLength_Default };
float meshDebugDrawCamArmLength = meshDebugDrawCamArmLength_Default;
DirectX::XMVECTOR meshDebugDrawCamPos = meshDebugDrawCamPos_Default;
#endif // #if ENGINE_DEBUG_MESH

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

#if ENGINE_DEBUG_MESH
    debugProjectionBuffer = e_globBufAllocator.alloc(nullptr, consts::CONST_PROJ_SIZE, 1, buf::GBF_CBV, buf::RESOURCE_UPLOAD);
#endif // #if ENGINE_DEBUG_MESH

    return true;
}

static bool showWindow;
static bool showShadersWindow = false;
static bool showDebugWindow = false;

#if ENGINE_DEBUG_GPUPROF || ENGINE_DEBUG_CPUPROF
static prof::EVENT_INDEX selectedGPUEvent = prof::EVENT_INVALID;
static prof::EVENT_INDEX selectedCPUEvent = prof::EVENT_INVALID;
#endif

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

#if ENGINE_DEBUG_MESH
    static bool openDebugWindow = false;
    if (ImGui::BeginTabItem("Mesh"))
    {
        bool openDebugClick = false;
        static uint meshID;

        ImGui::BeginChild("left pane", ImVec2(250, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);

        msh::guiMeshSetting(openDebugClick, meshID);

        if (openDebugClick == true)
        {
            e_globRenderer.debugFrameBufferRequest(meshID, debugProjectionBuffer->getDesc(buf::GBF_CBV)->getHandle().ptr);
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

            if (changed || openDebugClick)
            {
                float xPos = std::sinf(x) * std::sinf(y) * meshDebugDrawCamArmLength;
                float yPos = std::cosf(y) * meshDebugDrawCamArmLength;
                float zPos = std::cosf(x) * std::sinf(y) * meshDebugDrawCamArmLength;
                meshDebugDrawCamPos = DirectX::XMVECTOR{ xPos, yPos, zPos };
                e_globRenderer.debugFrameBufferRequest(meshID, debugProjectionBuffer->getDesc(buf::GBF_CBV)->getHandle().ptr);

                DirectX::XMVECTOR forward = DirectX::XMVector3Normalize(DirectX::XMVectorNegate(meshDebugDrawCamPos));
                DirectX::XMVECTOR globUp = DirectX::XMVECTOR{ 0.0f, 1.0f, 0.0f };

                DirectX::XMVECTOR right = DirectX::XMVector3Cross(forward, globUp);
                DirectX::XMVECTOR up = DirectX::XMVector3Cross(right, forward);

                DirectX::XMMATRIX view = DirectX::XMMatrixLookToRH(meshDebugDrawCamPos, forward, up);

                DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovRH(DirectX::XMConvertToRadians(45.0f), 1.0f, 0.1f, 10.0f);

                DirectX::XMMATRIX viewProj = DirectX::XMMatrixMultiply(view, projection);

                float* aabbSize = msh::getMesh(meshID)->getData()->boundData.halfExtent;

                float safeExtent[3] =
                {
                    (std::max)(aabbSize[msh::AXIS_X], 1e-6f),
                    (std::max)(aabbSize[msh::AXIS_Y], 1e-6f),
                    (std::max)(aabbSize[msh::AXIS_Z], 1e-6f)
                };

                debugProjectionBuffer->uploadBuffer(sizeof(float) * 4 * 4, 0, &viewProj);
                debugProjectionBuffer->uploadBuffer(sizeof(float) * 3, sizeof(float) * 4 * 4, safeExtent);
            }
        }

        ImGui::EndChild();

        ImGui::EndTabItem();
    }
    else
    {
        openDebugWindow = false;
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
                ImGui::Text("GbufferPosTex");
                framebuffer* fbo = e_globRenderer.getFrameBuffer();
                ImGui::Image((ImTextureID)(fbo->getDescHandle(0).ptr), ImVec2(160.0f, 90.0f), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImGui::GetStyleColorVec4(ImGuiCol_Border));
                ImGui::Text("GbufferNormTex");
                ImGui::Image((ImTextureID)(fbo->getDescHandle(1).ptr), ImVec2(160.0f, 90.0f), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImGui::GetStyleColorVec4(ImGuiCol_Border));
                ImGui::Text("ObjectID");
                ImGui::Image((ImTextureID)(fbo->getDescHandle(2).ptr), ImVec2(160.0f, 90.0f), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImGui::GetStyleColorVec4(ImGuiCol_Border));
                ImGui::Text("SSAOTex");
                //ImGui::Image((ImTextureID)(e_globRenderer.ssaoDesc[0].getHandle().ptr), ImVec2(160.0f, 90.0f), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImGui::GetStyleColorVec4(ImGuiCol_Border));
                //ImGui::Image((ImTextureID)(e_globRenderer.ssaoDesc[2].getHandle().ptr), ImVec2(160.0f, 90.0f), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImGui::GetStyleColorVec4(ImGuiCol_Border));

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Culling"))
            {
                e_globRenderer.guiCullingToggles();
                ImGui::Separator();

                const renderer::cullStats& stats = e_globRenderer.getCullStats();

                uint frustumCulledInstances = 0;
                if (stats.instancesTotal > stats.instancesPass1 + stats.instancesPass2)
                {
                    frustumCulledInstances = stats.instancesTotal - stats.instancesPass1 - stats.instancesPass2;
                }

                ImGui::Text("Instances: total=%u  frustumCulled=%u  pass1=%u  pass2=%u",
                    stats.instancesTotal, frustumCulledInstances, stats.instancesPass1, stats.instancesPass2);

                if (ImGui::BeginTable("cullStats", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                {
                    ImGui::TableSetupColumn("Metric");
                    ImGui::TableSetupColumn("Pass 1");
                    ImGui::TableSetupColumn("Pass 2");
                    ImGui::TableHeadersRow();

                    auto row = [](const char* name, uint p1, uint p2)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0); ImGui::Text("%s", name);
                        ImGui::TableSetColumnIndex(1); ImGui::Text("%u", p1);
                        ImGui::TableSetColumnIndex(2); ImGui::Text("%u", p2);
                    };

                    row("Cluster candidates", stats.pass1.clusterCandidates, stats.pass2.clusterCandidates);
                    row("Cluster frustum-culled", stats.pass1.clusterFrustumCulled, stats.pass2.clusterFrustumCulled);
                    row("Cluster HZB-occluded", stats.pass1.clusterOccluded, stats.pass2.clusterOccluded);
                    row("Cluster survivors", stats.pass1.clusterSurvivors, stats.pass2.clusterSurvivors);
                    row("Tri candidates", stats.pass1.triCandidates, stats.pass2.triCandidates);
                    row("Tri survivors", stats.pass1.triSurvivors, stats.pass2.triSurvivors);

                    ImGui::EndTable();
                }

                ImGui::Separator();

                float pass1Candidates = (float)(std::max)(1u, stats.pass1.clusterCandidates);
                float pass2Candidates = (float)(std::max)(1u, stats.pass2.clusterCandidates);

                ImGui::ProgressBar((float)stats.pass1.clusterOccluded / pass1Candidates, ImVec2(0, 0), "Pass1 occluded %");
                ImGui::ProgressBar((float)stats.pass2.clusterOccluded / pass2Candidates, ImVec2(0, 0), "Pass2 occluded %");
                ImGui::ProgressBar((float)stats.pass1.clusterSurvivors / pass1Candidates, ImVec2(0, 0), "Pass1 survivors %");
                ImGui::ProgressBar((float)stats.pass2.clusterSurvivors / pass2Candidates, ImVec2(0, 0), "Pass2 survivors %");

                ImGui::PlotLines("Cluster survivors", e_globRenderer.getClusterSurvivorHistory(), (int)renderer::CULLSTATS_HISTORY, (int)e_globRenderer.getCullStatsHistoryOffset(), nullptr, 0.0f, FLT_MAX, ImVec2(0, 60));
                ImGui::PlotLines("Triangle survivors", e_globRenderer.getTriSurvivorHistory(), (int)renderer::CULLSTATS_HISTORY, (int)e_globRenderer.getCullStatsHistoryOffset(), nullptr, 0.0f, FLT_MAX, ImVec2(0, 60));

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("HZB"))
            {
                static int hzbViewMip = 0;
                static float hzbViewGain = 1.0f;

                int mipCount = (int)e_globRenderer.getHZBMipCount();
                if (mipCount <= 0) mipCount = 1;
                hzbViewMip = (std::max)(0, (std::min)(hzbViewMip, mipCount - 1));

                ImGui::SliderInt("Mip", &hzbViewMip, 0, mipCount - 1);
                ImGui::DragFloat("Gain", &hzbViewGain, 0.05f, 0.01f, 50.0f);

                uint mipW = 0, mipH = 0;
                e_globRenderer.getHZBMipSize((uint)hzbViewMip, mipW, mipH);
                ImGui::Text("mip %d: %u x %u", hzbViewMip, mipW, mipH);

                ImGui::Image((ImTextureID)e_globRenderer.getHZBMipHandle((uint)hzbViewMip).ptr, ImVec2(384, 216), ImVec2(0, 0), ImVec2(1, 1), ImVec4(hzbViewGain, hzbViewGain, hzbViewGain, 1), ImGui::GetStyleColorVec4(ImGuiCol_Border));

                ImGui::BeginChild("HZBMipStrip", ImVec2(0, 70), ImGuiChildFlags_Border, ImGuiWindowFlags_HorizontalScrollbar);
                for (int m = 0; m < mipCount; ++m)
                {
                    ImGui::PushID(m);
                    if (ImGui::ImageButton("hzbMipThumb", (ImTextureID)e_globRenderer.getHZBMipHandle((uint)m).ptr, ImVec2(96, 54), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), ImVec4(hzbViewGain, hzbViewGain, hzbViewGain, 1)))
                    {
                        hzbViewMip = m;
                    }
                    ImGui::PopID();
                    ImGui::SameLine();
                }
                ImGui::EndChild();

                ImGui::EndTabItem();
            }

#if ENGINE_DEBUG_GPUPROF || ENGINE_DEBUG_CPUPROF
            if (ImGui::BeginTabItem("Profiler"))
            {
#if ENGINE_DEBUG_GPUPROF
                ImGui::SeparatorText("GPU");

                uint gpuLaneCount = prof::getGPULaneCount();
                for (uint laneIdx = 0; laneIdx < gpuLaneCount; ++laneIdx)
                {
                    const prof::profLaneView* laneView = prof::getGPULaneView(laneIdx);
                    if (!laneView)
                        continue;

                    std::string overlay = std::format("{} {:.3f} ms", laneView->label, laneView->totalMs);
                    ImGui::PlotLines(laneView->label, laneView->totalHistory, (int)prof::PROF_HISTORY_FRAMES,
                        (int)prof::getGPUHistoryOffset(), overlay.c_str(), 0.0f, FLT_MAX, ImVec2(0, 60));

                    if (ImGui::BeginTable(std::format("GPUEventsLane{}", laneIdx).c_str(), 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                    {
                        ImGui::TableSetupColumn("Event");
                        ImGui::TableSetupColumn("ms");
                        ImGui::TableHeadersRow();

                        for (uint eventIdx = 0; eventIdx < laneView->eventCount; ++eventIdx)
                        {
                            const prof::profEventView& event = laneView->events[eventIdx];

                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::PushID(eventIdx);

                            for (uint d = 0; d < event.depth; ++d)
                                ImGui::Spacing();
                            ImGui::SameLine();

                            bool isSelected = (selectedGPUEvent == event.nameID);
                            if (ImGui::Selectable(event.name, isSelected))
                            {
                                if (isSelected)
                                    selectedGPUEvent = prof::EVENT_INVALID;
                                else
                                    selectedGPUEvent = event.nameID;
                            }
                            ImGui::PopID();

                            ImGui::TableSetColumnIndex(1);
                            ImGui::Text("%.3f", event.timeMs);
                        }

                        ImGui::EndTable();
                    }

                    if (selectedGPUEvent != prof::EVENT_INVALID)
                    {
                        const float* eventHist = prof::getGPUEventHistory(selectedGPUEvent);
                        if (eventHist)
                        {
                            std::string eventName = std::format("Event: {} (GPU)", prof::getEventName(selectedGPUEvent));
                            ImGui::PlotLines(eventName.c_str(), eventHist, (int)prof::PROF_HISTORY_FRAMES,
                                (int)prof::getGPUHistoryOffset(), nullptr, 0.0f, FLT_MAX, ImVec2(0, 60));
                        }
                    }
                }
#endif // ENGINE_DEBUG_GPUPROF

#if ENGINE_DEBUG_CPUPROF
                ImGui::SeparatorText("CPU");

                ImGui::Text("CPU Total (all threads): %.3f ms", prof::getCPUFrameTotalMs());
                ImGui::PlotLines("CPU Total", prof::getCPUFrameTotalHistory(), (int)prof::PROF_HISTORY_FRAMES, (int)prof::getCPUHistoryOffset(), nullptr, 0.0f, FLT_MAX, ImVec2(0, 60));

                uint cpuLaneCount = prof::getCPULaneCount();
                for (uint laneIdx = 0; laneIdx < cpuLaneCount; ++laneIdx)
                {
                    const prof::profLaneView* laneView = prof::getCPULaneView(laneIdx);
                    if (!laneView)
                        continue;

                    std::string overlay = std::format("{} {:.3f} ms", laneView->label, laneView->totalMs);
                    ImGui::PlotLines(laneView->label, laneView->totalHistory, (int)prof::PROF_HISTORY_FRAMES,
                        (int)prof::getCPUHistoryOffset(), overlay.c_str(), 0.0f, FLT_MAX, ImVec2(0, 60));

                    if (ImGui::BeginTable(std::format("CPUEventsLane{}", laneIdx).c_str(), 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                    {
                        ImGui::TableSetupColumn("Event");
                        ImGui::TableSetupColumn("ms");
                        ImGui::TableHeadersRow();

                        for (uint eventIdx = 0; eventIdx < laneView->eventCount; ++eventIdx)
                        {
                            const prof::profEventView& event = laneView->events[eventIdx];

                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::PushID(eventIdx);

                            for (uint d = 0; d < event.depth; ++d)
                                ImGui::Spacing();
                            ImGui::SameLine();

                            bool isSelected = (selectedCPUEvent == event.nameID);
                            if (ImGui::Selectable(event.name, isSelected))
                            {
                                if (isSelected)
                                    selectedCPUEvent = prof::EVENT_INVALID;
                                else
                                    selectedCPUEvent = event.nameID;
                            }
                            ImGui::PopID();

                            ImGui::TableSetColumnIndex(1);
                            ImGui::Text("%.3f", event.timeMs);
                        }

                        ImGui::EndTable();
                    }

                    if (selectedCPUEvent != prof::EVENT_INVALID)
                    {
                        const float* eventHist = prof::getCPUEventHistory(selectedCPUEvent);
                        if (eventHist)
                        {
                            std::string eventName = std::format("Event: {} (CPU)", prof::getEventName(selectedCPUEvent));
                            ImGui::PlotLines(eventName.c_str(), eventHist, (int)prof::PROF_HISTORY_FRAMES,
                                (int)prof::getCPUHistoryOffset(), nullptr, 0.0f, FLT_MAX, ImVec2(0, 60));
                        }
                    }
                }
#endif // ENGINE_DEBUG_CPUPROF

                ImGui::EndTabItem();
            }
#endif // ENGINE_DEBUG_GPUPROF || ENGINE_DEBUG_CPUPROF

            ImGui::EndTabBar();
        }

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