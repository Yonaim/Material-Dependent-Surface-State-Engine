#pragma once

#include "Renderer/Swapchain.h"

namespace MDSS
{
    class VulkanContext;
    class Window;

    class Renderer
    {
    public:
        Renderer(const VulkanContext& Context, const Window& Window);

        [[nodiscard]] const Swapchain& GetSwapchain() const noexcept;

    private:
        Swapchain SwapchainData;
    };
} // namespace MDSS
