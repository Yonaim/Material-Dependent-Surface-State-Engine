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
    class TRenderer;
    class TScene;
    class TVulkanContext;
    class TWindow;

    class TDebugUI
    {
    public:
        TDebugUI(const TVulkanContext& Context, const TWindow& TWindow, TRenderer& TRenderer);
        ~TDebugUI();

        TDebugUI(const TDebugUI&) = delete;
        TDebugUI& operator=(const TDebugUI&) = delete;
        TDebugUI(TDebugUI&&) = delete;
        TDebugUI& operator=(TDebugUI&&) = delete;

        /** @brief 새 ImGui frame을 시작해 진단 창을 갱신하고 draw data를 확정한다. */
        void BeginFrame(TScene& SceneData);

        /** @brief 현재 Vulkan render pass에 ImGui draw command를 기록한다. */
        void Render(VkCommandBuffer CommandBuffer) const;

        /** @brief swapchain 재생성 후 ImGui Vulkan backend의 image count를 갱신한다. */
        void OnSwapchainRecreated(const TVulkanContext& Context, const TRenderer& TRenderer);

    private:
        void ProcessCameraInput(TScene& SceneData);
        void DrawCameraWindow(TScene& SceneData);
        void DrawRenderOptionsWindow();
        void DrawLogWindow();

        VkDevice    Device = VK_NULL_HANDLE;
        GLFWwindow* NativeWindow = nullptr;
        TRenderer*   FrameRenderer = nullptr;
        bool        bRotatingCamera = false;

        std::array<bool, static_cast<std::size_t>(TLogLevel::Count)> LogLevelFilters{true, true, true, true, true};
        std::vector<TLogEntry>                                       CachedLogEntries;
        std::uint64_t                                               LastSeenLogRevision = 0;
        bool                                                        bScrollLogToBottom = true;
        float                                                       LogWindowHeight = 240.0F;
    };
} // namespace MDSS
