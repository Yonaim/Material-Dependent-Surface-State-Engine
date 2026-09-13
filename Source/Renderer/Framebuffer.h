#pragma once

#include <vulkan/vulkan.h>

#include <cstddef>
#include <vector>

namespace MDSS
{
    class Framebuffer
    {
    public:
        Framebuffer(VkDevice                        Device,
                    VkRenderPass                    RenderPass,
                    VkExtent2D                      Extent,
                    const std::vector<VkImageView>& ColorImageViews);
        ~Framebuffer();

        Framebuffer(const Framebuffer&) = delete;
        Framebuffer& operator=(const Framebuffer&) = delete;
        Framebuffer(Framebuffer&&) = delete;
        Framebuffer& operator=(Framebuffer&&) = delete;

        [[nodiscard]] VkFramebuffer Get(std::size_t Index) const;
        [[nodiscard]] std::size_t   GetCount() const noexcept;

    private:
        VkDevice                   Device = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> Handles;
    };
} // namespace MDSS
