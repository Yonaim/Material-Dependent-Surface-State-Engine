/**
 * @file SurfaceStateSystem.cpp
 * @brief SurfaceStateSystem의 구현을 제공한다.
 */

#include "SurfaceState/SurfaceStateSystem.h"

#include "AssetManager/Assets/MeshAsset.h"
#include "AssetManager/Assets/SRProfileAsset.h"
#include "AssetManager/Core/AssetManager.h"
#include "GPU/Vulkan/VulkanContext.h"
#include "Logger/Logger.h"
#include "Scene/Scene.h"
#include "Scene/StaticMeshInstance.h"
#include "SurfaceState/GPU/SurfaceGPUResourceLayout.h"
#include "SurfaceState/Geometry/SharedSurfaceGeometryData.h"
#include "SurfaceState/Preprocessing/SurfaceDataManager.h"
#include "SurfaceState/Preprocessing/SurfaceRuntimeData.h"
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
#pragma region Lifecycle

    TSurfaceStateSystem::TSurfaceStateSystem(const GPU::TVulkanContext&  Context,
                                             const Asset::TAssetManager& Assets,
                                             const TSurfaceDataManager&  SurfaceData,
                                             const TScene&               Scene)
        : Context(Context), Assets(Assets), SurfaceData(SurfaceData), Scene(Scene),
          GPUResources(std::make_unique<TSurfaceGPUResourceManager>(Context, Assets, SurfaceData, Scene))
    {
        if (const TSurfaceStateDescriptorResources* Descriptors = GPUResources->GetAnyInstanceDescriptors())
        {
            Solver = std::make_unique<TSurfaceStateSolver>(Context.GetDevice(), Descriptors->GetLayout());
        }
        // Dynamic geometry buffers start empty.  The first feedback-enabled solver step performs one
        // force-full bootstrap before Pass 1, then normal steps use Solver -> Height -> Geometry -> Weights.
        bForceFullGeometryOnNextStep = true;
        bSeedPersistentActivityOnNextStep = true;
        PendingInputActivation.assign(GPUResources->GetSceneInstanceCount(), false);
        QueueInitialContacts();
    }

    void TSurfaceStateSystem::QueueInitialContacts()
    {
        const auto& Instances = Scene.GetStaticMeshInstances();
        for (const TSceneInitialContact& Initial : Scene.GetInitialContacts())
        {
            const auto Target =
                std::find_if(Instances.begin(),
                             Instances.end(),
                             [&](const TStaticMeshInstance& Instance) { return Instance.GetId() == Initial.Target; });
            if (Target == Instances.end())
                throw std::invalid_argument("Initial contact target is not in Scene: " + Initial.Target);
            TSurfaceContactInput Contact;
            Contact.TargetInstance = static_cast<TSurfaceInstanceID>(std::distance(Instances.begin(), Target));
            Contact.State = SurfaceData.GetSurfaceStateRegistry().GetStateId(Initial.State);
            Contact.WorldPosition = Initial.WorldPosition;
            Contact.Radius = Initial.Radius;
            Contact.Strength = Initial.Strength;
            Contact.Falloff = Initial.Falloff;
            SubmitContact(Contact);
        }
    }

    TSurfaceStateSystem::~TSurfaceStateSystem() = default;

    void TSurfaceStateSystem::ReplaceSceneResources(TSurfaceStateSystem&& Replacement)
    {
        if (&Context != &Replacement.Context || &Assets != &Replacement.Assets || &Scene != &Replacement.Scene)
        {
            throw std::invalid_argument("Replacement Surface State resources must use the same runtime owners.");
        }

        Solver = std::move(Replacement.Solver);
        GPUResources = std::move(Replacement.GPUResources);
        PendingContacts = std::move(Replacement.PendingContacts);
        PendingInputActivation = std::move(Replacement.PendingInputActivation);
        DebugSolverSettings = Replacement.DebugSolverSettings;
        bTransferWeightSettingsDirty = Replacement.bTransferWeightSettingsDirty;
        bForceFullGeometryOnNextStep = Replacement.bForceFullGeometryOnNextStep;
        bSeedPersistentActivityOnNextStep = Replacement.bSeedPersistentActivityOnNextStep;
        bStableDeltaTimeDirty = Replacement.bStableDeltaTimeDirty;
        CachedMaximumStableDeltaTime = Replacement.CachedMaximumStableDeltaTime;
        StableDeltaTimeModelMatrices = std::move(Replacement.StableDeltaTimeModelMatrices);
        RuntimeProfileOverrides = std::move(Replacement.RuntimeProfileOverrides);
    }
#pragma endregion

#pragma region State_and_Contact_Management

    void TSurfaceStateSystem::SubmitContact(TSurfaceContactInput Contact)
    {
        PendingContacts.push_back(std::move(Contact));
    }

    void TSurfaceStateSystem::ResetState()
    {
        if (vkQueueWaitIdle(Context.GetQueues().GetGraphics()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to wait for the graphics queue before resetting Surface State.");
        }
        PendingContacts.clear();
        GPUResources->ResetStates();
        // Resetting State invalidates the derived simulation geometry.  Rebuild it before the next
        // feedback-enabled solver step rather than tracking a separate revision/cache key.
        bForceFullGeometryOnNextStep = true;
        bSeedPersistentActivityOnNextStep = true;
        std::fill(PendingInputActivation.begin(), PendingInputActivation.end(), false);
    }

    void TSurfaceStateSystem::RestartState()
    {
        ResetState();
        QueueInitialContacts();
        bStableDeltaTimeDirty = true;
        bForceFullGeometryOnNextStep = true;
    }

    void TSurfaceStateSystem::SetDebugProfileParameters(Asset::TSRProfileAssetHandle   Profile,
                                                        TStateId                       State,
                                                        const TSurfaceStateParameters& Parameters,
                                                        bool                           bKeepRuntimeOverride)
    {
        const TSurfaceStateRegistry& Registry = SurfaceData.GetSurfaceStateRegistry();
        if (State >= Registry.GetStateCount())
        {
            throw std::out_of_range("Debug Profile override State is outside the Registry.");
        }

        TSurfaceResponseProfileData ValidationData;
        ValidationData.States.emplace(Registry.GetStateName(State), Parameters);
        ValidateSurfaceResponseProfileData(ValidationData);

        if (vkQueueWaitIdle(Context.GetQueues().GetGraphics()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to wait for the graphics queue before updating Profile parameters.");
        }
        if (!GPUResources->UpdateProfileParameters(Profile, State, Parameters))
        {
            throw std::invalid_argument("Debug Profile override does not belong to the current Scene.");
        }

        bStableDeltaTimeDirty = true;
        // Accumulation factors/capacity can change the State -> Height mapping.  A one-shot full
        // rebuild on the next solver step keeps derived data authoritative without a revision cache.
        bForceFullGeometryOnNextStep = true;
        const auto Key = std::make_pair(Profile, State);
        if (bKeepRuntimeOverride)
        {
            RuntimeProfileOverrides[Key] = Parameters;
        }
        else
        {
            RuntimeProfileOverrides.erase(Key);
        }
    }

#pragma endregion

#pragma region Pending_Contact_Application

    void TSurfaceStateSystem::ApplyPendingContacts()
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

        std::vector<std::vector<float>>        InputDeltas(InstanceCount);
        std::vector<std::vector<std::uint8_t>> InputGroupFlags(InstanceCount);
        std::vector<std::vector<std::uint32_t>> InputGroupMasks(InstanceCount);
        std::vector<std::uint32_t>              InputChannelMasks(InstanceCount, 0U);
        std::vector<bool>                       bInstanceHasInput(InstanceCount, false);

        for (const TSurfaceContactInput& Contact : PendingContacts)
        {
            bool bLoggedDiagnostic = false;
            auto Diagnose = [&bLoggedDiagnostic](const std::string& Message)
            {
                if (!bLoggedDiagnostic)
                {
                    TLogger::Warning("TSurfaceStateSystem", Message);
                    bLoggedDiagnostic = true;
                }
            };

            const std::size_t InstanceIndex = Contact.TargetInstance;
            if (InstanceIndex >= InstanceCount || Contact.State >= ChannelCount ||
                !std::isfinite(Contact.WorldPosition.x) || !std::isfinite(Contact.WorldPosition.y) ||
                !std::isfinite(Contact.WorldPosition.z) || !std::isfinite(Contact.Radius) || Contact.Radius <= 0.0F ||
                !std::isfinite(Contact.Strength) || Contact.Strength < 0.0F || !std::isfinite(Contact.Falloff) ||
                Contact.Falloff < 0.0F)
            {
                Diagnose("Rejected contact with an invalid instance, State, radius, strength, or falloff.");
                continue;
            }

            const TStaticMeshInstance&      Instance = Scene.GetStaticMeshInstances()[InstanceIndex];
            const TSurfaceRuntimeDataHandle SurfaceDataHandle = Instance.GetSurfaceData();
            if (!SurfaceData.HasSurfaceData(SurfaceDataHandle))
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

            const TSurfaceRuntimeData&                RuntimeData = SurfaceData.GetSurfaceData(SurfaceDataHandle);
            const TSharedSurfaceGeometryData&         Geometry = *RuntimeData.GetSharedGeometry();
            const std::size_t                         TexelCount = Geometry.GetTexelCount();
            const std::size_t                         ScalarCount = TexelCount * ChannelCount;
            const glm::mat4                           Model = Instance.GetTransform().GetMatrix();
            const std::vector<TSurfaceTexelGeometry>& Texels = Geometry.GetTexels();
            glm::vec3                                 InfluenceCenter = Contact.WorldPosition;
            if (Contact.bHasSimulationMapping)
            {
                const Asset::TMeshAsset&                       Mesh = Assets.GetMesh(Instance.GetMesh());
                const std::vector<Asset::TMeshTriangleSource>& Triangles = Mesh.GetTriangles();
                if (Contact.TargetTriangle >= Triangles.size() || !std::isfinite(Contact.SimulationUV.x) ||
                    !std::isfinite(Contact.SimulationUV.y) || Contact.SimulationUV.x < 0.0F ||
                    Contact.SimulationUV.x > 1.0F || Contact.SimulationUV.y < 0.0F || Contact.SimulationUV.y > 1.0F)
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
                const std::uint32_t       CenterX =
                    std::min(static_cast<std::uint32_t>(Contact.SimulationUV.x * Range.Resolution.Width),
                             Range.Resolution.Width - 1U);
                const std::uint32_t CenterY =
                    std::min(static_cast<std::uint32_t>(Contact.SimulationUV.y * Range.Resolution.Height),
                             Range.Resolution.Height - 1U);
                const std::uint32_t MaxSearchRadius = std::max(Range.Resolution.Width, Range.Resolution.Height);
                const std::uint32_t EffectiveSearchRadius = std::min(Contact.TexelSearchRadius, MaxSearchRadius);
                const std::int32_t  SearchRadius = static_cast<std::int32_t>(EffectiveSearchRadius);
                std::optional<TLocalTexelIndex> ResolvedCenter;
                const auto                      IsHitTriangleTexel = [&](std::uint32_t X, std::uint32_t Y)
                {
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
                            const TSurfaceTexelGeometry& Texel = Texels[Index];
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

            const std::vector<Asset::TSRProfileAssetHandle>& ProfileHandles =
                SurfaceData.GetSurfaceProfileTable(SurfaceDataHandle);
            std::vector<float> InputFactors(ProfileHandles.size(), 0.0F);
            std::vector<bool>  bProfileSupportsState(ProfileHandles.size(), false);
            for (std::size_t ProfileIndex = 0; ProfileIndex < ProfileHandles.size(); ++ProfileIndex)
            {
                const TRegisteredSurfaceResponseProfileData Resolved =
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
            for (std::size_t TexelIndex = 0; TexelIndex < Texels.size(); ++TexelIndex)
            {
                const TSurfaceTexelGeometry& Texel = Texels[TexelIndex];
                if (!Texel.IsValid())
                {
                    continue;
                }

                const TSurfaceProfileIndex ProfileIndex =
                    Geometry.GetProfileIndex(static_cast<TLocalTexelIndex>(TexelIndex));
                const glm::vec3 WorldTexelPosition = glm::vec3(Model * glm::vec4(Texel.Position, 1.0F));
                const float     Distance = glm::length(WorldTexelPosition - InfluenceCenter);
                if (!std::isfinite(Distance) || Distance > Contact.Radius)
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

                const float NormalizedDistance = std::clamp(Distance / Contact.Radius, 0.0F, 1.0F);
                const float LinearFalloff = 1.0F - NormalizedDistance;
                const float ContactWeight = Contact.Falloff == 0.0F ? 1.0F : std::pow(LinearFalloff, Contact.Falloff);
                const std::size_t ScalarIndex = GetSurfaceGPUStateValueIndex(TexelIndex, StateChannel, ChannelCount);
                const float       AreaScale = GetSurfaceWorldTexelArea(Texel, Model) / SurfaceStateReferenceArea;
                const float Delta = Contact.Strength * ContactWeight * InputFactors[ProfileIndex] * AreaScale;
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
                TLogger::Info("TSurfaceStateSystem",
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
            std::any_of(bInstanceHasInput.begin(), bInstanceHasInput.end(), [](bool Value) { return Value; });
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
            const GPU::TGPUBuffer&    InputDeltaBuffer = GPUResources->GetInstanceInputDeltaBuffer(InstanceIndex);
            const std::vector<float>& InputDelta = InputDeltas[InstanceIndex];
            const VkDeviceSize        ByteSize = static_cast<VkDeviceSize>(InputDelta.size() * sizeof(float));
            if (ByteSize != InputDeltaBuffer.GetSize())
            {
                throw std::runtime_error("CPU InputDelta size does not match the instance GPU buffer size.");
            }
            InputDeltaBuffer.Upload(InputDelta.data(), ByteSize);

            // Persistent sparse solver does not rescan all texels. CPU contact distribution already knows
            // exactly which workgroups/channels received InputDelta, so upload that compact activation list.
            const std::size_t TexelCount = GPUResources->GetInstanceTexelCount(InstanceIndex);
            const TSurfaceSparseMetadataLayout SparseLayout = GetSurfaceSparseMetadataLayout(TexelCount);
            std::vector<std::uint32_t> InputGroups;
            std::vector<std::uint32_t> CompactInputGroupMasks;
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
                const GPU::TGPUBuffer& AccumulationBuffer =
                    GPUResources->GetInstanceAccumulationHeightBuffer(InstanceIndex);
                const std::uint32_t GroupCount = static_cast<std::uint32_t>(InputGroups.size());
                const std::uint32_t ChannelMask = InputChannelMasks[InstanceIndex];
                AccumulationBuffer.Upload(&GroupCount, sizeof(GroupCount),
                    static_cast<VkDeviceSize>(SparseLayout.InputCountWord * sizeof(std::uint32_t)));
                AccumulationBuffer.Upload(&ChannelMask, sizeof(ChannelMask),
                    static_cast<VkDeviceSize>(SparseLayout.InputMaskWord * sizeof(std::uint32_t)));
                AccumulationBuffer.Upload(InputGroups.data(),
                    static_cast<VkDeviceSize>(InputGroups.size() * sizeof(std::uint32_t)),
                    static_cast<VkDeviceSize>(SparseLayout.InputListWord * sizeof(std::uint32_t)));
                AccumulationBuffer.Upload(CompactInputGroupMasks.data(),
                    static_cast<VkDeviceSize>(CompactInputGroupMasks.size() * sizeof(std::uint32_t)),
                    static_cast<VkDeviceSize>(SparseLayout.InputGroupMaskWord * sizeof(std::uint32_t)));
                if (InstanceIndex >= PendingInputActivation.size())
                    PendingInputActivation.resize(InstanceCount, false);
                PendingInputActivation[InstanceIndex] = true;
            }
        }
        PendingContacts.clear();
    }

#pragma endregion

#pragma region Timestep_and_Transfer_Caches

    float TSurfaceStateSystem::GetMaximumStableDeltaTime()
    {
        const auto& Instances = Scene.GetStaticMeshInstances();
        if (StableDeltaTimeModelMatrices.size() != Instances.size())
            bStableDeltaTimeDirty = true;
        for (std::size_t I = 0; I < Instances.size(); ++I)
        {
            if (!GPUResources->GetInstanceDescriptors(I) || I >= StableDeltaTimeModelMatrices.size())
                continue;
            const glm::mat3 Model(Instances[I].GetTransform().GetMatrix());
            for (glm::length_t Column = 0; Column < 3; ++Column)
                for (glm::length_t Row = 0; Row < 3; ++Row)
                    if (StableDeltaTimeModelMatrices[I][Column][Row] != Model[Column][Row])
                        bStableDeltaTimeDirty = true;
        }
        if (!bStableDeltaTimeDirty)
            return CachedMaximumStableDeltaTime;

        double     Limit = 1.0 / 60.0;
        const bool SatEnabled = DebugSolverSettings.IsEnabled(TSurfaceSolverTerm::SaturationDrive);
        const bool GeoEnabled = DebugSolverSettings.IsEnabled(TSurfaceSolverTerm::GeometryDrive);
        for (std::size_t I = 0; I < Instances.size(); ++I)
        {
            if (!GPUResources->GetInstanceDescriptors(I))
                continue;
            const auto      Handle = Instances[I].GetSurfaceData();
            const auto&     Geometry = *SurfaceData.GetSurfaceData(Handle).GetSharedGeometry();
            const auto&     Texels = Geometry.GetTexels();
            const auto&     Profiles = SurfaceData.GetSurfaceProfileTable(Handle);
            const glm::mat4 Model = Instances[I].GetTransform().GetMatrix();
            const glm::mat3 Linear(Model);
            const auto      Weights = BuildSurfaceGPUTransferWeights(
                Geometry,
                Model,
                nullptr,
                DebugSolverSettings.IsEnabled(TSurfaceSolverTerm::NormalWeight),
                DebugSolverSettings.IsEnabled(TSurfaceSolverTerm::DistanceWeight),
                DebugSolverSettings.IsEnabled(TSurfaceSolverTerm::ProfileBoundaryWeight));
            for (std::size_t T = 0; T < Texels.size(); ++T)
            {
                const auto Profile = Geometry.GetProfileIndex(static_cast<TLocalTexelIndex>(T));
                if (!Texels[T].IsValid() || Profile >= Profiles.size())
                    continue;
                const double AreaScale = GetSurfaceWorldTexelArea(Texels[T], Model) / SurfaceStateReferenceArea;
                if (AreaScale <= 0.0)
                    continue;
                double          WeightSum = 0.0, HeightWeightSum = 0.0;
                const glm::vec3 Position = Texels[T].Position + Texels[T].Normal * Texels[T].Geometry.MesoVirtualHeight;
                for (std::size_t D = 0; D < SurfaceNeighborCount; ++D)
                {
                    const auto N = Texels[T].NeighborIndices[D];
                    if (N >= Texels.size())
                        continue;
                    const double W = Weights[T * SurfaceNeighborCount + D];
                    WeightSum += W;
                    const glm::vec3 Target =
                        Texels[N].Position + Texels[N].Normal * Texels[N].Geometry.MesoVirtualHeight;
                    // DirectionDrive <= 1; use an upper bound including both gravity directions.
                    HeightWeightSum += W * std::abs((Linear * (Target - Position)).z);
                }
                for (const auto& [Name, Original] : Assets.GetSRProfile(Profiles[Profile]).GetData().States)
                {
                    const TStateId State = SurfaceData.GetSurfaceStateRegistry().GetStateId(Name);
                    const auto     Override = RuntimeProfileOverrides.find({Profiles[Profile], State});
                    const auto&    P = Override == RuntimeProfileOverrides.end() ? Original : Override->second;
                    // SaturationDrive <= source saturation; Geometry uses the same unclamped mobility.
                    const double RateBound =
                        (SatEnabled ? BaseSaturationTransferRate * P.SaturationTransferFactor * WeightSum : 0.0) +
                        (GeoEnabled ? BaseGeometryTransferRate * P.GeometryTransferFactor * HeightWeightSum : 0.0);
                    if (RateBound > 0.0)
                        Limit = std::min(Limit, 0.9 * P.StateCapacity * AreaScale / RateBound);
                }
            }
        }
        CachedMaximumStableDeltaTime = static_cast<float>(Limit);
        StableDeltaTimeModelMatrices.resize(Instances.size());
        for (std::size_t I = 0; I < Instances.size(); ++I)
            StableDeltaTimeModelMatrices[I] = glm::mat3(Instances[I].GetTransform().GetMatrix());
        bStableDeltaTimeDirty = false;
        return CachedMaximumStableDeltaTime;
    }

    void TSurfaceStateSystem::PrepareTransferWeightCachesForSettingChange()
    {
        if (!bTransferWeightSettingsDirty)
            return;
        if (vkQueueWaitIdle(Context.GetQueues().GetGraphics()) != VK_SUCCESS)
            throw std::runtime_error("Failed to wait for the graphics queue before preparing TransferWeight caches.");
        for (std::size_t SceneIndex = 0; SceneIndex < GPUResources->GetSceneInstanceCount(); ++SceneIndex)
        {
            if (GPUResources->GetInstanceDescriptors(SceneIndex) == nullptr)
                continue;
            GPUResources->UpdateTransferWeightCache(
                SceneIndex,
                Scene.GetStaticMeshInstances()[SceneIndex].GetTransform(),
                DebugSolverSettings.IsEnabled(TSurfaceSolverTerm::NormalWeight),
                DebugSolverSettings.IsEnabled(TSurfaceSolverTerm::DistanceWeight),
                DebugSolverSettings.IsEnabled(TSurfaceSolverTerm::ProfileBoundaryWeight),
                bHalfDynamicWeightsEnabled);
        }
        bTransferWeightSettingsDirty = false;
        bForceFullGeometryOnNextStep = true;
    }

#pragma endregion

#pragma region GPU_Recording

    void TSurfaceStateSystem::RecordStep(VkCommandBuffer CommandBuffer,
                                         float           DeltaTime,
                                         VkQueryPool     TimestampQueryPool,
                                         std::uint32_t   FirstStepQuery,
                                         bool            bPrepareRenderHeight)
    {
        ApplyPendingContacts();
        if (!Solver)
        {
            return;
        }

        bool          bHasDirtyTransferWeightCache = bTransferWeightSettingsDirty;
        for (std::size_t SceneIndex = 0; SceneIndex < GPUResources->GetSceneInstanceCount(); ++SceneIndex)
        {
            if (GPUResources->GetInstanceDescriptors(SceneIndex) == nullptr)
            {
                continue;
            }
            bHasDirtyTransferWeightCache = bHasDirtyTransferWeightCache ||
                                           GPUResources->NeedsTransferWeightCacheUpdate(
                                               SceneIndex, Scene.GetStaticMeshInstances()[SceneIndex].GetTransform());
        }
        if (bHasDirtyTransferWeightCache && vkQueueWaitIdle(Context.GetQueues().GetGraphics()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to wait for the graphics queue before updating TransferWeight caches.");
        }

        std::vector<TSurfaceSolverInstanceStep> InstanceSteps;
        std::vector<std::size_t>                ActiveSceneIndices;
        InstanceSteps.reserve(GPUResources->GetSceneInstanceCount());
        ActiveSceneIndices.reserve(GPUResources->GetSceneInstanceCount());
        for (std::size_t SceneIndex = 0; SceneIndex < GPUResources->GetSceneInstanceCount(); ++SceneIndex)
        {
            const TSurfaceStateDescriptorResources* Descriptors = GPUResources->GetInstanceDescriptors(SceneIndex);
            if (Descriptors == nullptr)
            {
                continue;
            }

            const bool                 bCurrentStateAB = GPUResources->IsCurrentStateAB(SceneIndex);
            const TStaticMeshInstance& Instance = Scene.GetStaticMeshInstances()[SceneIndex];
            const glm::mat4            ModelMatrix = Instance.GetTransform().GetMatrix();
            const bool                 bRebuildStaticWeights =
                bTransferWeightSettingsDirty ||
                GPUResources->NeedsTransferWeightCacheUpdate(SceneIndex, Instance.GetTransform());
            if (bRebuildStaticWeights)
            {
                GPUResources->UpdateTransferWeightCache(
                    SceneIndex,
                    Instance.GetTransform(),
                    DebugSolverSettings.IsEnabled(TSurfaceSolverTerm::NormalWeight),
                    DebugSolverSettings.IsEnabled(TSurfaceSolverTerm::DistanceWeight),
                    DebugSolverSettings.IsEnabled(TSurfaceSolverTerm::ProfileBoundaryWeight),
                    bHalfDynamicWeightsEnabled);
            }
            const glm::vec3 GravityWorld(0.0F, 0.0F, -1.0F);
            std::uint32_t   SolverFlags = 0U;
            if (!DebugSolverSettings.IsEnabled(TSurfaceSolverTerm::GeometryDrive))
            {
                SolverFlags |= 1U << 0U;
            }
            if (!DebugSolverSettings.IsEnabled(TSurfaceSolverTerm::SaturationDrive))
            {
                SolverFlags |= 1U << 1U;
            }
            if (!DebugSolverSettings.IsEnabled(TSurfaceSolverTerm::Decay))
            {
                SolverFlags |= 1U << 2U;
            }
            if (!DebugSolverSettings.IsEnabled(TSurfaceSolverTerm::ConcavityRetention))
            {
                SolverFlags |= 1U << 3U;
            }
            if (!DebugSolverSettings.IsEnabled(TSurfaceSolverTerm::MesoDirectionNormal))
            {
                SolverFlags |= 1U << 4U;
            }
            if (bRawFluxCacheEnabled)
            {
                SolverFlags |= SurfaceSolverRawFluxCacheFlag;
                if (bCoalescedRawFluxLayoutEnabled)
                    SolverFlags |= SurfaceSolverCoalescedRawFluxLayoutFlag;
                if (bHalfRawFluxCacheEnabled)
                    SolverFlags |= SurfaceSolverHalfRawFluxCacheFlag;
            }
            if (bHalfDynamicWeightsEnabled)
                SolverFlags |= SurfaceSolverHalfDynamicWeightsFlag;
            if (bSeedPersistentActivityOnNextStep &&
                (bSparseSolverEnabled || bActiveChannelMaskEnabled || bPerWorkgroupChannelMaskEnabled))
                SolverFlags |= SurfaceSolverSeedPersistentActivityFlag;
            if (bSparseSolverEnabled)
                SolverFlags |= SurfaceSolverSparseSolverFlag;
            if (bSparseAccumulationHeightEnabled)
                SolverFlags |= SurfaceSolverSparseAccumulationHeightFlag;
            if (bActiveChannelMaskEnabled)
                SolverFlags |= SurfaceSolverActiveChannelMaskFlag;
            if (bPerWorkgroupChannelMaskEnabled)
                SolverFlags |= SurfaceSolverPerWorkgroupChannelMaskFlag;
            if (bSparseSimulationGeometryEnabled)
                SolverFlags |= SurfaceSolverSparseGeometryFlag;

            // Height validity is independent of simulation feedback. If feedback is OFF the full rebuild
            // is deferred until the final render-height step instead of adding revision/cache bookkeeping.
            if (bRebuildStaticWeights || bForceFullGeometryOnNextStep)
                SolverFlags |= SurfaceSolverForceFullGeometryFlag;

            if (DebugSolverSettings.bAccumulationGeometryUpdateEnabled)
            {
                SolverFlags |= SurfaceSolverAccumulationGeometryUpdateFlag;
                if (DebugSolverSettings.IsEnabled(TSurfaceSolverTerm::DistanceWeight))
                    SolverFlags |= SurfaceSolverDistanceWeightFlag;
                if (DebugSolverSettings.IsEnabled(TSurfaceSolverTerm::NormalWeight))
                    SolverFlags |= SurfaceSolverNormalWeightFlag;
                if (DebugSolverSettings.IsEnabled(TSurfaceSolverTerm::ProfileBoundaryWeight))
                    SolverFlags |= SurfaceSolverProfileBoundaryWeightFlag;
            }
            TSurfaceSolverInstanceStep Step;
            Step.Descriptors = Descriptors;
            Step.bCurrentStateAB = bCurrentStateAB;
            Step.TexelCount = GPUResources->GetInstanceTexelCount(SceneIndex);
            Step.ChannelCount = GPUResources->GetInstanceChannelCount(SceneIndex);
            Step.DeltaTime = DeltaTime;
            Step.ModelMatrix = ModelMatrix;
            Step.GravityWorld = GravityWorld;
            Step.SolverFlags = SolverFlags;
            Step.bPrepareAccumulationHeight = bPrepareRenderHeight;
            Step.bHasInputActivation = SceneIndex < PendingInputActivation.size() && PendingInputActivation[SceneIndex];
            InstanceSteps.push_back(Step);
            ActiveSceneIndices.push_back(SceneIndex);
        }
        Solver->RecordSteps(CommandBuffer, InstanceSteps, TimestampQueryPool, FirstStepQuery);
        for (const std::size_t SceneIndex : ActiveSceneIndices)
        {
            GPUResources->AdvanceCurrentState(SceneIndex);
            if (SceneIndex < PendingInputActivation.size())
                PendingInputActivation[SceneIndex] = false;
        }
        if (!InstanceSteps.empty())
            bSeedPersistentActivityOnNextStep = false;
        bTransferWeightSettingsDirty = false;
        // Feedback ON always derives Height after the step. Feedback OFF may execute several fixed substeps;
        // keep the invalidation alive until the final substep actually prepares render height.
        if (DebugSolverSettings.bAccumulationGeometryUpdateEnabled || bPrepareRenderHeight)
            bForceFullGeometryOnNextStep = false;
    }


#pragma endregion

#pragma region Debug_and_Accessors

    std::size_t TSurfaceStateSystem::GetSolverInstanceCount() const noexcept
    {
        if (!GPUResources)
            return 0U;
        std::size_t InstanceCount = 0U;
        for (std::size_t SceneIndex = 0; SceneIndex < GPUResources->GetSceneInstanceCount(); ++SceneIndex)
        {
            InstanceCount += GPUResources->GetInstanceDescriptors(SceneIndex) != nullptr ? 1U : 0U;
        }
        return InstanceCount;
    }

    void TSurfaceStateSystem::SetHalfDynamicWeightsEnabled(bool bEnabled) noexcept
    {
        if (bHalfDynamicWeightsEnabled == bEnabled)
            return;
        bHalfDynamicWeightsEnabled = bEnabled;
        // Static weights must be re-uploaded in the selected representation; dynamic feedback then rebuilds from it.
        bTransferWeightSettingsDirty = true;
        bForceFullGeometryOnNextStep = true;
        bStableDeltaTimeDirty = true;
    }

    void TSurfaceStateSystem::SetSparseSolverEnabled(bool bEnabled) noexcept
    {
        if (bSparseSolverEnabled == bEnabled)
            return;
        bSparseSolverEnabled = bEnabled;
        if (bEnabled)
            bSeedPersistentActivityOnNextStep = true;
    }

    void TSurfaceStateSystem::SetActiveChannelMaskEnabled(bool bEnabled) noexcept
    {
        if (bActiveChannelMaskEnabled == bEnabled)
            return;
        bActiveChannelMaskEnabled = bEnabled;
        if (bEnabled)
            bSeedPersistentActivityOnNextStep = true;
    }

    void TSurfaceStateSystem::SetPerWorkgroupChannelMaskEnabled(bool bEnabled) noexcept
    {
        if (bPerWorkgroupChannelMaskEnabled == bEnabled)
            return;
        bPerWorkgroupChannelMaskEnabled = bEnabled;
        if (bEnabled)
            bSeedPersistentActivityOnNextStep = true;
    }

    const TSurfaceSolverDebugSettings& TSurfaceStateSystem::GetDebugSolverSettings() const noexcept
    {
        return DebugSolverSettings;
    }

    void TSurfaceStateSystem::SetAccumulationGeometryUpdateEnabled(bool bEnabled) noexcept
    {
        if (DebugSolverSettings.bAccumulationGeometryUpdateEnabled != bEnabled)
            bTransferWeightSettingsDirty = true;
        DebugSolverSettings.bAccumulationGeometryUpdateEnabled = bEnabled;
    }

    void TSurfaceStateSystem::SetDebugSolverTermEnabled(TSurfaceSolverTerm Term, bool bEnabled) noexcept
    {
        if (DebugSolverSettings.IsEnabled(Term) == bEnabled)
        {
            return;
        }
        DebugSolverSettings.SetEnabled(Term, bEnabled);
        bStableDeltaTimeDirty = true;
        if (Term == TSurfaceSolverTerm::DistanceWeight || Term == TSurfaceSolverTerm::NormalWeight ||
            Term == TSurfaceSolverTerm::ProfileBoundaryWeight)
        {
            bTransferWeightSettingsDirty = true;
        }
    }

    void TSurfaceStateSystem::SetDebugGeometryDriveEnabled(bool bEnabled) noexcept
    {
        SetDebugSolverTermEnabled(TSurfaceSolverTerm::GeometryDrive, bEnabled);
    }

    void TSurfaceStateSystem::SetDebugNormalWeightEnabled(bool bEnabled) noexcept
    {
        SetDebugSolverTermEnabled(TSurfaceSolverTerm::NormalWeight, bEnabled);
    }

    const TSurfaceGPUResourceManager& TSurfaceStateSystem::GetGPUResources() const noexcept
    {
        return *GPUResources;
    }
#pragma endregion
} // namespace MDSS::SurfaceState
