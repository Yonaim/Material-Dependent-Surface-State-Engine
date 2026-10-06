/**
 * @file Application.cpp
 * @brief 응용 프로그램 초기화, 하위 시스템 구성과 메인 루프.
 */

#include "Application/Application.h"

#include "Application/BenchmarkOptions.h"
#include "Application/EngineConfig.h"
#include "DebugUI/DebugUI.h"
#include "InputSystem/InputSystem.h"
#include "Logger/Logger.h"
#include "Rendering/Renderer.h"
#include "Scene/SceneLoader.h"
#include "SurfaceState/SurfaceStateSystem.h"
#include "SurfaceState/State/SimulationClock.h"

#include <chrono>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <utility>

namespace MDSS
{
#pragma region Lifecycle

    TApplication::TApplication(TBenchmarkOptions BenchmarkOptions)
        : MainWindow(1280, 720, "MDSS Engine"), Context(MainWindow), Assets(Context), SurfaceData(Assets), MainScene(),
          Benchmark(std::move(BenchmarkOptions))
    {
        TLogger::Info("TApplication", "Initializing MDSS Engine.");

        const auto StartupScenePath = Benchmark.Enabled
                                          ? std::filesystem::absolute(Benchmark.ScenePath).lexically_normal()
                                          : LoadStartupScenePath(GetEngineConfigDirectory() / "Engine.ini");
        TLogger::Info("TApplication", "Startup Scene: " + StartupScenePath.string());
        MainScene = TSceneLoader::Load(StartupScenePath,
                                       Assets,
                                       SurfaceData,
                                       Benchmark.Enabled ? std::optional<std::uint32_t>(Benchmark.Resolution)
                                                         : std::nullopt);

        SurfaceData.ExchangeSurfaceStateRegistry(SurfaceData.BuildSurfaceStateRegistry(MainScene));
        SurfaceStates = std::make_unique<SurfaceState::TSurfaceStateSystem>(Context, Assets, SurfaceData, MainScene);
        FrameRenderer =
            std::make_unique<Rendering::TRenderer>(Context, MainWindow, Assets, SurfaceData, MainScene, *SurfaceStates);
        DebugInterface = std::make_unique<TDebugUI>(Context, MainWindow, *FrameRenderer, Assets, SurfaceData);
        if (Benchmark.Enabled)
        {
            MainScene.PauseDemoAnimation();
            DebugInterface->SetBenchmarkMode();
            FrameRenderer->ConfigureBenchmarkCapture(Benchmark.OutputPath,
                                                      Benchmark.WarmupFrames,
                                                      Benchmark.MeasurementFrames);
        }
        InputInterface = std::make_unique<TInputSystem>(MainWindow.GetNativeHandle());
        TLogger::Info("TApplication", "Surface State System, renderer, scene, asset system, and Debug UI are ready.");
    }

    TApplication::~TApplication() = default;

#pragma endregion

#pragma region Main_Loop

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
        std::size_t RenderedFrameCount = 0;
        auto        PreviousFrameTime = std::chrono::steady_clock::now();
        while (!MainWindow.ShouldClose() && (FrameLimit == 0 || RenderedFrameCount < FrameLimit))
        {
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
                                                     DebugInterface->GetInjectTexelSearchRadius(),
                                                     DebugInterface->ShouldSuppressDebugHotkey()))
            {
                SurfaceStates->SubmitContact(*Contact);
                TLogger::Info("TInputSystem",
                              "Debug contact submitted for processing (state=" + std::to_string(Contact->State) +
                                  ", instance=" + std::to_string(Contact->TargetInstance) +
                                  ", strength=" + std::to_string(Contact->Strength) + ").");
            }
            const auto CurrentFrameTime = std::chrono::steady_clock::now();
            // Settings and file dialogs can block inside BeginFrame. Exclude
            // their elapsed time from the interactive simulation clock.
            const bool  bSuspendSimulationClock = DebugInterface->ConsumeFrameTimeResetRequest();
            const float DeltaTime = Benchmark.Enabled
                                        ? SurfaceState::FixedSimulationStepSeconds
                                        : bSuspendSimulationClock
                                              ? 0.0F
                                              : std::chrono::duration<float>(CurrentFrameTime - PreviousFrameTime)
                                                    .count();
            PreviousFrameTime = CurrentFrameTime;
            MainScene.AdvanceDemoAnimation(DeltaTime * DebugInterface->GetAnimationTimeScale());
            FrameRenderer->RenderFrame(MainScene, *DebugInterface, DeltaTime, bSuspendSimulationClock);
            if (bSuspendSimulationClock)
            {
                // The setting's GPU work can outlive RenderFrame. Resume the
                // clock only after that work is complete.
                if (vkDeviceWaitIdle(Context.GetDevice()) != VK_SUCCESS)
                    throw std::runtime_error("Failed to wait for GPU after a setting change.");
                PreviousFrameTime = std::chrono::steady_clock::now();
            }
            ++RenderedFrameCount;
        }

        TLogger::Debug("TApplication", "Waiting for the Vulkan device to become idle before shutdown.");
        vkDeviceWaitIdle(Context.GetDevice());
    }
#pragma endregion
} // namespace MDSS
