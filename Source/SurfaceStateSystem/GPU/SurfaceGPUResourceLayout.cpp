/**
 * @file SurfaceGPUResourceLayout.cpp
 * @brief Pack validated CPU Surface data into the selected shader-facing arrays.
 */

#include "SurfaceStateSystem/GPU/SurfaceGPUResourceLayout.h"

#include <limits>
#include <stdexcept>

namespace MDSS
{
    namespace
    {
        TSurfaceGPUVec4 ToGPUVec4(const glm::vec3& Value)
        {
            return {Value.x, Value.y, Value.z, 0.0F};
        }
    } // namespace

    TSurfaceGPUSharedGeometryUpload PackSharedSurfaceGeometry(const TSharedSurfaceGeometryData& Geometry)
    {
        const std::size_t TexelCount = Geometry.GetTexelCount();
        if (TexelCount == 0)
        {
            throw std::invalid_argument("Cannot pack empty Surface Geometry.");
        }
        if (Geometry.GetProfileMap().size() != TexelCount)
        {
            throw std::invalid_argument("Surface Geometry Profile map size does not match its texel count.");
        }

        TSurfaceGPUSharedGeometryUpload Result;
        Result.TexelSurfaceIndices.reserve(TexelCount);
        Result.TexelProfileIndices = Geometry.GetProfileMap();
        Result.Positions.reserve(TexelCount);
        Result.Normals.reserve(TexelCount);
        Result.GeometryScalars.reserve(TexelCount);
        Result.NeighborIndices.reserve(TexelCount);

        for (const TSurfaceTexelGeometry& Texel : Geometry.GetTexels())
        {
            Result.TexelSurfaceIndices.push_back(Texel.Surface);
            Result.Positions.push_back(ToGPUVec4(Texel.Position));
            Result.Normals.push_back(ToGPUVec4(Texel.Normal));
            Result.GeometryScalars.push_back({Texel.Geometry.MesoVirtualHeight, Texel.Geometry.ConcavityWeight});
            Result.NeighborIndices.push_back({Texel.NeighborIndices});
        }

        return Result;
    }

    TSurfaceGPUProfileUpload PackSurfaceProfiles(const std::vector<TSurfaceResponseProfileData>& Profiles,
                                                 const TSurfaceStateRegistry& Registry)
    {
        if (Profiles.empty())
        {
            throw std::invalid_argument("Cannot pack an empty Surface Profile table.");
        }
        const std::size_t ChannelCount = Registry.GetStateCount();
        if (ChannelCount == 0)
        {
            throw std::invalid_argument("Cannot pack Surface Profiles without registered State channels.");
        }
        if (Profiles.size() > std::numeric_limits<std::size_t>::max() / ChannelCount)
        {
            throw std::overflow_error("Surface Profile table element count overflowed.");
        }

        const std::size_t RecordCount = Profiles.size() * ChannelCount;
        TSurfaceGPUProfileUpload Result;
        Result.Parameters.resize(RecordCount);
        Result.Supported.resize(RecordCount, 0U);

        for (std::size_t ProfileIndex = 0; ProfileIndex < Profiles.size(); ++ProfileIndex)
        {
            const TRegisteredSurfaceResponseProfileData Resolved = Registry.ResolveProfile(Profiles[ProfileIndex]);
            for (std::size_t ChannelIndex = 0; ChannelIndex < ChannelCount; ++ChannelIndex)
            {
                const std::size_t RecordIndex = ProfileIndex * ChannelCount + ChannelIndex;
                if (!Resolved.States[ChannelIndex].has_value())
                {
                    continue;
                }

                const TSurfaceStateParameters& Parameters = *Resolved.States[ChannelIndex];
                Result.Parameters[RecordIndex] = {
                    {Parameters.StateCapacity,
                     Parameters.InputFactor,
                     Parameters.SaturationTransferRate,
                     Parameters.GeometryTransferRate},
                    {Parameters.DecayRate,
                     Parameters.CavityRetentionFactor,
                     Parameters.AccumulationFactor,
                     Parameters.CavityFillFactor}};
                Result.Supported[RecordIndex] = 1U;
            }
        }

        return Result;
    }

    std::size_t GetSurfaceGPUStateValueIndex(std::size_t TexelIndex,
                                             std::size_t ChannelIndex,
                                             std::size_t ChannelCount)
    {
        if (ChannelCount == 0 || ChannelIndex >= ChannelCount)
        {
            throw std::out_of_range("Surface State channel index is outside the Registry range.");
        }
        if (TexelIndex > (std::numeric_limits<std::size_t>::max() - ChannelIndex) / ChannelCount)
        {
            throw std::overflow_error("Surface State scalar index overflowed.");
        }
        return TexelIndex * ChannelCount + ChannelIndex;
    }

    std::size_t GetSurfaceGPUProfileRecordIndex(std::size_t ProfileIndex,
                                                std::size_t ChannelIndex,
                                                std::size_t ChannelCount)
    {
        if (ChannelCount == 0 || ChannelIndex >= ChannelCount)
        {
            throw std::out_of_range("Surface Profile channel index is outside the Registry range.");
        }
        if (ProfileIndex > (std::numeric_limits<std::size_t>::max() - ChannelIndex) / ChannelCount)
        {
            throw std::overflow_error("Surface Profile record index overflowed.");
        }
        return ProfileIndex * ChannelCount + ChannelIndex;
    }

    std::size_t GetSurfaceGPUBufferByteSize(std::size_t ElementCount,
                                            std::size_t ElementStride,
                                            std::size_t MaxStorageBufferRange)
    {
        if (ElementCount == 0 || ElementStride == 0)
        {
            throw std::invalid_argument("Surface GPU buffers require non-zero element count and stride.");
        }
        if (ElementCount > std::numeric_limits<std::size_t>::max() / ElementStride)
        {
            throw std::overflow_error("Surface GPU buffer byte size overflowed.");
        }
        const std::size_t ByteSize = ElementCount * ElementStride;
        if (ByteSize > MaxStorageBufferRange)
        {
            throw std::length_error("Surface GPU buffer exceeds maxStorageBufferRange.");
        }
        return ByteSize;
    }
} // namespace MDSS
