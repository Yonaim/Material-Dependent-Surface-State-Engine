/**
 * @file Application.h
 * @brief 응용 프로그램 초기화, 하위 시스템 구성과 메인 루프.
 */

#pragma once

#include "Application/BenchmarkOptions.h"
#include "Application/Window.h"
#include "AssetManager/Core/AssetManager.h"
#include "GPU/Vulkan/VulkanContext.h"
#include "Scene/Scene.h"
#include "SurfaceState/Runtime/SurfaceDataManager.h"

#include <chrono>
#include <cstddef>
#include <memory>
#include <vector>

namespace MDSS::Rendering
{
    class FRenderer;
}

namespace MDSS::SurfaceState
{
    class FSurfaceStateSystem;
}

namespace MDSS
{
    class FDebugUI;
    class FFrameCoordinator;
    class FInputSystem;

    class FApplication
    {
    public:
        // Application lifecycle and execution
        explicit FApplication(FBenchmarkOptions     BenchmarkOptions = {},
                              std::uint32_t         RenderTileSize = 16U,
                              bool                  bPerWorkgroupChannelMask = true,
                              std::filesystem::path ScenePath = {});
        ~FApplication();

        /** @brief 엔진 main loop를 시작하고 종료 시 정상 정리를 수행한다. 0은 무제한 실행이다. */
        void Run(std::size_t FrameLimit = 0);

    private:
        /** @brief window 종료까지 event polling, UI 갱신, frame rendering을 반복한다. */
        void MainLoop(std::size_t FrameLimit);

        struct FFrameTiming
        {
            bool  bSuspendSimulationClock;
            float PlaybackDeltaTime;
        };

        void                       SubmitInteractiveContact();
        [[nodiscard]] FFrameTiming UpdateFrameTiming(std::chrono::steady_clock::time_point& PreviousFrameTime);
        void                       AdvanceDemoAndSubmitContacts(float PlaybackDeltaTime);
        void WaitForSettingsAndResetClock(std::chrono::steady_clock::time_point& PreviousFrameTime);
        void FinalizeRun(const std::vector<double>& BenchmarkCpuFrames);

        // Declaration order is intentional: resources are destroyed in reverse order.
        FWindow                                            MainWindow;
        GPU::FVulkanContext                                Context;
        Asset::FAssetManager                               Assets;
        SurfaceState::FSurfaceDataManager                  SurfaceData;
        FScene                                             MainScene;
        std::unique_ptr<SurfaceState::FSurfaceStateSystem> SurfaceStateSystem;
        std::unique_ptr<FFrameCoordinator>                 FrameCoordinator;
        std::unique_ptr<Rendering::FRenderer>              FrameRenderer;
        std::unique_ptr<FDebugUI>                          DebugInterface;
        std::unique_ptr<FInputSystem>                      InputInterface;
        FBenchmarkOptions                                  Benchmark;
    };
} // namespace MDSS
