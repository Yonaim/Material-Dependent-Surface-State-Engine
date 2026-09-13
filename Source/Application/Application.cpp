#include "Application/Application.h"

namespace mdssp
{
Application::Application()
    : window_(1280, 720, "MDSSP Engine")
{
}

void Application::run()
{
    mainLoop();
}

void Application::mainLoop()
{
    while (!window_.shouldClose())
    {
        window_.pollEvents();
    }
}
} // namespace mdssp
