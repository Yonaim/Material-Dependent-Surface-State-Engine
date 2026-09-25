/**
 * @file Application.h
 * @brief 응용 프로그램 초기화, 하위 시스템 구성과 메인 루프.
 */

#pragma once

#include "Application/Window.h"
#include "AssetManager/AssetManager.h"
#include "Scene/Scene.h"
#include "VulkanContext/VulkanContext.h"

#include <memory>

namespace MDSS
{
    class DebugUI;
    class Renderer;

    class Application
    {
    public:
        Application();
        ~Application();

        /** @brief 엔진 main loop를 시작하고 종료 시 정상 정리를 수행한다. */
        void Run();

    private:
        /** @brief window 종료까지 event polling, UI 갱신, frame rendering을 반복한다. */
        void MainLoop();

        // Declaration order is intentional: resources are destroyed in reverse order.
        Window                    MainWindow;
        VulkanContext             Context;
        AssetManager              Assets;
        Scene                     MainScene;
        std::unique_ptr<Renderer> FrameRenderer;
        std::unique_ptr<DebugUI>  DebugInterface;
    };
} // namespace MDSS
