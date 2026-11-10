/**
 * @file DescriptorResources.cpp
 * @brief Surface의 AB/BA descriptor 수명과 instance buffer 재연결을 관리한다.
 */

#include "SurfaceState/GPU/Resources/DescriptorResources.h"

#include "SurfaceState/GPU/Resources/ResourceBufferHelpers.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/packing.hpp>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace MDSS::SurfaceState
{
    using namespace Detail;
// ============================================================
// 00. Descriptor Resources
// ============================================================
#pragma region 00_Descriptor_Resources
// 기존 AB/BA layout·set을 유지하며 호환되는 instance 버퍼를 연결한다.

    FSurfaceStateDescriptorResources::FSurfaceStateDescriptorResources(
        VkDevice                                  Device,
        const FSurfaceSharedGeometryGPUResources& SharedGeometry,
        const FSurfaceProfileGPUResources&        Profiles,
        const FSurfaceInstanceGPUResources&       Instance)
        : Device(Device), SharedGeometryResources(&SharedGeometry), ProfileResources(&Profiles)
    {
        if (SharedGeometry.GetTexelCount() != Instance.GetTexelCount() ||
            Profiles.GetChannelCount() != Instance.GetChannelCount() ||
            Profiles.GetMaxReactionCount() > Instance.GetReactionCapacity())
        {
            throw std::invalid_argument("Surface descriptor resources have incompatible texel or channel counts.");
        }

        std::array<VkDescriptorSetLayoutBinding, DescriptorBindingCount> Bindings{};

        for (std::uint32_t Binding = 0; Binding < DescriptorBindingCount; ++Binding)
        {
            Bindings[Binding].binding = Binding;
            Bindings[Binding].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            Bindings[Binding].descriptorCount = 1;
            Bindings[Binding].stageFlags =
                Binding == static_cast<std::uint32_t>(ESurfaceGPUDescriptorBinding::TransferWeightDebugAverages)
                    ? VK_SHADER_STAGE_FRAGMENT_BIT
                    : VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
        }

        VkDescriptorSetLayoutCreateInfo LayoutInfo{};
        LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        LayoutInfo.bindingCount = static_cast<std::uint32_t>(Bindings.size());
        LayoutInfo.pBindings = Bindings.data();

        if (vkCreateDescriptorSetLayout(Device, &LayoutInfo, nullptr, &Layout) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Surface solver descriptor set layout.");
        }

        VkDescriptorPoolSize PoolSize{};
        PoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        PoolSize.descriptorCount = DescriptorBindingCount * static_cast<std::uint32_t>(Sets.size());

        VkDescriptorPoolCreateInfo PoolInfo{};
        PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        PoolInfo.maxSets = static_cast<std::uint32_t>(Sets.size());
        PoolInfo.poolSizeCount = 1;
        PoolInfo.pPoolSizes = &PoolSize;

        if (vkCreateDescriptorPool(Device, &PoolInfo, nullptr, &Pool) != VK_SUCCESS)
        {
            vkDestroyDescriptorSetLayout(Device, Layout, nullptr);
            Layout = VK_NULL_HANDLE;
            throw std::runtime_error("Failed to create Surface solver descriptor pool.");
        }

        const std::array<VkDescriptorSetLayout, 2> Layouts{Layout, Layout};
        VkDescriptorSetAllocateInfo                AllocateInfo{};
        AllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        AllocateInfo.descriptorPool = Pool;
        AllocateInfo.descriptorSetCount = static_cast<std::uint32_t>(Layouts.size());
        AllocateInfo.pSetLayouts = Layouts.data();

        if (vkAllocateDescriptorSets(Device, &AllocateInfo, Sets.data()) != VK_SUCCESS)
        {
            vkDestroyDescriptorPool(Device, Pool, nullptr);
            Pool = VK_NULL_HANDLE;
            vkDestroyDescriptorSetLayout(Device, Layout, nullptr);
            Layout = VK_NULL_HANDLE;
            throw std::runtime_error("Failed to allocate Surface solver descriptor sets.");
        }

        RebindInstanceBuffers(Instance);
    }

    void FSurfaceStateDescriptorResources::RebindInstanceBuffers(const FSurfaceInstanceGPUResources& Instance)
    {
        const auto& SharedGeometry = *SharedGeometryResources;
        const auto& Profiles = *ProfileResources;
        if (SharedGeometry.GetTexelCount() != Instance.GetTexelCount() ||
            Profiles.GetChannelCount() != Instance.GetChannelCount() ||
            Profiles.GetMaxReactionCount() > Instance.GetReactionCapacity())
        {
            throw std::invalid_argument("Surface descriptor resources have incompatible texel or channel counts.");
        }

        const std::array<const GPU::FGPUBuffer*, DescriptorBindingCount> SharedAndProfileBuffers{
            &SharedGeometry.GetTexelSurfaceIndexBuffer(),
            &SharedGeometry.GetTexelProfileIndexBuffer(),
            &SharedGeometry.GetPositionBuffer(),
            &SharedGeometry.GetNormalBuffer(),
            &SharedGeometry.GetGeometryScalarBuffer(),
            &SharedGeometry.GetNeighborIndexBuffer(),
            &Profiles.GetParametersBuffer(),
            &Profiles.GetSupportedBuffer(),
            nullptr,
            nullptr,
            &Instance.GetOutgoingFluxScaleBuffer(),
            &Instance.GetInputDeltaBuffer(),
            &SharedGeometry.GetSurfaceRangeBuffer(),
            &SharedGeometry.GetTexelChartIndexBuffer(),
            &Instance.GetTransferWeightBuffer(),
            &Instance.GetRawOutgoingBuffer(),
            &Instance.GetTransferWeightDebugAverageBuffer(),
            // 바인딩 17에는 공유 Meso 노멀 버퍼를 연결한다.
            &SharedGeometry.GetMesoNormalBuffer(),
            &SharedGeometry.GetReverseNeighborDirectionIndexBuffer(),
            &Instance.GetWorldTexelAreaBuffer(),
            &Instance.GetDynamicGeometryBuffer(),
            &Instance.GetAccumulationHeightBuffer(),
            &Instance.GetDynamicConcavityWeightBuffer(),
            &Profiles.GetReactionsBuffer(),
            &Instance.GetReactionScratchBuffer()};

        for (std::size_t SetIndex = 0; SetIndex < Sets.size(); ++SetIndex)
        {
            const bool             bAB = SetIndex == 0;
            const GPU::FGPUBuffer* StateCurrent = bAB ? &Instance.GetStateABuffer() : &Instance.GetStateBBuffer();
            const GPU::FGPUBuffer* StateNext = bAB ? &Instance.GetStateBBuffer() : &Instance.GetStateABuffer();
            std::array<VkDescriptorBufferInfo, DescriptorBindingCount> BufferInfos{};
            std::array<VkWriteDescriptorSet, DescriptorBindingCount>   Writes{};

            for (std::uint32_t BindingNumber = 0; BindingNumber < DescriptorBindingCount; ++BindingNumber)
            {
                const ESurfaceGPUDescriptorBinding Binding = static_cast<ESurfaceGPUDescriptorBinding>(BindingNumber);
                const GPU::FGPUBuffer*             Buffer = SharedAndProfileBuffers[BindingNumber];

                if (Binding == ESurfaceGPUDescriptorBinding::CurrentState)
                {
                    Buffer = StateCurrent;
                }
                else if (Binding == ESurfaceGPUDescriptorBinding::NextState)
                {
                    Buffer = StateNext;
                }

                BufferInfos[BindingNumber] = {Buffer->GetHandle(), 0, Buffer->GetSize()};
                BoundBufferHandles[SetIndex][BindingNumber] = Buffer->GetHandle();

                VkWriteDescriptorSet& Write = Writes[BindingNumber];
                Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                Write.dstSet = Sets[SetIndex];
                Write.dstBinding = BindingIndex(Binding);
                Write.descriptorCount = 1;
                Write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                Write.pBufferInfo = &BufferInfos[BindingNumber];
            }

            vkUpdateDescriptorSets(Device, static_cast<std::uint32_t>(Writes.size()), Writes.data(), 0, nullptr);
        }
    }

    FSurfaceStateDescriptorResources::~FSurfaceStateDescriptorResources()
    {
        if (Pool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(Device, Pool, nullptr);
            Pool = VK_NULL_HANDLE;
        }

        if (Layout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(Device, Layout, nullptr);
            Layout = VK_NULL_HANDLE;
        }
    }

    VkDescriptorSetLayout FSurfaceStateDescriptorResources::GetLayout() const noexcept
    {
        return Layout;
    }

    VkDescriptorSet FSurfaceStateDescriptorResources::GetABSet() const noexcept
    {
        return Sets[0];
    }

    VkDescriptorSet FSurfaceStateDescriptorResources::GetBASet() const noexcept
    {
        return Sets[1];
    }

    VkBuffer FSurfaceStateDescriptorResources::GetBoundBufferHandle(ESurfaceGPUDescriptorBinding Binding,
                                                                    bool                         bAB) const
    {
        const std::size_t BindingNumber = static_cast<std::size_t>(Binding);

        if (BindingNumber >= static_cast<std::size_t>(ESurfaceGPUDescriptorBinding::Count))
        {
            throw std::out_of_range("Surface descriptor binding is outside the declared layout.");
        }

        return BoundBufferHandles[bAB ? 0U : 1U][BindingNumber];
    }

    std::array<std::uint64_t, 6> FSurfaceStateDescriptorResources::GetGeometryInputRevisions() const noexcept
    {
        return {SharedGeometryResources->GetTexelSurfaceIndexBuffer().GetUploadRevision(),
                SharedGeometryResources->GetPositionBuffer().GetUploadRevision(),
                SharedGeometryResources->GetNormalBuffer().GetUploadRevision(),
                SharedGeometryResources->GetGeometryScalarBuffer().GetUploadRevision(),
                SharedGeometryResources->GetNeighborIndexBuffer().GetUploadRevision(),
                SharedGeometryResources->GetMesoNormalBuffer().GetUploadRevision()};
    }

#pragma endregion
}
