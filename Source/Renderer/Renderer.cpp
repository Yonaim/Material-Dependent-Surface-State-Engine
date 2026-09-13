#include "Renderer/Renderer.h"

#include "Application/Window.h"
#include "VulkanContext/VulkanContext.h"

namespace MDSS
{
    Renderer::Renderer(const VulkanContext& Context, const Window& Window) : SwapchainData(Context, Window)
    {
    }

    const Swapchain& Renderer::GetSwapchain() const noexcept
    {
        return SwapchainData;
    }
} // namespace MDSS
