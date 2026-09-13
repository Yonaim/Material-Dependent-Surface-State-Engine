#include "Application/Application.h"

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
        const std::filesystem::path DemoMeshPath = std::filesystem::path(MDSS_ASSET_DIR) / "Meshes" / "DemoCube.obj";
        const MeshAssetHandle       DemoMesh = Assets.LoadOBJ(DemoMeshPath);

        Transform InstanceTransform{};
        InstanceTransform.RotationDegrees = {20.0F, 35.0F, 0.0F};
        MainScene.AddStaticMeshInstance(StaticMeshInstance(DemoMesh, InstanceTransform));

        FrameRenderer = std::make_unique<Renderer>(Context, MainWindow, Assets);
    }

    Application::~Application() = default;

    void Application::Run()
    {
        MainLoop();
    }

    void Application::MainLoop()
    {
        while (!MainWindow.ShouldClose())
        {
            MainWindow.PollEvents();
            FrameRenderer->RenderFrame(MainScene);
        }

        vkDeviceWaitIdle(Context.GetDevice());
    }
} // namespace MDSS
