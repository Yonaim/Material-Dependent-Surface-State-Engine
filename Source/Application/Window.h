#pragma once

#include <cstdint>
#include <string>

struct GLFWwindow;

namespace MDSS
{
    class Window
    {
    public:
        Window(std::uint32_t Width, std::uint32_t Height, std::string Title);
        ~Window();

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;
        Window(Window&&) = delete;
        Window& operator=(Window&&) = delete;

        [[nodiscard]] bool ShouldClose() const;
        void               PollEvents() const;

        [[nodiscard]] GLFWwindow* GetNativeHandle() const noexcept;

    private:
        static void InitializeGLFW();
        static void TerminateGLFW();

        GLFWwindow* Handle = nullptr;
    };
} // namespace MDSS
