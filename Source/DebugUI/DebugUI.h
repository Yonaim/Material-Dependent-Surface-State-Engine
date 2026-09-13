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

        // Starts a new ImGui frame, builds all debug windows and finalizes draw data.
        void BeginFrame(Scene& SceneData);

        // Records ImGui rendering commands into the active Vulkan render pass.
        void Render(VkCommandBuffer CommandBuffer) const;

        // Updates Vulkan backend swapchain-dependent image-count state after a resize.
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
