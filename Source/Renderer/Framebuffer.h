/**
 * @file Framebuffer.h
 * @brief swapchain image와 depth image를 연결하는 framebuffer 자원.
 */

#pragma once

#include <vulkan/vulkan.h>

#include <cstddef>
#include <vector>

namespace MDSS
{
    class TFramebuffer
    {
    public:
        /** @brief 각 color image view와 공통 depth view를 연결한 framebuffer를 생성한다. */
        TFramebuffer(VkDevice                        Device,
                    VkRenderPass                    TRenderPass,
                    VkExtent2D                      Extent,
                    const std::vector<VkImageView>& ColorImageViews,
                    VkImageView                     DepthImageView);
        ~TFramebuffer();

        TFramebuffer(const TFramebuffer&) = delete;
        TFramebuffer& operator=(const TFramebuffer&) = delete;
        TFramebuffer(TFramebuffer&&) = delete;
        TFramebuffer& operator=(TFramebuffer&&) = delete;

        /** @brief 이전 framebuffer를 제거하고 새 extent와 image view로 다시 생성한다. */
        void Recreate(VkRenderPass                    TRenderPass,
                      VkExtent2D                      Extent,
                      const std::vector<VkImageView>& ColorImageViews,
                      VkImageView                     DepthImageView);
        /** @brief 보유 중인 framebuffer handle을 해제한다. */
        void Reset();

        /** @throws std::out_of_range Index가 framebuffer 개수 밖인 경우. */
        [[nodiscard]] VkFramebuffer Get(std::size_t Index) const;
        [[nodiscard]] std::size_t   GetCount() const noexcept;

    private:
        void Create(VkRenderPass                    TRenderPass,
                    VkExtent2D                      Extent,
                    const std::vector<VkImageView>& ColorImageViews,
                    VkImageView                     DepthImageView);

        VkDevice                   Device = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> Handles;
    };
} // namespace MDSS
