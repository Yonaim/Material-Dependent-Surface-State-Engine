#include "Application/Application.h"

namespace MDSS
{
    Application::Application()
        : MainWindow(1280, 720, "MDSSP Engine"), Context(MainWindow), MainScene(), FrameRenderer(Context, MainWindow)
    {
    }

    void Application::Run()
    {
        MainLoop();
    }

    void Application::MainLoop()
    {
        while (!MainWindow.ShouldClose())
        {
            MainWindow.PollEvents();
            FrameRenderer.RenderFrame(MainScene);
        }

        vkDeviceWaitIdle(Context.GetDevice());
    }
} // namespace MDSS
