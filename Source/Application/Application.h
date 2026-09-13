#pragma once

#include "Application/Window.h"
#include "Renderer/Renderer.h"
#include "VulkanContext/VulkanContext.h"

namespace MDSS
{
    class Application
    {
    public:
        Application();

        void Run();

    private:
        void MainLoop();

        // Declaration order is intentional: resources are destroyed in reverse order.
        Window        MainWindow;
        VulkanContext Context;
        Renderer      FrameRenderer;
    };
} // namespace MDSS
