/**
 * @file SurfaceStateSystem.cpp
 * @brief Scene-level owner and step recorder for Surface State simulation.
 */

#include "SurfaceStateSystem/SurfaceStateSystem.h"

#include "AssetManager/Core/AssetManager.h"
#include "AssetManager/Assets/SRProfileAsset.h"
#include "AssetManager/Assets/MeshAsset.h"
#include "Logger/Logger.h"
#include "Scene/Scene.h"
#include "Scene/StaticMeshInstance.h"
#include "SurfaceStateSystem/GPU/SurfaceGPUResourceLayout.h"
#include "SurfaceStateSystem/Geometry/SharedSurfaceGeometryData.h"
#include "SurfaceStateSystem/Preprocessing/SurfaceRuntimeData.h"
#include "VulkanContext/VulkanContext.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace MDSS
{
    TSurfaceStateSystem::TSurfaceStateSystem(const TVulkanContext& Context,
                                             const TAssetManager& Assets,
                                             const TScene& Scene)
        : Context(Context), Assets(Assets), Scene(Scene),
          GPUResources(std::make_unique<TSurfaceGPUResourceManager>(Context, Assets, Scene))
    {
        if (const TSurfaceStateDescriptorResources* Descriptors = GPUResources->GetAnyInstanceDescriptors())
        {
            Solver = std::make_unique<TSurfaceStateSolver>(Context.GetDevice(), Descriptors->GetLayout());
        }
    }

    TSurfaceStateSystem::~TSurfaceStateSystem() = default;

    void TSurfaceStateSystem::SubmitContact(TSurfaceContactInput Contact)
    {
        PendingContacts.push_back(std::move(Contact));
    }

    void TSurfaceStateSystem::ApplyPendingContacts()
    {
        if (PendingContacts.empty())
        {
            return;
        }

        const std::size_t InstanceCount = Scene.GetStaticMeshInstances().size();
        const std::size_t ChannelCount = Assets.GetSurfaceStateRegistry().GetStateCount();
        if (ChannelCount == 0)
        {
            PendingContacts.clear();
            return;
        }

        std::vector<std::vector<float>> InputDeltas(InstanceCount);
        std::vector<bool>               bInstanceHasInput(InstanceCount, false);

        for (const TSurfaceContactInput& Contact : PendingContacts)
        {
            bool bLoggedDiagnostic = false;
            auto Diagnose = [&bLoggedDiagnostic](const std::string& Message) {
                if (!bLoggedDiagnostic)
                {
                    TLogger::Warning("TSurfaceStateSystem", Message);
                    bLoggedDiagnostic = true;
                }
            };

            const std::size_t InstanceIndex = Contact.TargetInstance;
            if (InstanceIndex >= InstanceCount || Contact.State >= ChannelCount ||
                !std::isfinite(Contact.WorldPosition.x) || !std::isfinite(Contact.WorldPosition.y) ||
                !std::isfinite(Contact.WorldPosition.z) ||
                !std::isfinite(Contact.Radius) || Contact.Radius <= 0.0F ||
                !std::isfinite(Contact.Strength) || Contact.Strength < 0.0F ||
                !std::isfinite(Contact.Falloff) || Contact.Falloff < 0.0F)
            {
                Diagnose("Rejected contact with an invalid instance, State, radius, strength, or falloff.");
                continue;
            }

            const TStaticMeshInstance& Instance = Scene.GetStaticMeshInstances()[InstanceIndex];
            const TSurfaceRuntimeDataHandle SurfaceDataHandle = Instance.GetSurfaceData();
            if (!Assets.HasSurfaceData(SurfaceDataHandle))
            {
                Diagnose("Rejected contact for an instance without Surface simulation data.");
                continue;
            }

            const TSurfaceStateDescriptorResources* Descriptors = GPUResources->GetInstanceDescriptors(InstanceIndex);
            if (Descriptors == nullptr)
            {
                Diagnose("Rejected contact because the target instance has no GPU State resources.");
                continue;
            }

            const TSurfaceRuntimeData& RuntimeData = Assets.GetSurfaceData(SurfaceDataHandle);
            const TSharedSurfaceGeometryData& Geometry = *RuntimeData.GetSharedGeometry();
            const std::size_t TexelCount = Geometry.GetTexelCount();
            const std::size_t ScalarCount = TexelCount * ChannelCount;
            const glm::mat4 Model = Instance.GetTransform().GetMatrix();
            const std::vector<TSurfaceTexelGeometry>& Texels = Geometry.GetTexels();
            glm::vec3 InfluenceCenter = Contact.WorldPosition;
            if (Contact.bHasSimulationMapping)
            {
                const TMeshAsset& Mesh = Assets.GetMesh(Instance.GetMesh());
                const std::vector<TMeshTriangleSource>& Triangles = Mesh.GetTriangles();
                if (Contact.TargetTriangle >= Triangles.size() ||
                    !std::isfinite(Contact.SimulationUV.x) || !std::isfinite(Contact.SimulationUV.y) ||
                    Contact.SimulationUV.x < 0.0F || Contact.SimulationUV.x > 1.0F ||
                    Contact.SimulationUV.y < 0.0F || Contact.SimulationUV.y > 1.0F)
                {
                    Diagnose("Rejected contact because its hit triangle or Simulation UV is invalid.");
                    continue;
                }

                const TSurfaceLocalID TargetSurface = Triangles[Contact.TargetTriangle].Surface;
                if (TargetSurface >= Geometry.GetSurfaces().size())
                {
                    Diagnose("Rejected contact because its hit triangle has no mapped Surface.");
                    continue;
                }
                const TSurfaceTexelRange& Range = Geometry.GetSurface(TargetSurface);
                const std::uint32_t CenterX = std::min(static_cast<std::uint32_t>(Contact.SimulationUV.x *
                                                                                  Range.Resolution.Width),
                                                       Range.Resolution.Width - 1U);
                const std::uint32_t CenterY = std::min(static_cast<std::uint32_t>(Contact.SimulationUV.y *
                                                                                  Range.Resolution.Height),
                                                       Range.Resolution.Height - 1U);
                std::optional<TLocalTexelIndex> ResolvedCenter;
                const auto IsHitTriangleTexel = [&](std::uint32_t X, std::uint32_t Y) {
                    const std::size_t Index = static_cast<std::size_t>(Range.FirstTexel) +
                                              static_cast<std::size_t>(Y) * Range.Resolution.Width + X;
                    if (Index >= Texels.size())
                    {
                        return false;
                    }
                    const TSurfaceTexelGeometry& Texel = Texels[Index];
                    if (Texel.IsValid() && Texel.Surface == TargetSurface && Texel.Triangle == Contact.TargetTriangle)
                    {
                        ResolvedCenter = static_cast<TLocalTexelIndex>(Index);
                        return true;
                    }
                    return false;
                };

                if (!IsHitTriangleTexel(CenterX, CenterY))
                {
                    std::uint32_t BestDistanceSquared = std::numeric_limits<std::uint32_t>::max();
                    for (std::int32_t OffsetY = -2; OffsetY <= 2; ++OffsetY)
                    {
                        for (std::int32_t OffsetX = -2; OffsetX <= 2; ++OffsetX)
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
                            const std::optional<TLocalTexelIndex> Previous = ResolvedCenter;
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
                    Diagnose("Rejected contact because no valid texel for the hit triangle was found within the 2-texel fallback radius.");
                    continue;
                }
                InfluenceCenter = glm::vec3(Model * glm::vec4(Texels[*ResolvedCenter].Position, 1.0F));
            }
            if (InputDeltas[InstanceIndex].empty())
            {
                InputDeltas[InstanceIndex].assign(ScalarCount, 0.0F);
            }

            const std::vector<TSRProfileAssetHandle>& ProfileHandles = Assets.GetSurfaceProfileTable(SurfaceDataHandle);
            std::vector<float> InputFactors(ProfileHandles.size(), 0.0F);
            std::vector<bool>  bProfileSupportsState(ProfileHandles.size(), false);
            for (std::size_t ProfileIndex = 0; ProfileIndex < ProfileHandles.size(); ++ProfileIndex)
            {
                const TRegisteredSurfaceResponseProfileData Resolved =
                    Assets.GetSurfaceStateRegistry().ResolveProfile(Assets.GetSRProfile(ProfileHandles[ProfileIndex]).GetData());
                if (Contact.State < Resolved.States.size() && Resolved.States[Contact.State].has_value())
                {
                    InputFactors[ProfileIndex] = Resolved.States[Contact.State]->InputFactor;
                    bProfileSupportsState[ProfileIndex] = true;
                }
            }

            bool bAppliedToAnyTexel = false;
            bool bUnsupportedProfile = false;
            const std::size_t StateChannel = Contact.State;
            for (std::size_t TexelIndex = 0; TexelIndex < Texels.size(); ++TexelIndex)
            {
                const TSurfaceTexelGeometry& Texel = Texels[TexelIndex];
                if (!Texel.IsValid())
                {
                    continue;
                }

                const TSurfaceProfileIndex ProfileIndex = Geometry.GetProfileIndex(static_cast<TLocalTexelIndex>(TexelIndex));
                const glm::vec3 WorldTexelPosition = glm::vec3(Model * glm::vec4(Texel.Position, 1.0F));
                const float Distance = glm::length(WorldTexelPosition - InfluenceCenter);
                if (!std::isfinite(Distance) || Distance > Contact.Radius)
                {
                    continue;
                }

                if (ProfileIndex >= InputFactors.size() || !bProfileSupportsState[ProfileIndex])
                {
                    bUnsupportedProfile = true;
                    continue;
                }

                const float NormalizedDistance = std::clamp(Distance / Contact.Radius, 0.0F, 1.0F);
                const float LinearFalloff = 1.0F - NormalizedDistance;
                const float ContactWeight = Contact.Falloff == 0.0F ? 1.0F : std::pow(LinearFalloff, Contact.Falloff);
                const std::size_t ScalarIndex =
                    GetSurfaceGPUStateValueIndex(TexelIndex, StateChannel, ChannelCount);
                InputDeltas[InstanceIndex][ScalarIndex] += Contact.Strength * ContactWeight * InputFactors[ProfileIndex];
                bAppliedToAnyTexel = true;
            }

            if (bUnsupportedProfile)
            {
                Diagnose("Some contact texels do not support the requested State; those texels were skipped.");
            }
            if (!bAppliedToAnyTexel && !bUnsupportedProfile)
            {
                Diagnose("Contact did not overlap a valid texel; no State input was applied.");
            }
            bInstanceHasInput[InstanceIndex] = bInstanceHasInput[InstanceIndex] || bAppliedToAnyTexel;
        }

        const bool bAnyInput = std::any_of(bInstanceHasInput.begin(), bInstanceHasInput.end(), [](bool Value) {
            return Value;
        });
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
            const TGPUBuffer& InputDeltaBuffer = GPUResources->GetInstanceInputDeltaBuffer(InstanceIndex);
            const std::vector<float>& InputDelta = InputDeltas[InstanceIndex];
            const VkDeviceSize ByteSize = static_cast<VkDeviceSize>(InputDelta.size() * sizeof(float));
            if (ByteSize != InputDeltaBuffer.GetSize())
            {
                throw std::runtime_error("CPU InputDelta size does not match the instance GPU buffer size.");
            }
            InputDeltaBuffer.Upload(InputDelta.data(), ByteSize);
        }
        PendingContacts.clear();
    }

    void TSurfaceStateSystem::RecordStep(VkCommandBuffer CommandBuffer, float DeltaTime)
    {
        ApplyPendingContacts();
        if (!Solver)
        {
            return;
        }

        for (std::size_t SceneIndex = 0; SceneIndex < GPUResources->GetSceneInstanceCount(); ++SceneIndex)
        {
            const TSurfaceStateDescriptorResources* Descriptors = GPUResources->GetInstanceDescriptors(SceneIndex);
            if (Descriptors == nullptr)
            {
                continue;
            }

            const bool bCurrentStateAB = GPUResources->IsCurrentStateAB(SceneIndex);
            Solver->RecordStep(CommandBuffer,
                               *Descriptors,
                               bCurrentStateAB,
                               GPUResources->GetInstanceTexelCount(SceneIndex),
                               GPUResources->GetInstanceChannelCount(SceneIndex),
                               DeltaTime);
            GPUResources->AdvanceCurrentState(SceneIndex);
        }
    }

    const TSurfaceGPUResourceManager& TSurfaceStateSystem::GetGPUResources() const noexcept
    {
        return *GPUResources;
    }
} // namespace MDSS
