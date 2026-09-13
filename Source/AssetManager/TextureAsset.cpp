#include "AssetManager/TextureAsset.h"

#include "Logger/Logger.h"
#include "VulkanContext/GPU/GPUBuffer.h"
#include "VulkanContext/VulkanContext.h"

#include <stdexcept>
#include <utility>

namespace MDSS
{
    TextureAsset::TextureAsset(AssetID                          ID,
                               std::string                      Name,
                               std::filesystem::path            SourcePath,
                               const VulkanContext&             Context,
                               std::uint32_t                    Width,
                               std::uint32_t                    Height,
                               const std::vector<std::uint8_t>& RGBA8Pixels,
                               VkFormat                         Format)
        : Asset(ID, std::move(Name), std::move(SourcePath)), Width(Width), Height(Height), Format(Format)
    {
        if (Width == 0 || Height == 0 || RGBA8Pixels.size() != static_cast<std::size_t>(Width) * Height * 4U)
        {
            throw std::invalid_argument("TextureAsset requires valid RGBA8 pixel data.");
        }

        const VkDeviceSize ByteCount = static_cast<VkDeviceSize>(RGBA8Pixels.size());
        GPUBuffer          StagingBuffer(Context.GetPhysicalDevice(),
                                Context.GetDevice(),
                                ByteCount,
                                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        StagingBuffer.Upload(RGBA8Pixels.data(), ByteCount);

        Image = std::make_unique<GPUImage>(Context.GetPhysicalDevice(),
                                           Context.GetDevice(),
                                           VkExtent2D{Width, Height},
                                           Format,
                                           VK_IMAGE_TILING_OPTIMAL,
                                           VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        TransitionImageLayout(
            Context, Image->GetHandle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        CopyBufferToImage(Context, StagingBuffer.GetHandle(), Image->GetHandle(), Width, Height);
        TransitionImageLayout(Context,
                              Image->GetHandle(),
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        ImageView =
            std::make_unique<GPUImageView>(Context.GetDevice(), Image->GetHandle(), Format, VK_IMAGE_ASPECT_COLOR_BIT);
        Sampler = std::make_unique<GPUSampler>(Context.GetDevice());
        Logger::Debug("AssetManager",
                      "Uploaded TextureAsset '" + GetName() + "' to GPU (" + std::to_string(Width) + "x" +
                          std::to_string(Height) + ", format=" + std::to_string(static_cast<int>(Format)) + ").");
    }

    std::uint32_t TextureAsset::GetWidth() const noexcept
    {
        return Width;
    }

    std::uint32_t TextureAsset::GetHeight() const noexcept
    {
        return Height;
    }

    VkFormat TextureAsset::GetFormat() const noexcept
    {
        return Format;
    }

    VkImageView TextureAsset::GetImageView() const noexcept
    {
        return ImageView->GetHandle();
    }

    VkSampler TextureAsset::GetSampler() const noexcept
    {
        return Sampler->GetHandle();
    }

    void TextureAsset::TransitionImageLayout(const VulkanContext& Context,
                                             VkImage              Image,
                                             VkImageLayout        OldLayout,
                                             VkImageLayout        NewLayout)
    {
        VkCommandBuffer CommandBuffer = Context.GetCommands().BeginSingleTime();

        VkImageMemoryBarrier Barrier{};
        Barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        Barrier.oldLayout = OldLayout;
        Barrier.newLayout = NewLayout;
        Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        Barrier.image = Image;
        Barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        Barrier.subresourceRange.baseMipLevel = 0;
        Barrier.subresourceRange.levelCount = 1;
        Barrier.subresourceRange.baseArrayLayer = 0;
        Barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags SourceStage = 0;
        VkPipelineStageFlags DestinationStage = 0;

        if (OldLayout == VK_IMAGE_LAYOUT_UNDEFINED && NewLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            Barrier.srcAccessMask = 0;
            Barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            SourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            DestinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (OldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                 NewLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            Barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            Barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            SourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            DestinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else
        {
            Context.GetCommands().EndSingleTime(CommandBuffer, Context.GetQueues().GetGraphics());
            throw std::invalid_argument("Unsupported texture image layout transition.");
        }

        vkCmdPipelineBarrier(CommandBuffer, SourceStage, DestinationStage, 0, 0, nullptr, 0, nullptr, 1, &Barrier);

        Context.GetCommands().EndSingleTime(CommandBuffer, Context.GetQueues().GetGraphics());
    }

    void TextureAsset::CopyBufferToImage(
        const VulkanContext& Context, VkBuffer Buffer, VkImage Image, std::uint32_t Width, std::uint32_t Height)
    {
        VkCommandBuffer CommandBuffer = Context.GetCommands().BeginSingleTime();

        VkBufferImageCopy Region{};
        Region.bufferOffset = 0;
        Region.bufferRowLength = 0;
        Region.bufferImageHeight = 0;
        Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        Region.imageSubresource.mipLevel = 0;
        Region.imageSubresource.baseArrayLayer = 0;
        Region.imageSubresource.layerCount = 1;
        Region.imageOffset = {0, 0, 0};
        Region.imageExtent = {Width, Height, 1};

        vkCmdCopyBufferToImage(CommandBuffer, Buffer, Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Region);
        Context.GetCommands().EndSingleTime(CommandBuffer, Context.GetQueues().GetGraphics());
    }
} // namespace MDSS
