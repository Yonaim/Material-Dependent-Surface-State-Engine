#pragma once

#include "Application/Window.h"
#include "AssetManager/AssetManager.h"
#include "Scene/Scene.h"
#include "VulkanContext/VulkanContext.h"

#include <memory>

namespace MDSS
{
    class DebugUI;
    class Renderer;

    class Application
    {
    public:
        Application();
        ~Application();

        void Run();

    private:
        void MainLoop();

        // Declaration order is intentional: resources are destroyed in reverse order.
        Window                    MainWindow;
        VulkanContext             Context;
        AssetManager              Assets;
        Scene                     MainScene;
        std::unique_ptr<Renderer> FrameRenderer;
        std::unique_ptr<DebugUI>  DebugInterface;
    };
} // namespace MDSS
