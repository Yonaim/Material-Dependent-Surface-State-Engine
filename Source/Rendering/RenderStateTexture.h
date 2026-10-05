/**
 * @file RenderStateTexture.h
 * @brief 시뮬레이션 SSBO의 demo State를 Surface별 RGBA texture layer로 변환한다.
 */
#pragma once

#include "GPU/Vulkan/Resource/GPUImage.h"
#include "GPU/Vulkan/Resource/GPUImageView.h"
#include "SurfaceState/GPU/SurfaceGPUResources.h"

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace MDSS::Rendering
{
    class TRenderStateTexture final
    {
    public:
        TRenderStateTexture(VkPhysicalDevice                                PhysicalDevice,
                            VkDevice                                        Device,
                            VkDescriptorSetLayout                           SurfaceLayout,
                            const SurfaceState::TSurfaceGPUResourceManager& Resources);
        ~TRenderStateTexture();

        TRenderStateTexture(const TRenderStateTexture&) = delete;
        TRenderStateTexture& operator=(const TRenderStateTexture&) = delete;

        [[nodiscard]] VkDescriptorSetLayout GetLayout() const noexcept
        {
            return Layout;
        }
        [[nodiscard]] VkDescriptorSet GetSet(std::size_t Instance) const noexcept;
        void                          Record(VkCommandBuffer                                 Command,
                                             const SurfaceState::TSurfaceGPUResourceManager& Resources,
                                             std::array<std::uint32_t, 4>                    Channels,
                                             std::uint32_t                                   ChannelCount);

    private:
        struct TInstance
        {
            std::unique_ptr<GPU::TGPUImage>     Image;
            std::unique_ptr<GPU::TGPUImageView> View;
            VkDescriptorSet                     Set = VK_NULL_HANDLE;
            VkExtent2D                          Extent{};
            std::uint32_t                       SurfaceCount = 0;
            bool                                bInitialized = false;
        };

        void Destroy() noexcept;

        VkDevice               Device = VK_NULL_HANDLE;
        VkDescriptorSetLayout  Layout = VK_NULL_HANDLE;
        VkDescriptorPool       Pool = VK_NULL_HANDLE;
        VkPipelineLayout       PipelineLayout = VK_NULL_HANDLE;
        VkPipeline             Pipeline = VK_NULL_HANDLE;
        VkSampler              Sampler = VK_NULL_HANDLE;
        std::vector<TInstance> Instances;
    };
}
