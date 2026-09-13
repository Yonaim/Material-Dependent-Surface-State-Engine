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
        [[nodiscard]] bool        WasFramebufferResized() const noexcept;
        void                      ResetFramebufferResized() noexcept;
        void                      GetFramebufferSize(std::uint32_t& Width, std::uint32_t& Height) const noexcept;
        void                      WaitForNonZeroFramebuffer() const;

    private:
        static void InitializeGLFW();
        static void TerminateGLFW();
        static void FramebufferSizeCallback(GLFWwindow* WindowHandle, int Width, int Height);

        GLFWwindow* Handle = nullptr;
        bool        bFramebufferResized = false;
    };
} // namespace MDSS
