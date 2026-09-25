/**
 * @file Application.cpp
 * @brief 응용 프로그램 초기화, 하위 시스템 구성과 메인 루프.
 */

#include "Application/Application.h"

#include "DebugUI/DebugUI.h"
#include "Logger/Logger.h"
#include "Renderer/Renderer.h"
#include "Scene/StaticMeshInstance.h"

#include <filesystem>

#ifndef MDSS_ASSET_DIR
#define MDSS_ASSET_DIR "Assets"
#endif

namespace MDSS
{
    TApplication::TApplication() : MainWindow(1280, 720, "MDSS Engine"), Context(MainWindow), Assets(Context), MainScene()
    {
        TLogger::Info("TApplication", "Initializing MDSS Engine.");

        const std::filesystem::path DemoMeshPath = std::filesystem::path(MDSS_ASSET_DIR) / "Meshes" / "DemoCube.obj";
        const TMeshAssetHandle       DemoMesh = Assets.LoadOBJ(DemoMeshPath);

        TTransform InstanceTransform{};
        InstanceTransform.RotationDegrees = {20.0F, 35.0F, 0.0F};
        MainScene.AddStaticMeshInstance(TStaticMeshInstance(DemoMesh, InstanceTransform));

        FrameRenderer = std::make_unique<TRenderer>(Context, MainWindow, Assets);
        DebugInterface = std::make_unique<TDebugUI>(Context, MainWindow, *FrameRenderer);
        TLogger::Info("TApplication", "TRenderer, scene, asset system, and TDebugUI are ready.");
    }

    TApplication::~TApplication() = default;

    void TApplication::Run()
    {
        TLogger::Info("TApplication", "Entering main loop.");
        MainLoop();
        TLogger::Info("TApplication", "Main loop finished.");
    }

    void TApplication::MainLoop()
    {
        while (!MainWindow.ShouldClose())
        {
            MainWindow.PollEvents();
            DebugInterface->BeginFrame(MainScene);
            FrameRenderer->RenderFrame(MainScene, *DebugInterface);
        }

        TLogger::Debug("TApplication", "Waiting for the Vulkan device to become idle before shutdown.");
        vkDeviceWaitIdle(Context.GetDevice());
    }
} // namespace MDSS
