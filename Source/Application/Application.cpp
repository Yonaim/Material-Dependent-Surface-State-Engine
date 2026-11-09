/**
 * @file Application.cpp
 * @brief 응용 프로그램 초기화, 하위 시스템 구성과 메인 루프.
 */

#include "Application/Application.h"

#include "Application/BenchmarkOptions.h"
#include "Application/EngineConfig.h"
#include "Application/FrameCoordinator.h"
#include "Application/PerformanceSettings.h"
#include "DebugUI/Core/DebugUI.h"
#include "InputSystem/InputSystem.h"
#include "Logger/Logger.h"
#include "Rendering/Core/Renderer.h"
#include "Scene/SceneLoader.h"
#include "Scene/StaticMeshInstance.h"
#include "SurfaceState/Solver/SimulationClock.h"
#include "SurfaceState/SurfaceStateSystem.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <optional>
#include <stdexcept>
#include <utility>

namespace MDSS
{
// ============================================================
// 00. Lifecycle
// ============================================================
#pragma region 00_Lifecycle

    TApplication::TApplication(TBenchmarkOptions     BenchmarkOptions,
                               std::uint32_t         RenderTileSize,
                               bool                  bPerWorkgroupChannelMask,
                               std::filesystem::path ScenePath)
        : MainWindow(1280, 720, "MDSS Engine"), Context(MainWindow), Assets(Context), SurfaceData(Assets), MainScene(),
          Benchmark(std::move(BenchmarkOptions))
    {
        TLogger::Info("TApplication", "Initializing MDSS Engine.");

        const auto StartupScenePath =
            Benchmark.Enabled    ? std::filesystem::absolute(Benchmark.ScenePath).lexically_normal()
            : !ScenePath.empty() ? std::filesystem::absolute(ScenePath).lexically_normal()
                                 : LoadStartupScenePath(GetEngineConfigDirectory() / "Engine.ini");
        TLogger::Info("TApplication", "Startup Scene: " + StartupScenePath.string());
        MainScene =
            TSceneLoader::Load(StartupScenePath,
                               Assets,
                               SurfaceData,
                               Benchmark.Enabled ? std::optional<std::uint32_t>(Benchmark.Resolution) : std::nullopt);

        SurfaceData.ExchangeSurfaceStateRegistry(SurfaceData.BuildSurfaceStateRegistry(MainScene));
        SurfaceStateSystem =
            std::make_unique<SurfaceState::TSurfaceStateSystem>(Context, Assets, SurfaceData, MainScene);
        if (StartupScenePath.filename() == "ReactionTransitionComparison.Scene")
        {
            SurfaceStateSystem->SetSparseSolverEnabled(true);
            TLogger::Info("TApplication", "Sparse Solver enabled for comparison scene.");
        }
        FrameCoordinator = std::make_unique<TFrameCoordinator>(*SurfaceStateSystem);
        FrameRenderer = std::make_unique<Rendering::TRenderer>(
            Context, MainWindow, Assets, SurfaceData, MainScene, *SurfaceStateSystem, *FrameCoordinator);
        FrameRenderer->SetOverlayOccupancyTileSize(static_cast<Rendering::TOverlayOccupancyTileSize>(RenderTileSize));
        FrameRenderer->SetPerWorkgroupChannelMaskEnabled(bPerWorkgroupChannelMask);
        DebugInterface = std::make_unique<TDebugUI>(Context, MainWindow, *FrameRenderer, Assets, SurfaceData);
        if (Benchmark.Enabled)
        {
            if (!PerformanceSettings().value("replay_demo", false))
                MainScene.PauseDemoAnimation();
            DebugInterface->SetBenchmarkMode();
            FrameRenderer->SetStateDrivenGeometryUpdateEnabled(true);
            const auto& Settings = PerformanceSettings();
            DebugInterface->SetPresentationMode(Settings.value("presentation_mode", false));
            if (Settings.contains("adaptive_geometry"))
                SurfaceStateSystem->SetAdaptiveGeometryEnabled(Settings.at("adaptive_geometry").get<bool>());
            if (Settings.contains("compact_storage"))
                SurfaceStateSystem->SetCompactScratchStorageEnabled(Settings.at("compact_storage").get<bool>());
            if (Settings.contains("runtime_solver"))
                SurfaceStateSystem->SetRuntimeSolverFlagsEnabled(Settings.at("runtime_solver").get<bool>());
            if (Settings.contains("raw_flux_cache"))
                FrameRenderer->SetRawFluxCacheEnabled(Settings.at("raw_flux_cache").get<bool>());
            if (Settings.contains("coalesced_raw_flux"))
                FrameRenderer->SetCoalescedRawFluxLayoutEnabled(Settings.at("coalesced_raw_flux").get<bool>());
            if (Settings.contains("fp16_raw_flux"))
                FrameRenderer->SetHalfRawFluxCacheEnabled(Settings.at("fp16_raw_flux").get<bool>());
            if (Settings.contains("fp16_weights"))
                FrameRenderer->SetHalfDynamicWeightsEnabled(Settings.at("fp16_weights").get<bool>());
            if (Settings.contains("sparse_solver"))
                FrameRenderer->SetSparseSolverEnabled(Settings.at("sparse_solver").get<bool>());
            if (Settings.contains("sparse_height"))
                FrameRenderer->SetSparseAccumulationHeightEnabled(Settings.at("sparse_height").get<bool>());
            if (Settings.contains("sparse_geometry"))
                FrameRenderer->SetSparseSimulationGeometryEnabled(Settings.at("sparse_geometry").get<bool>());
            if (Settings.contains("active_channel_mask"))
                FrameRenderer->SetActiveChannelMaskEnabled(Settings.at("active_channel_mask").get<bool>());
            if (Settings.contains("per_workgroup_channel_mask"))
                FrameRenderer->SetPerWorkgroupChannelMaskEnabled(Settings.at("per_workgroup_channel_mask").get<bool>());
            if (Settings.contains("overlay_tile_culling"))
                FrameRenderer->SetOverlayTileCullingEnabled(Settings.at("overlay_tile_culling").get<bool>());
            if (Settings.contains("render_mesh_resolution"))
            {
                const auto Resolution = Settings.at("render_mesh_resolution").get<unsigned>();
                FrameRenderer->SetSurfaceTexelMeshResolution(Resolution);
                FrameRenderer->SetOverlayTexelMeshResolution(Resolution);
            }
            if (Settings.value("prewarm_mesh_cache", false))
                for (std::size_t Index = 0; Index < SurfaceStateSystem->GetGPUResources().GetSceneInstanceCount();
                     ++Index)
                    if (const auto* Geometry = SurfaceStateSystem->GetGPUResources().GetInstanceSharedGeometry(Index))
                        for (const auto& Preset : SurfaceState::SurfaceRenderMeshResolutionPresets)
                            (void)Geometry->GetTexelMeshVariant(Preset.Resolution);
            FrameRenderer->ConfigureBenchmarkCapture(
                Benchmark.OutputPath, Benchmark.WarmupFrames, Benchmark.MeasurementFrames);
        }
        InputInterface = std::make_unique<TInputSystem>(MainWindow.GetNativeHandle());
        TLogger::Info("TApplication", "Surface State System, renderer, scene, asset system, and Debug UI are ready.");
    }

    TApplication::~TApplication() = default;

#pragma endregion

// ============================================================
// 01. Main Loop
// ============================================================
#pragma region 01_Main_Loop

    void TApplication::Run(std::size_t FrameLimit)
    {
        if (FrameLimit == 0)
        {
            TLogger::Info("TApplication", "Entering main loop.");
        }
        else
        {
            TLogger::Info("TApplication", "Entering main loop for " + std::to_string(FrameLimit) + " frame(s).");
        }
        MainLoop(FrameLimit);
        TLogger::Info("TApplication", "Main loop finished.");
    }

    void TApplication::MainLoop(std::size_t FrameLimit)
    {
        std::size_t         RenderedFrameCount = 0;
        std::vector<double> BenchmarkCpuFrames;
        auto                PreviousFrameTime = std::chrono::steady_clock::now();
        while (!MainWindow.ShouldClose() && (FrameLimit == 0 || RenderedFrameCount < FrameLimit))
        {
            const auto FrameCpuStart = std::chrono::steady_clock::now();
            MainWindow.PollEvents();
            DebugInterface->BeginFrame(MainScene);
            if (const std::optional<SurfaceState::TSurfaceContactInput> Contact =
                    InputInterface->PollDebugContact(MainScene,
                                                     Assets,
                                                     SurfaceData,
                                                     DebugInterface->GetActiveViewportCamera(MainScene),
                                                     DebugInterface->IsInjectModeEnabled(),
                                                     DebugInterface->GetInjectState(),
                                                     DebugInterface->GetInjectStrength(),
                                                     DebugInterface->GetInjectRadius(),
                                                     DebugInterface->GetInjectFalloff(),
                                                     DebugInterface->GetInjectOperation(),
                                                     DebugInterface->GetInjectShape(),
                                                     DebugInterface->GetInjectSecondaryRadius(),
                                                     DebugInterface->GetInjectRotationDegrees(),
                                                     DebugInterface->GetInjectTexelSearchRadius(),
                                                     DebugInterface->ShouldSuppressDebugHotkey()))
            {
                SurfaceStateSystem->SubmitContact(*Contact);
                TLogger::Info("TInputSystem",
                              "Debug contact submitted for processing (state=" + std::to_string(Contact->State) +
                                  ", instance=" + std::to_string(Contact->TargetInstance) +
                                  ", strength=" + std::to_string(Contact->Strength) + ").");
            }
            const auto CurrentFrameTime = std::chrono::steady_clock::now();
            // Settings and file dialogs can block inside BeginFrame. Exclude
            // their elapsed time from the interactive simulation clock.
            const bool  bSuspendSimulationClock = DebugInterface->ConsumeFrameTimeResetRequest();
            const float DeltaTime = Benchmark.Enabled ? SurfaceState::FixedSimulationStepSeconds
                                    : bSuspendSimulationClock
                                        ? 0.0F
                                        : std::chrono::duration<float>(CurrentFrameTime - PreviousFrameTime).count();
            PreviousFrameTime = CurrentFrameTime;
            const float PlaybackDeltaTime = !Benchmark.Enabled && !bSuspendSimulationClock &&
                                                    MainScene.IsDemoAnimationPlaying() &&
                                                    MainScene.HasDemoContactEvents()
                                                ? SurfaceState::FixedSimulationStepSeconds
                                                : DeltaTime;
            const auto  Events =
                MainScene.AdvanceDemoAnimation(PlaybackDeltaTime * DebugInterface->GetAnimationTimeScale());
            for (const auto& Event : Events)
            {
                const auto& Instances = MainScene.GetStaticMeshInstances();
                const auto  Target =
                    std::find_if(Instances.begin(),
                                 Instances.end(),
                                 [&](const TStaticMeshInstance& Instance) { return Instance.GetId() == Event.Target; });
                if (Target == Instances.end())
                    throw std::runtime_error("Demo contact target disappeared from Scene: " + Event.Target);
                const glm::mat4                    Model = Target->GetTransform().GetMatrix();
                SurfaceState::TSurfaceContactInput Contact;
                Contact.TargetInstance =
                    static_cast<SurfaceState::TSurfaceInstanceID>(std::distance(Instances.begin(), Target));
                Contact.State = SurfaceData.GetSurfaceStateRegistry().GetStateId(Event.State);
                Contact.WorldPosition = glm::vec3(Model * glm::vec4(Event.LocalPosition, 1.0F));
                Contact.PlaneNormal =
                    glm::normalize(glm::transpose(glm::inverse(glm::mat3(Model))) * Event.LocalNormal);
                Contact.bPlanar = true;
                Contact.PlaneHalfDepth = Event.Depth;
                Contact.Operation = Event.Operation;
                Contact.Shape = Event.Shape;
                Contact.Radius = Event.Radius;
                Contact.SecondaryRadius = Event.SecondaryRadius;
                Contact.RotationDegrees = Event.RotationDegrees;
                Contact.Strength = Event.Strength;
                Contact.Falloff = Event.Falloff;
                SurfaceStateSystem->SubmitContact(Contact);
            }
            FrameRenderer->RenderFrame(MainScene, *DebugInterface, PlaybackDeltaTime, bSuspendSimulationClock);
            if (bSuspendSimulationClock)
            {
                // The setting's GPU work can outlive RenderFrame. Resume the
                // clock only after that work is complete.
                if (vkDeviceWaitIdle(Context.GetDevice()) != VK_SUCCESS)
                    throw std::runtime_error("Failed to wait for GPU after a setting change.");
                PreviousFrameTime = std::chrono::steady_clock::now();
            }
            if (Benchmark.Enabled)
                BenchmarkCpuFrames.push_back(
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - FrameCpuStart)
                        .count());
            ++RenderedFrameCount;
        }

        TLogger::Debug("TApplication", "Waiting for the Vulkan device to become idle before shutdown.");
        vkDeviceWaitIdle(Context.GetDevice());
        if (Benchmark.Enabled)
        {
            FrameRenderer->FinalizeBenchmarkCapture();
            std::ofstream(Benchmark.OutputPath.string() + ".cpu.json") << nlohmann::json(BenchmarkCpuFrames).dump();
        }
    }
#pragma endregion
} // namespace MDSS
