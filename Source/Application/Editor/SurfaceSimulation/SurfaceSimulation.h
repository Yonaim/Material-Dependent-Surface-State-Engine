/**
 * @file SurfaceSimulation.h
 * @brief Surface Simulation의 에디터 세션과 독립 실행 진입점을 정의한다.
 */

#pragma once

#include "Application/Editor/SurfaceSimulation/EditorPanel.h"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <vulkan/vulkan.h>

struct ImVec2;

namespace MDSS::GPU
{
    class FVulkanContext;
}

namespace MDSS
{
    struct FApplicationLaunchOptions;

    namespace SurfaceSimulation
    {
        enum class EScene : int;

        /** @brief 현재 표시 중인 조건들의 중복 없는 바닥 재질과 상태 표시 이름이다. */
        struct FSceneOverviewValues
        {
            std::vector<std::string_view> Materials;
            std::vector<std::string_view> LayerStates;
            std::vector<std::string_view> SubstrateStates;
        };
        // ============================================================
        // 00. Embedded Editor Session
        // ============================================================
        #pragma region 00_Embedded_Editor_Session
            // 기존 에디터의 장치와 프레임 수명을 공유하며 장면 선택, 화면 표시와 실험을 연결한다.

            /**
             * @brief 에디터에 삽입되는 Surface Simulation 장면과 표시 리소스를 소유한다.
             * @pre Context의 Vulkan 장치와 에디터의 ImGui 백엔드는 세션의 수명 동안 유효해야 한다.
             */
            class FEditorSession
            {
                public:
                /** @brief 기존 에디터의 장치와 ImGui backend를 빌려 대표 씬을 초기화한다. */
                explicit FEditorSession(const GPU::FVulkanContext &Context);
                ~FEditorSession();

                /** @brief 공통 씬 라이브러리에서 내장 씬이나 저작된 씬을 연다. */
                void                                       OpenScene(EScene Scene);
                void                                       OpenScene(const std::filesystem::path &Path);
                [[nodiscard]] EScene                       GetScene() const noexcept;
                [[nodiscard]] const std::filesystem::path &GetScenePath() const noexcept;
                /** @brief 좌우 비교 씬의 표시 이름을 반환한다. 단일 씬은 파일 이름을 쓴다. */
                [[nodiscard]] std::string_view GetComparisonTitle() const noexcept;
                /** @brief 적용된 씬의 요약을 읽는다. 씬 전환 및 설정 적용 시 반환값의 문자열 뷰가 무효화된다. */
                [[nodiscard]] FSceneOverviewValues GetSceneOverviewValues() const;
                void                               BeginFrame();
                void                               DrawPanel(EEditorPanel Panel, FSceneAssetText SceneText = {});
                void                               DrawExperimentPlot();
                void                               RefreshProfilingReadings();
                void                               DrawProfilingWorkload() const;
                void                               QueueCapture(EEditorCapture Capture);
                [[nodiscard]] int                  GetViewCount() const noexcept;
                void                               SetViewCount(int Count);
                void                               DrawRenderOptions();
                void                               DrawQuickViewControls();
                void                               DrawCompareMenu();
                /** @brief 재생 조작부에 표시할 물의 출처와 누적 주입량을 반환한다. */
                [[nodiscard]] std::string GetWaterSourceStatus() const;
                /** @brief 현재 적용된 외곽 출구를 재생 조작부에 표시할 문구를 반환한다. */
                [[nodiscard]] const char *GetOutletStatus() const;

                void DrawViewport(const ImVec2 &Position, const ImVec2 &Size, bool bPresentation = false);

                /** @brief 적용한 설정을 유지하고 다음 프레임에 장면을 처음부터 재생한다. */
                void Restart();

                /** @brief 기존 renderer의 main pass 전에 이번 프레임 명령을 기록한다. */
                void Record(VkCommandBuffer Commands, std::uint32_t FrameSlot);

                private:
                class FImpl;
                std::unique_ptr<FImpl> Impl;
            };

        #pragma endregion

        // ============================================================
        // 01. Standalone Entry Point
        // ============================================================
        #pragma region 01_Standalone_Entry_Point
            // 실행 옵션에 맞춰 독립 검증 또는 표시 경로를 시작한다.

            /** @brief 실행 옵션에서 CPU 기준 검사, Vulkan 검사 또는 인터랙티브 실행을 선택한다. */
            void Run(const FApplicationLaunchOptions &Options);

        #pragma endregion

    } // namespace SurfaceSimulation
} // namespace MDSS
