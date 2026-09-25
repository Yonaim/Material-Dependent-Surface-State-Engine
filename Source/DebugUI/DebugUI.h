/**
 * @file DebugUI.h
 * @brief ImGui 기반 카메라·렌더 설정과 로그 진단 UI.
 */

#pragma once

#include "Logger/Logger.h"

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

struct GLFWwindow;

namespace MDSS
{
    class Renderer;
    class Scene;
    class VulkanContext;
    class Window;

    class DebugUI
    {
    public:
        DebugUI(const VulkanContext& Context, const Window& Window, Renderer& Renderer);
        ~DebugUI();

        DebugUI(const DebugUI&) = delete;
        DebugUI& operator=(const DebugUI&) = delete;
        DebugUI(DebugUI&&) = delete;
        DebugUI& operator=(DebugUI&&) = delete;

        /** @brief 새 ImGui frame을 시작해 진단 창을 갱신하고 draw data를 확정한다. */
        void BeginFrame(Scene& SceneData);

        /** @brief 현재 Vulkan render pass에 ImGui draw command를 기록한다. */
        void Render(VkCommandBuffer CommandBuffer) const;

        /** @brief swapchain 재생성 후 ImGui Vulkan backend의 image count를 갱신한다. */
        void OnSwapchainRecreated(const VulkanContext& Context, const Renderer& Renderer);

    private:
        void ProcessCameraInput(Scene& SceneData);
        void DrawCameraWindow(Scene& SceneData);
        void DrawRenderOptionsWindow();
        void DrawLogWindow();

        VkDevice    Device = VK_NULL_HANDLE;
        GLFWwindow* NativeWindow = nullptr;
        Renderer*   FrameRenderer = nullptr;
        bool        bRotatingCamera = false;

        std::array<bool, static_cast<std::size_t>(LogLevel::Count)> LogLevelFilters{true, true, true, true, true};
        std::vector<LogEntry>                                       CachedLogEntries;
        std::uint64_t                                               LastSeenLogRevision = 0;
        bool                                                        bScrollLogToBottom = true;
        float                                                       LogWindowHeight = 240.0F;
    };
} // namespace MDSS
