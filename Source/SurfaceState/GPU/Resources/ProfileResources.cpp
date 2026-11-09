#include "SurfaceState/GPU/Resources/ProfileResources.h"

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
// 00. Profile Resources
// ============================================================
#pragma region 00_Profile_Resources

    FSurfaceProfileGPUResources::FSurfaceProfileGPUResources(VkPhysicalDevice PhysicalDevice,
                                                             VkDevice         Device,
                                                             const std::vector<FSurfaceResponseProfileData>& Profiles,
                                                             const FSurfaceStateRegistry&                    Registry,
                                                             FSurfaceGPUBufferPool*                          Pool)
        : ProfileCount(Profiles.size()), ChannelCount(Registry.GetStateCount())
    {
        const std::size_t              MaxRange = GetMaximumStorageBufferRange(PhysicalDevice);
        const FSurfaceGPUProfileUpload Upload = PackSurfaceProfiles(Profiles, Registry);
        SupportedChannels = Upload.Supported;
        MaxReactionCount = Upload.MaxReactionCount;
        ReactionsBuffer = CreateUploadedBuffer(PhysicalDevice,
                                               Device,
                                               Upload.Reactions.data(),
                                               Upload.Reactions.size(),
                                               sizeof(std::uint32_t),
                                               MaxRange,
                                               GPU::EGPUBufferMemoryCategory::SurfaceProfile,
                                               Pool);

        // GPU hot path uses a 3-plane vec4 SoA layout instead of 48-byte AoS records:
        // [Capacity/Input/Transfer records][Decay/Geometry records][Accumulation/Thickness records].
        // This keeps the descriptor count unchanged while allowing shaders that only need capacity or
        // accumulation fields to fetch one 16-byte vector instead of touching the whole logical record.
        std::vector<FSurfaceGPUVec4> ParameterPlanes(Upload.Parameters.size() * 3U);
        const std::size_t            RecordCount = Upload.Parameters.size();

        for (std::size_t Record = 0; Record < RecordCount; ++Record)
        {
            const auto& P = Upload.Parameters[Record];
            ParameterPlanes[Record] = {P.CapacityInputAndTransfer[0],
                                       P.CapacityInputAndTransfer[1],
                                       P.CapacityInputAndTransfer[2],
                                       P.CapacityInputAndTransfer[3]};
            ParameterPlanes[RecordCount + Record] = {
                P.DecayAndGeometry[0], P.DecayAndGeometry[1], P.DecayAndGeometry[2], P.DecayAndGeometry[3]};
            ParameterPlanes[RecordCount * 2U + Record] = {P.AccumulationThickness[0],
                                                          P.AccumulationThickness[1],
                                                          P.AccumulationThickness[2],
                                                          P.AccumulationThickness[3]};
        }

        ParametersBuffer = CreateUploadedBuffer(PhysicalDevice,
                                                Device,
                                                ParameterPlanes.data(),
                                                ParameterPlanes.size(),
                                                sizeof(FSurfaceGPUVec4),
                                                MaxRange,
                                                GPU::EGPUBufferMemoryCategory::SurfaceProfile,
                                                Pool);
        SupportedBuffer = CreateUploadedBuffer(PhysicalDevice,
                                               Device,
                                               Upload.Supported.data(),
                                               Upload.Supported.size(),
                                               sizeof(std::uint32_t),
                                               MaxRange,
                                               GPU::EGPUBufferMemoryCategory::SurfaceProfile,
                                               Pool);
    }

    const GPU::FGPUBuffer& FSurfaceProfileGPUResources::GetParametersBuffer() const noexcept
    {
        return *ParametersBuffer;
    }

    void FSurfaceProfileGPUResources::RetireBuffers(FSurfaceGPUBufferPool& Pool) noexcept
    {
        Pool.Release(std::move(ParametersBuffer));
        Pool.Release(std::move(SupportedBuffer));
        Pool.Release(std::move(ReactionsBuffer));
    }

    const GPU::FGPUBuffer& FSurfaceProfileGPUResources::GetSupportedBuffer() const noexcept
    {
        return *SupportedBuffer;
    }

    std::size_t FSurfaceProfileGPUResources::GetProfileCount() const noexcept
    {
        return ProfileCount;
    }

    std::size_t FSurfaceProfileGPUResources::GetChannelCount() const noexcept
    {
        return ChannelCount;
    }

    bool FSurfaceProfileGPUResources::IsSupported(std::size_t ProfileIndex, std::size_t ChannelIndex) const noexcept
    {
        return ProfileIndex < ProfileCount && ChannelIndex < ChannelCount &&
               SupportedChannels[ProfileIndex * ChannelCount + ChannelIndex] != 0U;
    }

    void FSurfaceProfileGPUResources::UpdateParameters(std::size_t                    ProfileIndex,
                                                       std::size_t                    ChannelIndex,
                                                       const FSurfaceStateParameters& Parameters)
    {
        if (ProfileIndex >= ProfileCount || ChannelIndex >= ChannelCount)
        {
            throw std::out_of_range("Surface Profile parameter index is outside the GPU table.");
        }

        const std::size_t RecordIndex = GetSurfaceGPUProfileRecordIndex(ProfileIndex, ChannelIndex, ChannelCount);

        if (RecordIndex >= SupportedChannels.size() || SupportedChannels[RecordIndex] == 0U)
        {
            throw std::invalid_argument("Cannot override a State that this Profile does not support.");
        }

        const std::array<FSurfaceGPUVec4, 3> Packed{
            FSurfaceGPUVec4{Parameters.StateCapacity,
                            Parameters.InputFactor,
                            Parameters.SaturationTransferFactor,
                            Parameters.GeometryTransferFactor},
            FSurfaceGPUVec4{Parameters.DecayRate,
                            Parameters.CavityRetentionFactor,
                            Parameters.AccumulationFactor,
                            Parameters.CavityFillFactor},
            FSurfaceGPUVec4{Parameters.ThicknessPerAmount, Parameters.CavityTransportRetentionFactor, 0.0F, 0.0F}};
        const VkDeviceSize PlaneStride =
            static_cast<VkDeviceSize>(ProfileCount * ChannelCount * sizeof(FSurfaceGPUVec4));
        const VkDeviceSize RecordOffset = static_cast<VkDeviceSize>(RecordIndex * sizeof(FSurfaceGPUVec4));

        for (std::size_t Plane = 0; Plane < Packed.size(); ++Plane)
            ParametersBuffer->Upload(
                &Packed[Plane], sizeof(FSurfaceGPUVec4), static_cast<VkDeviceSize>(Plane) * PlaneStride + RecordOffset);
    }

#pragma endregion
}
