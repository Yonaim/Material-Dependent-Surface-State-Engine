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
    // 객체 초기화와 종료에서 소유 자원·참조의 유효 기간을 맞춘다.

    FApplication::FApplication(FBenchmarkOptions     BenchmarkOptions,
                               std::uint32_t         RenderTileSize,
                               bool                  bPerWorkgroupChannelMask,
                               std::filesystem::path ScenePath)
        : MainWindow(1280, 720, "MDSS Engine"), Context(MainWindow), Assets(Context), SurfaceData(Assets), MainScene(),
          Benchmark(std::move(BenchmarkOptions))
    {
        FLogger::Info("TApplication", "Initializing MDSS Engine.");

        const auto StartupScenePath = ResolveStartupScenePath(ScenePath);
        LoadStartupScene(StartupScenePath);
        InitializeSurfaceStateSystem(StartupScenePath);
        InitializeFrameServices(RenderTileSize, bPerWorkgroupChannelMask);
        ConfigureBenchmarkIfEnabled();
        InitializeInput();

        FLogger::Info("TApplication", "Surface State System, renderer, scene, asset system, and Debug UI are ready.");
    }

    FApplication::~FApplication() = default;

    // 00.00. Startup and Dependencies
    #pragma region 00_00_Startup_and_Dependencies
        // 시작 Scene을 결정한 뒤 의존성 순서에 따라 Simulation·Rendering·UI를 구성한다.

        std::filesystem::path FApplication::ResolveStartupScenePath(const std::filesystem::path &ScenePath) const
        {
            if (Benchmark.bEnabled)
            {
                return std::filesystem::absolute(Benchmark.ScenePath).lexically_normal();
            }

            if (!ScenePath.empty())
            {
                return std::filesystem::absolute(ScenePath).lexically_normal();
            }

            return LoadStartupScenePath(GetEngineConfigDirectory() / "Engine.ini");
        }

        void FApplication::LoadStartupScene(const std::filesystem::path &StartupScenePath)
        {
            FLogger::Info("TApplication", "Startup Scene: " + StartupScenePath.string());

            MainScene =
                FSceneLoader::Load(StartupScenePath,
                                   Assets,
                                   SurfaceData,
                                   Benchmark.bEnabled ? std::optional<std::uint32_t>(Benchmark.Resolution) : std::nullopt);
        }

        void FApplication::InitializeSurfaceStateSystem(const std::filesystem::path &StartupScenePath)
        {
            SurfaceData.ExchangeSurfaceStateRegistry(SurfaceData.BuildSurfaceStateRegistry(MainScene));
            SurfaceStateSystem =
                std::make_unique<SurfaceState::FSurfaceStateSystem>(Context, Assets, SurfaceData, MainScene);

            if (StartupScenePath.filename() == "ReactionTransitionComparison.Scene")
            {
                SurfaceStateSystem->SetSparseSolverEnabled(true);
                FLogger::Info("TApplication", "Sparse Solver enabled for comparison scene.");
            }
        }

        void FApplication::InitializeFrameServices(std::uint32_t RenderTileSize, bool bPerWorkgroupChannelMask)
        {
            FrameCoordinator = std::make_unique<FFrameCoordinator>(*SurfaceStateSystem);
            FrameRenderer = std::make_unique<Rendering::FRenderer>(Context,
                                                                   MainWindow,
                                                                   Assets,
                                                                   SurfaceData,
                                                                   MainScene,
                                                                   *SurfaceStateSystem,
                                                                   *FrameCoordinator);

            FrameRenderer->SetOverlayOccupancyTileSize(static_cast<Rendering::EOverlayOccupancyTileSize>(RenderTileSize));
            FrameRenderer->SetPerWorkgroupChannelMaskEnabled(bPerWorkgroupChannelMask);

            DebugInterface = std::make_unique<FDebugUI>(Context, MainWindow, *FrameRenderer, Assets, SurfaceData);
        }

        void FApplication::InitializeInput()
        {
            InputInterface = std::make_unique<FInputSystem>(MainWindow.GetNativeHandle());
        }

    #pragma endregion

    // 00.01. Benchmark Configuration
    #pragma region 00_01_Benchmark_Configuration
        // Bench 모드에만 필요한 UI·Solver 옵션과 리소스 예열을 초기화한다.

        void FApplication::ConfigureBenchmarkIfEnabled()
        {
            if (!Benchmark.bEnabled)
            {
                return;
            }

            ConfigureBenchmarkPlayback();
            ApplyBenchmarkPerformanceSettings();
            PrewarmBenchmarkMeshCache();

            FrameRenderer->ConfigureBenchmarkCapture(Benchmark.OutputPath,
                                                     Benchmark.WarmupFrames,
                                                     Benchmark.MeasurementFrames,
                                                     &MainScene);
        }

        void FApplication::ConfigureBenchmarkPlayback()
        {
            if (!PerformanceSettings().value("replay_demo", false))
            {
                MainScene.PauseDemoAnimation();
            }

            DebugInterface->SetBenchmarkMode();
            FrameRenderer->SetStateDrivenGeometryUpdateEnabled(PerformanceSettings().value("geometry_feedback", true));
            DebugInterface->SetPresentationMode(PerformanceSettings().value("presentation_mode", false));
        }

        void FApplication::ApplyBenchmarkPerformanceSettings()
        {
            const auto &Settings = PerformanceSettings();

            // Simulation configuration
            if (Settings.contains("adaptive_geometry"))
            {
                SurfaceStateSystem->SetAdaptiveGeometryEnabled(Settings.at("adaptive_geometry").get<bool>());
            }
            if (Settings.contains("compact_storage"))
            {
                SurfaceStateSystem->SetCompactScratchStorageEnabled(Settings.at("compact_storage").get<bool>());
            }
            if (Settings.contains("runtime_solver"))
            {
                SurfaceStateSystem->SetRuntimeSolverFlagsEnabled(Settings.at("runtime_solver").get<bool>());
            }

            // Flux cache and precision
            if (Settings.contains("raw_flux_cache"))
            {
                FrameRenderer->SetRawFluxCacheEnabled(Settings.at("raw_flux_cache").get<bool>());
            }
            if (Settings.contains("coalesced_raw_flux"))
            {
                FrameRenderer->SetCoalescedRawFluxLayoutEnabled(Settings.at("coalesced_raw_flux").get<bool>());
            }
            if (Settings.contains("fp16_raw_flux"))
            {
                FrameRenderer->SetHalfRawFluxCacheEnabled(Settings.at("fp16_raw_flux").get<bool>());
            }
            if (Settings.contains("fp16_weights"))
            {
                FrameRenderer->SetHalfDynamicWeightsEnabled(Settings.at("fp16_weights").get<bool>());
            }

            // Sparse scheduling and output layout
            if (Settings.contains("sparse_solver"))
            {
                FrameRenderer->SetSparseSolverEnabled(Settings.at("sparse_solver").get<bool>());
            }
            if (Settings.contains("sparse_height"))
            {
                FrameRenderer->SetSparseAccumulationHeightEnabled(Settings.at("sparse_height").get<bool>());
            }
            if (Settings.contains("sparse_geometry"))
            {
                FrameRenderer->SetSparseSimulationGeometryEnabled(Settings.at("sparse_geometry").get<bool>());
            }
            if (Settings.contains("active_channel_mask"))
            {
                FrameRenderer->SetActiveChannelMaskEnabled(Settings.at("active_channel_mask").get<bool>());
            }
            if (Settings.contains("per_workgroup_channel_mask"))
            {
                FrameRenderer->SetPerWorkgroupChannelMaskEnabled(Settings.at("per_workgroup_channel_mask").get<bool>());
            }
            if (Settings.contains("overlay_tile_culling"))
            {
                FrameRenderer->SetOverlayTileCullingEnabled(Settings.at("overlay_tile_culling").get<bool>());
            }

            // Shared render-mesh resolution
            if (Settings.contains("render_mesh_resolution"))
            {
                const auto Resolution = Settings.at("render_mesh_resolution").get<unsigned>();
                FrameRenderer->SetSurfaceTexelMeshResolution(Resolution);
                FrameRenderer->SetOverlayTexelMeshResolution(Resolution);
            }
        }

        void FApplication::PrewarmBenchmarkMeshCache()
        {
            if (!PerformanceSettings().value("prewarm_mesh_cache", false))
            {
                return;
            }

            const auto &GPUResources = SurfaceStateSystem->GetGPUResources();

            for (std::size_t Index = 0; Index < GPUResources.GetSceneInstanceCount(); ++Index)
            {
                if (const auto *Geometry = GPUResources.GetInstanceSharedGeometry(Index))
                {
                    for (const auto &Preset : SurfaceState::SurfaceRenderMeshResolutionPresets)
                    {
                        (void)Geometry->GetTexelMeshVariant(Preset.Resolution);
                    }
                }
            }
        }

    #pragma endregion

#pragma endregion

// ============================================================
// 01. Main Loop
// ============================================================
#pragma region 01_Main_Loop
    // 종료 요청까지 입력·시간·Scene 갱신과 렌더 제출을 반복한다.

    void FApplication::Run(std::size_t FrameLimit)
    {
        if (FrameLimit == 0)
        {
            FLogger::Info("TApplication", "Entering main loop.");
        }
        else
        {
            FLogger::Info("TApplication", "Entering main loop for " + std::to_string(FrameLimit) + " frame(s).");
        }

        MainLoop(FrameLimit);
        FLogger::Info("TApplication", "Main loop finished.");
    }

    void FApplication::MainLoop(std::size_t FrameLimit)
    {
        std::size_t         RenderedFrameCount = 0;
        std::vector<double> BenchmarkCpuFrames;
        auto                PreviousFrameTime = std::chrono::steady_clock::now();

        while (!MainWindow.ShouldClose() && (FrameLimit == 0 || RenderedFrameCount < FrameLimit))
        {
            ExecuteFrame(PreviousFrameTime, BenchmarkCpuFrames);

            ++RenderedFrameCount;
        }

        FinalizeRun(BenchmarkCpuFrames);
    }

    void FApplication::ExecuteFrame(std::chrono::steady_clock::time_point &PreviousFrameTime,
                                    std::vector<double>                   &BenchmarkCpuFrames)
    {
        const auto FrameCpuStart = std::chrono::steady_clock::now();

        MainWindow.PollEvents();
        DebugInterface->BeginFrame(MainScene);
        SubmitInteractiveContact();

        const FFrameTiming Timing = UpdateFrameTiming(PreviousFrameTime);
        AdvanceDemoAndSubmitContacts(Timing.PlaybackDeltaTime);
        FrameRenderer->RenderFrame(MainScene,
                                   *DebugInterface,
                                   Timing.PlaybackDeltaTime,
                                   Timing.bSuspendSimulationClock);

        if (Timing.bSuspendSimulationClock)
        {
            WaitForSettingsAndResetClock(PreviousFrameTime);
        }

        if (Benchmark.bEnabled)
        {
            BenchmarkCpuFrames.push_back(
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - FrameCpuStart).count());
        }
    }
#pragma endregion

// ============================================================
// 02. Frame Input and Timing
// ============================================================
#pragma region 02_Frame_Input_and_Timing
    // 프레임 입력과 시간 값을 준비해 다음 Scene·Solver 갱신에 전달한다.

    void FApplication::SubmitInteractiveContact()
    {
        if (const std::optional<SurfaceState::FSurfaceContactInput> Contact =
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
            FLogger::Info("TInputSystem",
                          "Debug contact submitted for processing (state=" + std::to_string(Contact->State) +
                              ", instance=" + std::to_string(Contact->TargetInstance) +
                              ", strength=" + std::to_string(Contact->Strength) + ").");
        }
    }

    FApplication::FFrameTiming FApplication::UpdateFrameTiming(std::chrono::steady_clock::time_point &PreviousFrameTime)
    {
        const auto CurrentFrameTime = std::chrono::steady_clock::now();

        // Settings and file dialogs can block inside BeginFrame. Exclude
        // their elapsed time from the interactive simulation clock.
        const bool  bSuspendSimulationClock = DebugInterface->ConsumeFrameTimeResetRequest();
        const float DeltaTime = Benchmark.bEnabled ? SurfaceState::FixedSimulationStepSeconds
                                : bSuspendSimulationClock
                                    ? 0.0F
                                    : std::chrono::duration<float>(CurrentFrameTime - PreviousFrameTime).count();
        PreviousFrameTime = CurrentFrameTime;
        const float PlaybackDeltaTime = !Benchmark.bEnabled && !bSuspendSimulationClock &&
                                                MainScene.IsDemoAnimationPlaying() && MainScene.HasDemoContactEvents()
                                            ? SurfaceState::FixedSimulationStepSeconds
                                            : DeltaTime;
        return {bSuspendSimulationClock, PlaybackDeltaTime};
    }

    void FApplication::AdvanceDemoAndSubmitContacts(float PlaybackDeltaTime)
    {
        const auto Events = MainScene.AdvanceDemoAnimation(PlaybackDeltaTime * DebugInterface->GetAnimationTimeScale());

        for (const auto &Event : Events)
        {
            const auto &Instances = MainScene.GetStaticMeshInstances();
            const auto  Target =
                std::find_if(Instances.begin(),
                             Instances.end(),
                             [&](const FStaticMeshInstance &Instance) { return Instance.GetId() == Event.Target; });
            if (Target == Instances.end())
                throw std::runtime_error("Demo contact target disappeared from Scene: " + Event.Target);
            const glm::mat4                    Model = Target->GetTransform().GetMatrix();
            SurfaceState::FSurfaceContactInput Contact;
            Contact.TargetInstance =
                static_cast<SurfaceState::FSurfaceInstanceID>(std::distance(Instances.begin(), Target));
            Contact.State = SurfaceData.GetSurfaceStateRegistry().GetStateId(Event.State);
            Contact.WorldPosition = glm::vec3(Model * glm::vec4(Event.LocalPosition, 1.0F));
            Contact.PlaneNormal = glm::normalize(glm::transpose(glm::inverse(glm::mat3(Model))) * Event.LocalNormal);
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
    }

    void FApplication::WaitForSettingsAndResetClock(std::chrono::steady_clock::time_point &PreviousFrameTime)
    {
        // The setting's GPU work can outlive RenderFrame. Resume the
        // clock only after that work is complete.
        if (vkDeviceWaitIdle(Context.GetDevice()) != VK_SUCCESS)
            throw std::runtime_error("Failed to wait for GPU after a setting change.");
        PreviousFrameTime = std::chrono::steady_clock::now();
    }

    void FApplication::FinalizeRun(const std::vector<double> &BenchmarkCpuFrames)
    {
        FLogger::Debug("TApplication", "Waiting for the Vulkan device to become idle before shutdown.");
        vkDeviceWaitIdle(Context.GetDevice());

        if (Benchmark.bEnabled)
        {
            FrameRenderer->FinalizeBenchmarkCapture();
            std::ofstream(Benchmark.OutputPath.string() + ".cpu.json") << nlohmann::json(BenchmarkCpuFrames).dump();
        }
    }

#pragma endregion
} // namespace MDSS
