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
        // Render state texture lifecycle
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
        // Descriptor access and texture command recording
        [[nodiscard]] VkDescriptorSet GetSet(std::size_t Instance, bool bSmoothed = false) const noexcept;
        void                          Record(VkCommandBuffer                                 Command,
                                             const SurfaceState::TSurfaceGPUResourceManager& Resources,
                                             std::array<std::uint32_t, 4>                    Channels,
                                             std::uint32_t                                   ChannelCount,
                                             bool                                            bUpdateStates,
                                             bool                                            bPrecomputeSmoothing,
                                             bool                                            bSeparableSmoothing,
                                             bool                                            bSharedHalo);

    private:
        struct TInstance
        {
            std::unique_ptr<GPU::TGPUImage>     Image;
            std::unique_ptr<GPU::TGPUImageView> View;
            std::unique_ptr<GPU::TGPUImage>     SmoothingTempImage;
            std::unique_ptr<GPU::TGPUImageView> SmoothingTempView;
            std::unique_ptr<GPU::TGPUImage>     SmoothedImage;
            std::unique_ptr<GPU::TGPUImageView> SmoothedView;
            VkDescriptorSet                     Set = VK_NULL_HANDLE;
            VkDescriptorSet                     SmoothedSet = VK_NULL_HANDLE;
            VkDescriptorSet                     SmoothingSet = VK_NULL_HANDLE;
            VkExtent2D                          Extent{};
            std::uint32_t                       SurfaceCount = 0;
            bool                                bInitialized = false;
            bool                                bSmoothingInitialized = false;
        };

        void Destroy() noexcept;

        VkDevice               Device = VK_NULL_HANDLE;
        VkDescriptorSetLayout  Layout = VK_NULL_HANDLE;
        VkDescriptorSetLayout  SmoothingLayout = VK_NULL_HANDLE;
        VkDescriptorPool       Pool = VK_NULL_HANDLE;
        VkPipelineLayout       PipelineLayout = VK_NULL_HANDLE;
        VkPipelineLayout       SmoothingPipelineLayout = VK_NULL_HANDLE;
        VkPipeline             Pipeline = VK_NULL_HANDLE;
        std::array<VkPipeline, 5> SmoothingPipelines{};
        VkSampler              Sampler = VK_NULL_HANDLE;
        std::vector<TInstance> Instances;
    };
}
