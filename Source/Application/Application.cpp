/**
 * @file Application.cpp
 * @brief 응용 프로그램 초기화, 하위 시스템 구성과 메인 루프.
 */

#include "Application/Application.h"

#include "DebugUI/DebugUI.h"
#include "AssetManager/Loaders/SceneLoader.h"
#include "InputSystem/InputSystem.h"
#include "Logger/Logger.h"
#include "Renderer/Renderer.h"

#include <chrono>
#include <filesystem>

#ifndef MDSS_ASSET_DIR
#define MDSS_ASSET_DIR "Assets"
#endif

namespace MDSS
{
    TApplication::TApplication() : MainWindow(1280, 720, "MDSS Engine"), Context(MainWindow), Assets(Context), MainScene()
    {
        TLogger::Info("TApplication", "Initializing MDSS Engine.");

        const std::filesystem::path DemoScenePath = std::filesystem::path(MDSS_ASSET_DIR) / "Scenes" / "Demo.Scene";
        MainScene = TSceneLoader::Load(DemoScenePath, Assets);

        FrameRenderer = std::make_unique<TRenderer>(Context, MainWindow, Assets, MainScene);
        DebugInterface = std::make_unique<TDebugUI>(Context, MainWindow, *FrameRenderer, Assets);
        InputInterface = std::make_unique<TInputSystem>(MainWindow.GetNativeHandle());
        TLogger::Info("TApplication", "TRenderer, scene, asset system, and TDebugUI are ready.");
    }

    TApplication::~TApplication() = default;

    void TApplication::Run(std::size_t FrameLimit)
    {
        if (FrameLimit == 0)
        {
            TLogger::Info("TApplication", "Entering main loop.");
        }
        else
        {
            TLogger::Info("TApplication", "Entering main loop for " + std::to_string(FrameLimit) + " frame(s).");
        }
        MainLoop(FrameLimit);
        TLogger::Info("TApplication", "Main loop finished.");
    }

    void TApplication::MainLoop(std::size_t FrameLimit)
    {
        std::size_t RenderedFrameCount = 0;
        auto        PreviousFrameTime = std::chrono::steady_clock::now();
        while (!MainWindow.ShouldClose() && (FrameLimit == 0 || RenderedFrameCount < FrameLimit))
        {
            MainWindow.PollEvents();
            DebugInterface->BeginFrame(MainScene);
            if (const std::optional<TSurfaceContactInput> Contact = InputInterface->PollDebugContact(
                    MainScene,
                    Assets,
                    MainScene.GetMainCamera(),
                    DebugInterface->IsInjectModeEnabled(),
                    DebugInterface->GetInjectState(),
                    DebugInterface->GetInjectStrength(),
                    DebugInterface->IsKeyboardCaptured()))
            {
                FrameRenderer->SubmitContact(*Contact);
            }
            const auto  CurrentFrameTime = std::chrono::steady_clock::now();
            const float DeltaTime = std::chrono::duration<float>(CurrentFrameTime - PreviousFrameTime).count();
            PreviousFrameTime = CurrentFrameTime;
            FrameRenderer->RenderFrame(MainScene, *DebugInterface, DeltaTime);
            ++RenderedFrameCount;
        }

        TLogger::Debug("TApplication", "Waiting for the Vulkan device to become idle before shutdown.");
        vkDeviceWaitIdle(Context.GetDevice());
    }
} // namespace MDSS
