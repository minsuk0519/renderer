#include "engine.hpp"

#include "system\config.hpp"
#include "system\logger.hpp"
#include "system\window.hpp"
#include "system\input.hpp"
#include "render\renderer.hpp"
#include "system\jsonhelper.hpp"
#include "world\world.hpp"

#include <filesystem>
#include <shlobj.h>
#include <string>

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

configJson configFile{};

engine e_globEngine;

#if CONFIG_CAPTURE_ENABLED
static std::wstring GetLatestWinPixGpuCapturerPath()
{
    LPWSTR programFilesPath = nullptr;
    SHGetKnownFolderPath(FOLDERID_ProgramFiles, KF_FLAG_DEFAULT, NULL, &programFilesPath);

    std::filesystem::path pixInstallationPath = programFilesPath;
    pixInstallationPath /= "Microsoft PIX";

    std::wstring newestVersionFound;

    for (auto const& directory_entry : std::filesystem::directory_iterator(pixInstallationPath))
    {
        if (directory_entry.is_directory())
        {
            if (newestVersionFound.empty() || newestVersionFound < directory_entry.path().filename().c_str())
            {
                newestVersionFound = directory_entry.path().filename().c_str();
            }
        }
    }

    if (newestVersionFound.empty())
    {
        TC_LOG_ERROR("Cannot find PIX installation");
    }

    return pixInstallationPath / newestVersionFound / L"WinPixGpuCapturer.dll";
}
#endif // #if CONFIG_CAPTURE_ENABLED

bool engine::init(HINSTANCE hInstance, int nCmdShow, LPSTR cmdArgs)
{
    std::string cmdArgsStr = cmdArgs;

    uint captureFlag = 2;

    if (auto find = cmdArgsStr.find("+capture"); find != std::string::npos)
    {
        captureFlag = std::stoi(cmdArgsStr.substr(find + 9, 1));
    }

#ifdef _DEBUG
#if CONFIG_CAPTURE_ENABLED
    if (captureFlag == 0)
    {
        if ((GetModuleHandle(L"WinPixGpuCapturer.dll") == 0))
        {
            LoadLibrary(GetLatestWinPixGpuCapturerPath().c_str());
        }
    }
#endif // #if CONFIG_CAPTURE_ENABLED

    Microsoft::WRL::ComPtr<ID3D12Debug> debugInterface;
    D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface));
    debugInterface->EnableDebugLayer();
#endif // #ifdef _DEBUG

    //create factory
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    UINT createFactoryFlags = 0;
#if defined(_DEBUG)
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif
    TC_CONDITIONB(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&factory)) == S_OK, "Failed to create factory");

    Microsoft::WRL::ComPtr<IDXGIAdapter1> hardwareAdapter;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;

    Microsoft::WRL::ComPtr<IDXGIFactory6> factory6;
    if (SUCCEEDED(factory->QueryInterface(IID_PPV_ARGS(&factory6))))
    {
        for (
            UINT adapterIndex = 0;
            DXGI_ERROR_NOT_FOUND != factory6->EnumAdapterByGpuPreference(
                adapterIndex,
                DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                IID_PPV_ARGS(&adapter));
            ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                continue;
            }

            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_1, _uuidof(ID3D12Device), nullptr)))
            {
                break;
            }
        }
    }

    readJsonBuffer(configFile, JSON_FILE_NAME::CONFIG_FILE);

    TC_INIT(e_globWindow.init(hInstance, nCmdShow, configFile.width, configFile.height));

    TC_INIT(e_globRenderer.init(factory, adapter));

    e_globRenderer.setUp();

    TC_INIT(e_globWorld.init());

	return true;
}

void engine::run()
{
    uint64_t frameCounter = 0;
    double elapsedSeconds = 0.0;
    std::chrono::high_resolution_clock clock;
    auto t0 = clock.now();

    double fps = 0;

    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        e_globWindow.run();

        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        //when user press the escape key break the loop
        if (input::isPressed(input::KEY_ESC)) break;

        ++frameCounter;
        auto t1 = clock.now();
        auto deltaTime = t1 - t0;
        t0 = t1;

        elapsedSeconds += deltaTime.count() * 1e-9;
        if (elapsedSeconds > 1.0)
        {
            fps = frameCounter / elapsedSeconds;

            frameCounter = 0;
            elapsedSeconds = 0.0;
        }

        float dt = static_cast<float>(deltaTime.count() * 1e-5);

        e_globWorld.update(dt);

        e_globRenderer.preDraw(dt);

        e_globRenderer.draw(dt);
    }
}

void engine::close()
{
    writeJsonBuffer(configFile, JSON_FILE_NAME::CONFIG_FILE);

    TC_LOG("shutting down engine!");
}