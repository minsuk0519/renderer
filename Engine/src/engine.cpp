#include "engine.hpp"

#include "system\config.hpp"
#include "system\logger.hpp"

#include <filesystem>
#include <shlobj.h>

#include <wrl.h>
#include <d3d12.h>

#if CONFIG_PIX_ENABLED
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
        // TODO: Error, no PIX installation found
    }

    return pixInstallationPath / newestVersionFound / L"WinPixGpuCapturer.dll";
}
#endif

bool engine::init(HINSTANCE hInstance, int nCmdShow)
{
    TC_LOG("engine initializing start...");

#ifdef _DEBUG
#if CONFIG_PIX_ENABLED
	if ((GetModuleHandle(L"WinPixGpuCapturer.dll") == 0))
	{
		LoadLibrary(GetLatestWinPixGpuCapturerPath().c_str());
	}
#endif // #if CONFIG_PIX_ENABLED

    Microsoft::WRL::ComPtr<ID3D12Debug> debugInterface;
    D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface));
    debugInterface->EnableDebugLayer();
#endif // #ifdef _DEBUG

	TC_LOG("engine initializing success!");

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
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        //when user press the escape key break the loop
        //if (input::isPressed(input::KEY_ESC)) break;

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
    }
}

void engine::close()
{
    TC_LOG("shutting down engine!");
}