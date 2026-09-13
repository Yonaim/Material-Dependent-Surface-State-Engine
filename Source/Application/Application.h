#pragma once

#include "Application/Window.h"

namespace mdssp
{
class Application
{
public:
    Application();

    void run();

private:
    void mainLoop();

    Window window_;
};
} // namespace mdssp
