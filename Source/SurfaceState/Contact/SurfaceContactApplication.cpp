/**
 * @file SurfaceStateSystem.cpp
 * @brief SurfaceStateSystem의 구현을 제공한다.
 */

#include "AssetManager/Assets/MeshAsset.h"
#include "AssetManager/Assets/SRProfileAsset.h"
#include "AssetManager/Core/AssetManager.h"
#include "GPU/Vulkan/VulkanContext.h"
#include "Logger/Logger.h"
#include "Scene/Scene.h"
#include "Scene/StaticMeshInstance.h"
#include "SurfaceState/GPU/Packing/SurfaceGPUResourceLayout.h"
#include "SurfaceState/Geometry/SharedSurfaceGeometryData.h"
#include "SurfaceState/Runtime/SurfaceDataManager.h"
#include "SurfaceState/Runtime/SurfaceRuntimeData.h"
#include "SurfaceState/SurfaceStateSystem.h"
#include "SurfaceState/Types/SurfaceSolverRates.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace MDSS::SurfaceState
{
// ============================================================
// 00. Pending Contact Application
// ============================================================
#pragma region 00_Pending_Contact_Application

    void FSurfaceStateSystem::ApplyPendingContacts()
    {
        if (PendingContacts.empty())
        {
            return;
        }

        const std::size_t InstanceCount = Scene.GetStaticMeshInstances().size();
        const std::size_t ChannelCount = SurfaceData.GetSurfaceStateRegistry().GetStateCount();

        if (ChannelCount == 0)
        {
            PendingContacts.clear();
            return;
        }

        std::vector<std::vector<float>>         InputDeltas(InstanceCount);
        std::vector<std::vector<std::uint8_t>>  InputGroupFlags(InstanceCount);
        std::vector<std::vector<std::uint32_t>> InputGroupMasks(InstanceCount);
        std::vector<std::uint32_t>              InputChannelMasks(InstanceCount, 0U);
        std::vector<bool>                       bInstanceHasInput(InstanceCount, false);

        for (const FSurfaceContactInput& Contact : PendingContacts)
        {
            bool bLoggedDiagnostic = false;
            auto Diagnose = [&bLoggedDiagnostic](const std::string& Message)
            {
                if (!bLoggedDiagnostic)
                {
                    FLogger::Warning("TSurfaceStateSystem", Message);
                    bLoggedDiagnostic = true;
                }
            };

            const std::size_t InstanceIndex = Contact.TargetInstance;

            if (InstanceIndex >= InstanceCount || Contact.State >= ChannelCount ||
                !std::isfinite(Contact.WorldPosition.x) || !std::isfinite(Contact.WorldPosition.y) ||
                !std::isfinite(Contact.WorldPosition.z) || !std::isfinite(Contact.Radius) || Contact.Radius <= 0.0F ||
                !std::isfinite(Contact.Strength) || Contact.Strength < 0.0F || !std::isfinite(Contact.Falloff) ||
                Contact.Falloff < 0.0F || !std::isfinite(Contact.SecondaryRadius) ||
                !std::isfinite(Contact.RotationDegrees) || !std::isfinite(Contact.PlaneHalfDepth) ||
                (Contact.Shape != ESurfaceContactShape::Circle && Contact.SecondaryRadius <= 0.0F) ||
                (Contact.bPlanar && (Contact.PlaneHalfDepth <= 0.0F || !std::isfinite(Contact.PlaneNormal.x) ||
                                     !std::isfinite(Contact.PlaneNormal.y) || !std::isfinite(Contact.PlaneNormal.z) ||
                                     glm::dot(Contact.PlaneNormal, Contact.PlaneNormal) <= 1.0e-12F)))
            {
                Diagnose("Rejected contact with an invalid instance, State, radius, strength, or falloff.");
                continue;
            }

            const FStaticMeshInstance&      Instance = Scene.GetStaticMeshInstances()[InstanceIndex];
            const FSurfaceRuntimeDataHandle SurfaceDataHandle = Instance.GetSurfaceData();

            if (!SurfaceData.HasSurfaceData(SurfaceDataHandle))
            {
                Diagnose("Rejected contact for an instance without Surface simulation data.");
                continue;
            }

            const FSurfaceStateDescriptorResources* Descriptors = GPUResources->GetInstanceDescriptors(InstanceIndex);

            if (Descriptors == nullptr)
            {
                Diagnose("Rejected contact because the target instance has no GPU State resources.");
                continue;
            }

            const FSurfaceRuntimeData&                RuntimeData = SurfaceData.GetSurfaceData(SurfaceDataHandle);
            const FSharedSurfaceGeometryData&         Geometry = *RuntimeData.GetSharedGeometry();
            const std::size_t                         TexelCount = Geometry.GetTexelCount();
            const std::size_t                         ScalarCount = TexelCount * ChannelCount;
            const glm::mat4                           Model = Instance.GetTransform().GetMatrix();
            const std::vector<FSurfaceTexelGeometry>& Texels = Geometry.GetTexels();
            glm::vec3                                 InfluenceCenter = Contact.WorldPosition;

            if (Contact.bHasSimulationMapping)
            {
                const Asset::FMeshAsset&                       Mesh = Assets.GetMesh(Instance.GetMesh());
                const std::vector<Asset::FMeshTriangleSource>& Triangles = Mesh.GetTriangles();

                if (Contact.TargetTriangle >= Triangles.size() || !std::isfinite(Contact.SimulationUV.x) ||
                    !std::isfinite(Contact.SimulationUV.y) || Contact.SimulationUV.x < 0.0F ||
                    Contact.SimulationUV.x > 1.0F || Contact.SimulationUV.y < 0.0F || Contact.SimulationUV.y > 1.0F)
                {
                    Diagnose("Rejected contact because its hit triangle or Simulation UV is invalid.");
                    continue;
                }

                const FSurfaceLocalID TargetSurface = Triangles[Contact.TargetTriangle].Surface;

                if (TargetSurface >= Geometry.GetSurfaces().size())
                {
                    Diagnose("Rejected contact because its hit triangle has no mapped Surface.");
                    continue;
                }

                const FSurfaceTexelRange& Range = Geometry.GetSurface(TargetSurface);
                const std::uint32_t       CenterX =
                    std::min(static_cast<std::uint32_t>(Contact.SimulationUV.x * Range.Resolution.Width),
                             Range.Resolution.Width - 1U);
                const std::uint32_t CenterY =
                    std::min(static_cast<std::uint32_t>(Contact.SimulationUV.y * Range.Resolution.Height),
                             Range.Resolution.Height - 1U);
                const std::uint32_t MaxSearchRadius = std::max(Range.Resolution.Width, Range.Resolution.Height);
                const std::uint32_t EffectiveSearchRadius = std::min(Contact.TexelSearchRadius, MaxSearchRadius);
                const std::int32_t  SearchRadius = static_cast<std::int32_t>(EffectiveSearchRadius);
                std::optional<FLocalTexelIndex> ResolvedCenter;
                const auto                      IsHitTriangleTexel = [&](std::uint32_t X, std::uint32_t Y)
                {
                    const std::size_t Index = static_cast<std::size_t>(Range.FirstTexel) +
                                              static_cast<std::size_t>(Y) * Range.Resolution.Width + X;
                    if (Index >= Texels.size())
                    {
                        return false;
                    }

                    const FSurfaceTexelGeometry& Texel = Texels[Index];

                    if (Texel.IsValid() && Texel.Surface == TargetSurface && Texel.Triangle == Contact.TargetTriangle)
                    {
                        ResolvedCenter = static_cast<FLocalTexelIndex>(Index);
                        return true;
                    }

                    return false;
                };

                if (!IsHitTriangleTexel(CenterX, CenterY))
                {
                    std::uint32_t BestDistanceSquared = std::numeric_limits<std::uint32_t>::max();

                    for (std::int32_t OffsetY = -SearchRadius; OffsetY <= SearchRadius; ++OffsetY)
                    {
                        for (std::int32_t OffsetX = -SearchRadius; OffsetX <= SearchRadius; ++OffsetX)
                        {
                            if (OffsetX == 0 && OffsetY == 0)
                            {
                                continue;
                            }

                            const std::int32_t X = static_cast<std::int32_t>(CenterX) + OffsetX;
                            const std::int32_t Y = static_cast<std::int32_t>(CenterY) + OffsetY;

                            if (X < 0 || Y < 0 || X >= static_cast<std::int32_t>(Range.Resolution.Width) ||
                                Y >= static_cast<std::int32_t>(Range.Resolution.Height))
                            {
                                continue;
                            }

                            const std::uint32_t DistanceSquared =
                                static_cast<std::uint32_t>(OffsetX * OffsetX + OffsetY * OffsetY);
                            if (DistanceSquared >= BestDistanceSquared)
                            {
                                continue;
                            }

                            const std::optional<FLocalTexelIndex> Previous = ResolvedCenter;

                            if (IsHitTriangleTexel(static_cast<std::uint32_t>(X), static_cast<std::uint32_t>(Y)))
                            {
                                BestDistanceSquared = DistanceSquared;
                            }
                            else
                            {
                                ResolvedCenter = Previous;
                            }
                        }
                    }
                }

                if (!ResolvedCenter.has_value())
                {
                    std::uint32_t NearestDistanceSquared = std::numeric_limits<std::uint32_t>::max();
                    std::int32_t  NearestOffsetX = 0;
                    std::int32_t  NearestOffsetY = 0;
                    std::uint32_t NearestX = 0;
                    std::uint32_t NearestY = 0;

                    for (std::uint32_t Y = 0; Y < Range.Resolution.Height; ++Y)
                    {
                        for (std::uint32_t X = 0; X < Range.Resolution.Width; ++X)
                        {
                            const std::size_t Index = static_cast<std::size_t>(Range.FirstTexel) +
                                                      static_cast<std::size_t>(Y) * Range.Resolution.Width + X;
                            if (Index >= Texels.size())
                            {
                                continue;
                            }

                            const FSurfaceTexelGeometry& Texel = Texels[Index];

                            if (!Texel.IsValid() || Texel.Surface != TargetSurface ||
                                Texel.Triangle != Contact.TargetTriangle)
                            {
                                continue;
                            }

                            const std::int32_t OffsetX =
                                static_cast<std::int32_t>(X) - static_cast<std::int32_t>(CenterX);
                            const std::int32_t OffsetY =
                                static_cast<std::int32_t>(Y) - static_cast<std::int32_t>(CenterY);
                            const std::uint32_t DistanceSquared =
                                static_cast<std::uint32_t>(OffsetX * OffsetX + OffsetY * OffsetY);
                            if (DistanceSquared < NearestDistanceSquared)
                            {
                                NearestDistanceSquared = DistanceSquared;
                                NearestOffsetX = OffsetX;
                                NearestOffsetY = OffsetY;
                                NearestX = X;
                                NearestY = Y;
                            }
                        }
                    }

                    if (NearestDistanceSquared == std::numeric_limits<std::uint32_t>::max())
                    {
                        Diagnose("Rejected contact: the hit triangle has no valid simulation texels at this "
                                 "resolution; no texel center was rasterized inside it. Surface=" +
                                 std::to_string(TargetSurface) +
                                 ", triangle=" + std::to_string(Contact.TargetTriangle) + ", UV=(" +
                                 std::to_string(Contact.SimulationUV.x) + ", " +
                                 std::to_string(Contact.SimulationUV.y) +
                                 "), grid=" + std::to_string(Range.Resolution.Width) + "x" +
                                 std::to_string(Range.Resolution.Height) + ".");
                    }
                    else
                    {
                        Diagnose("Rejected contact: the nearest valid texel for the hit triangle is outside the "
                                 "fallback search window (±" +
                                 std::to_string(EffectiveSearchRadius) +
                                 " texels per axis). Surface=" + std::to_string(TargetSurface) +
                                 ", triangle=" + std::to_string(Contact.TargetTriangle) + ", UV=(" +
                                 std::to_string(Contact.SimulationUV.x) + ", " +
                                 std::to_string(Contact.SimulationUV.y) +
                                 "), grid=" + std::to_string(Range.Resolution.Width) + "x" +
                                 std::to_string(Range.Resolution.Height) + ", center=(" + std::to_string(CenterX) +
                                 ", " + std::to_string(CenterY) + "), nearest=(" + std::to_string(NearestX) + ", " +
                                 std::to_string(NearestY) + "), offset=(" + std::to_string(NearestOffsetX) + ", " +
                                 std::to_string(NearestOffsetY) + ").");
                    }

                    continue;
                }

                InfluenceCenter = glm::vec3(Model * glm::vec4(Texels[*ResolvedCenter].Position, 1.0F));
            }

            if (InputDeltas[InstanceIndex].empty())
            {
                InputDeltas[InstanceIndex].assign(ScalarCount, 0.0F);
                InputGroupFlags[InstanceIndex].assign((TexelCount + 63U) / 64U, 0U);
                InputGroupMasks[InstanceIndex].assign((TexelCount + 63U) / 64U, 0U);
            }

            const std::vector<Asset::FSRProfileAssetHandle>& ProfileHandles =
                SurfaceData.GetSurfaceProfileTable(SurfaceDataHandle);
            std::vector<float> InputFactors(ProfileHandles.size(), 0.0F);
            std::vector<bool>  bProfileSupportsState(ProfileHandles.size(), false);

            for (std::size_t ProfileIndex = 0; ProfileIndex < ProfileHandles.size(); ++ProfileIndex)
            {
                const FRegisteredSurfaceResponseProfileData Resolved =
                    SurfaceData.GetSurfaceStateRegistry().ResolveProfile(
                        Assets.GetSRProfile(ProfileHandles[ProfileIndex]).GetData());
                if (Contact.State < Resolved.States.size() && Resolved.States[Contact.State].has_value())
                {
                    InputFactors[ProfileIndex] = Resolved.States[Contact.State]->InputFactor;
                    bProfileSupportsState[ProfileIndex] = true;
                    const auto Override = RuntimeProfileOverrides.find({ProfileHandles[ProfileIndex], Contact.State});

                    if (Override != RuntimeProfileOverrides.end())
                    {
                        InputFactors[ProfileIndex] = Override->second.InputFactor;
                    }
                }
            }

            bool              bAppliedToAnyTexel = false;
            bool              bUnsupportedProfile = false;
            bool              bOverlappedNoSimulationSurface = false;
            std::size_t       AppliedTexelCount = 0;
            const std::size_t StateChannel = Contact.State;
            const glm::vec3   PlaneNormal = Contact.bPlanar ? glm::normalize(Contact.PlaneNormal) : glm::vec3(0, 0, 1);
            const glm::vec3   Reference = std::abs(PlaneNormal.x) < 0.9F ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
            const glm::vec3   Tangent = glm::normalize(Reference - PlaneNormal * glm::dot(Reference, PlaneNormal));
            const glm::vec3   Bitangent = glm::cross(PlaneNormal, Tangent);
            const float       Angle = glm::radians(Contact.RotationDegrees);
            const float       CosAngle = std::cos(Angle);
            const float       SinAngle = std::sin(Angle);

            for (std::size_t TexelIndex = 0; TexelIndex < Texels.size(); ++TexelIndex)
            {
                const FSurfaceTexelGeometry& Texel = Texels[TexelIndex];

                if (!Texel.IsValid())
                {
                    continue;
                }

                const FSurfaceProfileIndex ProfileIndex =
                    Geometry.GetProfileIndex(static_cast<FLocalTexelIndex>(TexelIndex));
                const glm::vec3 WorldTexelPosition = glm::vec3(Model * glm::vec4(Texel.Position, 1.0F));
                const glm::vec3 Offset = WorldTexelPosition - InfluenceCenter;
                float           NormalizedDistance = glm::length(Offset) / Contact.Radius;

                if (Contact.bPlanar)
                {
                    if (std::abs(glm::dot(Offset, PlaneNormal)) > Contact.PlaneHalfDepth)
                        continue;
                    const float U = glm::dot(Offset, Tangent);
                    const float V = glm::dot(Offset, Bitangent);
                    const float X = CosAngle * U + SinAngle * V;
                    const float Y = -SinAngle * U + CosAngle * V;

                    switch (Contact.Shape)
                    {
                        case ESurfaceContactShape::Circle:
                            NormalizedDistance = glm::length(glm::vec2(X, Y)) / Contact.Radius;
                            break;
                        case ESurfaceContactShape::Ellipse:
                            NormalizedDistance =
                                glm::length(glm::vec2(X / Contact.Radius, Y / Contact.SecondaryRadius));
                            break;
                        case ESurfaceContactShape::Rectangle:
                            NormalizedDistance =
                                std::max(std::abs(X) / Contact.Radius, std::abs(Y) / Contact.SecondaryRadius);
                            break;
                        case ESurfaceContactShape::Capsule:
                            NormalizedDistance =
                                glm::length(glm::vec2(std::max(std::abs(X) - Contact.SecondaryRadius, 0.0F), Y)) /
                                Contact.Radius;
                            break;
                    }
                }

                if (!std::isfinite(NormalizedDistance) || NormalizedDistance > 1.0F)
                {
                    continue;
                }

                if (ProfileIndex == InvalidSurfaceProfileIndex)
                {
                    bOverlappedNoSimulationSurface = true;
                    continue;
                }

                if (ProfileIndex >= InputFactors.size() || !bProfileSupportsState[ProfileIndex])
                {
                    bUnsupportedProfile = true;
                    continue;
                }

                const float LinearFalloff = 1.0F - NormalizedDistance;
                const float ContactWeight = Contact.Falloff == 0.0F ? 1.0F : std::pow(LinearFalloff, Contact.Falloff);
                const std::size_t ScalarIndex = GetSurfaceGPUStateValueIndex(TexelIndex, StateChannel, ChannelCount);
                const float       AreaScale = GetSurfaceWorldTexelArea(Texel, Model) / SurfaceStateReferenceArea;
                const float       Sign = Contact.Operation == ESurfaceContactOperation::Remove ? -1.0F : 1.0F;
                const float Delta = Sign * Contact.Strength * ContactWeight * InputFactors[ProfileIndex] * AreaScale;
                InputDeltas[InstanceIndex][ScalarIndex] += Delta;

                if (Delta != 0.0F)
                {
                    InputGroupFlags[InstanceIndex][TexelIndex / 64U] = 1U;

                    if (StateChannel < 32U)
                    {
                        InputChannelMasks[InstanceIndex] |= (1U << static_cast<std::uint32_t>(StateChannel));
                        InputGroupMasks[InstanceIndex][TexelIndex / 64U] |=
                            (1U << static_cast<std::uint32_t>(StateChannel));
                    }
                }

                bAppliedToAnyTexel = true;
                ++AppliedTexelCount;
            }

            if (bAppliedToAnyTexel)
            {
                FLogger::Info("TSurfaceStateSystem",
                              "Contact input applied (state=" +
                                  SurfaceData.GetSurfaceStateRegistry().GetStateName(Contact.State) +
                                  ", instance=" + std::to_string(InstanceIndex) +
                                  ", texels=" + std::to_string(AppliedTexelCount) + ").");
            }

            if (bUnsupportedProfile)
            {
                Diagnose("Some contact texels do not support the requested State; those texels were skipped.");
            }

            if (!bAppliedToAnyTexel && !bUnsupportedProfile && !bOverlappedNoSimulationSurface)
            {
                Diagnose("Contact did not overlap a valid texel; no State input was applied.");
            }

            bInstanceHasInput[InstanceIndex] = bInstanceHasInput[InstanceIndex] || bAppliedToAnyTexel;
        }

        const bool bAnyInput =
            std::any_of(bInstanceHasInput.begin(), bInstanceHasInput.end(), [](bool bValue) { return bValue; });
        if (bAnyInput && vkQueueWaitIdle(Context.GetQueues().GetGraphics()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to wait for the graphics queue before uploading Surface InputDelta.");
        }

        for (std::size_t InstanceIndex = 0; InstanceIndex < InstanceCount; ++InstanceIndex)
        {
            if (!bInstanceHasInput[InstanceIndex])
            {
                continue;
            }

            const GPU::FGPUBuffer&    InputDeltaBuffer = GPUResources->GetInstanceInputDeltaBuffer(InstanceIndex);
            const std::vector<float>& InputDelta = InputDeltas[InstanceIndex];
            const VkDeviceSize        ByteSize = static_cast<VkDeviceSize>(InputDelta.size() * sizeof(float));

            if (ByteSize != InputDeltaBuffer.GetSize())
            {
                throw std::runtime_error("CPU InputDelta size does not match the instance GPU buffer size.");
            }

            InputDeltaBuffer.Upload(InputDelta.data(), ByteSize);

            // Persistent sparse solver does not rescan all texels. CPU contact distribution already knows
            // exactly which workgroups/channels received InputDelta, so upload that compact activation list.
            const std::size_t                  TexelCount = GPUResources->GetInstanceTexelCount(InstanceIndex);
            const FSurfaceSparseMetadataLayout SparseLayout = GetSurfaceSparseMetadataLayout(TexelCount);
            std::vector<std::uint32_t>         InputGroups;
            std::vector<std::uint32_t>         CompactInputGroupMasks;
            InputGroups.reserve(InputGroupFlags[InstanceIndex].size());
            CompactInputGroupMasks.reserve(InputGroupFlags[InstanceIndex].size());

            for (std::size_t Group = 0; Group < InputGroupFlags[InstanceIndex].size(); ++Group)
                if (InputGroupFlags[InstanceIndex][Group] != 0U)
                {
                    InputGroups.push_back(static_cast<std::uint32_t>(Group));
                    CompactInputGroupMasks.push_back(InputGroupMasks[InstanceIndex][Group]);
                }

            if (!InputGroups.empty())
            {
                const GPU::FGPUBuffer& AccumulationBuffer =
                    GPUResources->GetInstanceAccumulationHeightBuffer(InstanceIndex);
                const std::uint32_t GroupCount = static_cast<std::uint32_t>(InputGroups.size());
                const std::uint32_t ChannelMask = InputChannelMasks[InstanceIndex];
                AccumulationBuffer.Upload(
                    &GroupCount,
                    sizeof(GroupCount),
                    static_cast<VkDeviceSize>(SparseLayout.InputCountWord * sizeof(std::uint32_t)));
                AccumulationBuffer.Upload(
                    &ChannelMask,
                    sizeof(ChannelMask),
                    static_cast<VkDeviceSize>(SparseLayout.InputMaskWord * sizeof(std::uint32_t)));
                AccumulationBuffer.Upload(
                    InputGroups.data(),
                    static_cast<VkDeviceSize>(InputGroups.size() * sizeof(std::uint32_t)),
                    static_cast<VkDeviceSize>(SparseLayout.InputListWord * sizeof(std::uint32_t)));
                AccumulationBuffer.Upload(
                    CompactInputGroupMasks.data(),
                    static_cast<VkDeviceSize>(CompactInputGroupMasks.size() * sizeof(std::uint32_t)),
                    static_cast<VkDeviceSize>(SparseLayout.InputGroupMaskWord * sizeof(std::uint32_t)));
                if (InstanceIndex >= Execution.PendingInputActivation.size())
                    Execution.PendingInputActivation.resize(InstanceCount, false);
                Execution.PendingInputActivation[InstanceIndex] = true;
            }
        }

        PendingContacts.clear();
    }

#pragma endregion
}
