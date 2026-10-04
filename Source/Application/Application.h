/**
 * @file Application.h
 * @brief 응용 프로그램 초기화, 하위 시스템 구성과 메인 루프.
 */

#pragma once

#include "Application/Window.h"
#include "AssetManager/Core/AssetManager.h"
#include "GPU/Vulkan/VulkanContext.h"
#include "Scene/Scene.h"
#include "SurfaceState/Preprocessing/SurfaceDataManager.h"

#include <cstddef>
#include <memory>

namespace MDSS::Rendering
{
    class TRenderer;
}
namespace MDSS::SurfaceState
{
    class TSurfaceStateSystem;
}

namespace MDSS
{
    class TDebugUI;
    class TInputSystem;

    class TApplication
    {
    public:
        TApplication();
        ~TApplication();

        /** @brief 엔진 main loop를 시작하고 종료 시 정상 정리를 수행한다. 0은 무제한 실행이다. */
        void Run(std::size_t FrameLimit = 0);

    private:
        /** @brief window 종료까지 event polling, UI 갱신, frame rendering을 반복한다. */
        void MainLoop(std::size_t FrameLimit);

        // Declaration order is intentional: resources are destroyed in reverse order.
        TWindow                                            MainWindow;
        GPU::TVulkanContext                                Context;
        Asset::TAssetManager                               Assets;
        SurfaceState::TSurfaceDataManager                  SurfaceData;
        TScene                                             MainScene;
        std::unique_ptr<SurfaceState::TSurfaceStateSystem> SurfaceStates;
        std::unique_ptr<Rendering::TRenderer>              FrameRenderer;
        std::unique_ptr<TDebugUI>                          DebugInterface;
        std::unique_ptr<TInputSystem>                      InputInterface;
    };
} // namespace MDSS
