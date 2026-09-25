/**
 * @file Window.h
 * @brief GLFW 창의 수명 주기와 framebuffer 변경 이벤트.
 */

#pragma once

#include <cstdint>
#include <string>

struct GLFWwindow;

namespace MDSS
{
    class Window
    {
    public:
        /**
         * @brief GLFW를 초기화하고 Vulkan용 native window를 생성한다.
         * @throws std::runtime_error GLFW 또는 창 생성에 실패한 경우.
         */
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
        /** @brief 여러 Window 인스턴스 사이에서 GLFW를 첫 사용 시 한 번 초기화한다. */
        static void TerminateGLFW();
        /** @brief 마지막 Window가 사라질 때 GLFW global state를 종료한다. */
        static void FramebufferSizeCallback(GLFWwindow* WindowHandle, int Width, int Height);
        /** @brief framebuffer 크기 변경을 instance flag에 반영하는 GLFW callback. */

        GLFWwindow* Handle = nullptr;
        bool        bFramebufferResized = false;
    };
} // namespace MDSS
