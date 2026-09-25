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
    Application::Application() : MainWindow(1280, 720, "MDSS Engine"), Context(MainWindow), Assets(Context), MainScene()
    {
        Logger::Info("Application", "Initializing MDSS Engine.");

        const std::filesystem::path DemoMeshPath = std::filesystem::path(MDSS_ASSET_DIR) / "Meshes" / "DemoCube.obj";
        const MeshAssetHandle       DemoMesh = Assets.LoadOBJ(DemoMeshPath);

        Transform InstanceTransform{};
        InstanceTransform.RotationDegrees = {20.0F, 35.0F, 0.0F};
        MainScene.AddStaticMeshInstance(StaticMeshInstance(DemoMesh, InstanceTransform));

        FrameRenderer = std::make_unique<Renderer>(Context, MainWindow, Assets);
        DebugInterface = std::make_unique<DebugUI>(Context, MainWindow, *FrameRenderer);
        Logger::Info("Application", "Renderer, scene, asset system, and DebugUI are ready.");
    }

    Application::~Application() = default;

    void Application::Run()
    {
        Logger::Info("Application", "Entering main loop.");
        MainLoop();
        Logger::Info("Application", "Main loop finished.");
    }

    void Application::MainLoop()
    {
        while (!MainWindow.ShouldClose())
        {
            MainWindow.PollEvents();
            DebugInterface->BeginFrame(MainScene);
            FrameRenderer->RenderFrame(MainScene, *DebugInterface);
        }

        Logger::Debug("Application", "Waiting for the Vulkan device to become idle before shutdown.");
        vkDeviceWaitIdle(Context.GetDevice());
    }
} // namespace MDSS
