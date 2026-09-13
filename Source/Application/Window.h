#pragma once

#include <cstdint>
#include <string>

struct GLFWwindow;

namespace mdssp
{
class Window
{
public:
    Window(std::uint32_t width, std::uint32_t height, std::string title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    [[nodiscard]] bool shouldClose() const;
    void pollEvents() const;

    [[nodiscard]] GLFWwindow* nativeHandle() const noexcept;

private:
    static void initializeGLFW();
    static void terminateGLFW();

    GLFWwindow* handle_ = nullptr;
};
} // namespace mdssp
