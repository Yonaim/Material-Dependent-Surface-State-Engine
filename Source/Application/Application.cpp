#include "Application/Application.h"

namespace MDSS
{
    Application::Application()
        : MainWindow(1280, 720, "MDSSP Engine"), Context(MainWindow), FrameRenderer(Context, MainWindow)
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
        }

        // Renderer work will be submitted asynchronously in the next milestone.
        // Keeping this here now also guarantees a clean shutdown once that begins.
        vkDeviceWaitIdle(Context.GetDevice());
    }
} // namespace MDSS
