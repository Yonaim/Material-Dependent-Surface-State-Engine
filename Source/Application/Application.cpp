/**
 * @file Application.cpp
 * @brief 응용 프로그램 초기화, 하위 시스템 구성과 메인 루프.
 */

#include "Application/Application.h"
#include "Application/EngineConfig.h"
#include "Application/FrameCoordinator.h"
#include "DebugUI/Core/DebugUI.h"
#include "Logger/Logger.h"
#include "Rendering/Core/Renderer.h"
#include "SurfaceState/SurfaceStateSystem.h"

namespace MDSS
{
// ============================================================
// 00. Lifecycle
// ============================================================
#pragma region 00_Lifecycle
    // 객체 초기화와 종료에서 소유 자원·참조의 유효 기간을 맞춘다.

    FApplication::FApplication(std::filesystem::path ScenePath)
        : MainWindow(1280, 720, "MDSS Engine"),
          Context(MainWindow),
          Assets(Context),
          SurfaceData(Assets)
    {
        SurfaceStateSystem =
            std::make_unique<SurfaceState::FSurfaceStateSystem>(Context, Assets, SurfaceData, MainScene);
        FrameCoordinator = std::make_unique<FFrameCoordinator>(*SurfaceStateSystem);
        FrameRenderer = std::make_unique<Rendering::FRenderer>(Context,
                                                               MainWindow,
                                                               Assets,
                                                               SurfaceData,
                                                               MainScene,
                                                               *SurfaceStateSystem,
                                                               *FrameCoordinator);
        DebugInterface = std::make_unique<FDebugUI>(Context, MainWindow, *FrameRenderer, Assets, SurfaceData);

        if (ScenePath.empty())
            ScenePath = LoadStartupScenePath(GetEngineConfigDirectory() / "Engine.ini");
        if (ScenePath.empty())
            DebugInterface->SetSurfaceSimulationEnabled(true);
        else
            DebugInterface->OpenWorkspaceScene(std::filesystem::absolute(ScenePath).lexically_normal());
    }

    FApplication::~FApplication() = default;

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
        std::size_t RenderedFrameCount = 0;
        while (!MainWindow.ShouldClose() && (FrameLimit == 0 || RenderedFrameCount < FrameLimit))
        {
            MainWindow.PollEvents();
            DebugInterface->BeginFrame(MainScene);
            FrameRenderer->RenderFrame(MainScene, *DebugInterface, 0.F, DebugInterface->ConsumeFrameTimeResetRequest());
            ++RenderedFrameCount;
        }

        vkDeviceWaitIdle(Context.GetDevice());
    }
#pragma endregion
} // namespace MDSS
