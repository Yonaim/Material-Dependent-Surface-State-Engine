/**
 * @file SurfaceSimulationDisplay.h
 * @brief Surface Simulation의 뷰 설정, 프레임 입력과 표시 인터페이스를 정의한다.
 */

#pragma once

#include "Rendering/Surface/Simulation/SurfaceSimulationTiming.h"
#include "Rendering/Surface/StateVisualization.h"
#include "Rendering/Surface/SurfaceSimulationAppearance.h"
#include "SurfaceState/Simulation/SurfaceSimulationState.h"
#include <array>
#include <filesystem>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace MDSS
{
    class FWindow;
}
namespace MDSS::GPU
{
    class FVulkanContext;
}

namespace MDSS::Liquid
{
    class FSurfaceSimulationGpu;
}

namespace MDSS::SurfaceSimulation
{

    // ============================================================
    // 00. View Configuration
    // ============================================================
    #pragma region 00_View_Configuration
        // 뷰 ID는 셰이더 분기와 공유하며, 화면 표시 이름과 CLI 키는 같은 순서로 대응한다.

        enum class ESurfaceSimulationView : std::uint32_t
        {
            Flat,
            Fresnel,
            Refraction,
            LayerThickness,
            LayerStates,
            Temperature,
            SubstrateStates,
            Bed,
            Activity,
            Pbr,
            Elevation,
            SimulationGrid,
            GeometryGrid,
            StateHeatmap,
            ArealMass,
            ContactArea,
            EffectiveBed,
            SolverDepth,
            OverflowTransfers,
            AbsorptionFill,
            Count
        };

        inline constexpr auto SurfaceSimulationViewCount = static_cast<std::size_t>(ESurfaceSimulationView::Count);
        inline constexpr auto DefaultSurfaceSimulationView = ESurfaceSimulationView::Pbr;
        inline constexpr std::array<const char *, SurfaceSimulationViewCount> SurfaceSimulationViewNames{
            "Translucent",
            "Fresnel",
            "Screen refraction",
            "Layer Thickness [mm]",
            "Layer states",
            "Temperature",
            "Substrate states",
            "Bed elevation",
            "Emission activity",
            "PBR + refraction",
            "Elevation contours",
            "Simulation cells",
            "Geometry samples",
            "State heatmap",
            "Areal Mass [kg/m\xC2\xB2]",
            "Reaction contact",
            "Effective bed",
            "Solver water depth [mm]",
            "Overflow transfers",
            "Absorption fill [%]"};
        inline constexpr std::array<const char *, SurfaceSimulationViewCount> SurfaceSimulationViewKeys{
            "flat",          "fresnel",      "refraction",         "layer-thickness",
            "layer-states",  "temperature",  "substrate-states",   "bed",
            "activity",      "pbr",          "elevation",          "simulation-grid",
            "geometry-grid", "state",        "areal-mass",         "contact-area",
            "effective-bed", "solver-depth", "overflow-transfers", "absorption-fill"};
        inline constexpr std::uint32_t NoSurfaceStateSource = 0xffU;

        [[nodiscard]] inline bool UsesStateSource(ESurfaceSimulationView View)
        {
            using enum ESurfaceSimulationView;
            return View == StateHeatmap || View == LayerThickness || View == ArealMass;
        }

        [[nodiscard]] inline bool SupportsStateSource(ESurfaceSimulationView                View,
                                                      const Rendering::FStateVisualization &State)
        {
            using enum ESurfaceSimulationView;
            if (View == LayerThickness)
                return State.HasLayerThickness();
            if (View == ArealMass)
                return State.HasArealMass();
            return View == StateHeatmap;
        }

        /** @brief 두께 히트맵의 상한 [mm]. 눈은 표면 프로파일의 벌크 밀도를 사용한다. */
        [[nodiscard]] float ThicknessDisplayMaxMm(const Rendering::FStateVisualization &State,
                                                  const Liquid::FLiquidMaterial        &Liquid,
                                                  float                                 SnowBulkDensityKgM3);
        // The automatic palette ID is shared with SurfaceDiagnostics.glsl.
        inline constexpr int                         AutomaticHeatmapPalette = 3;
        inline constexpr std::array<const char *, 4> HeatmapNames{"Viridis", "Inferno", "Blue-white-red", "By state"};

        /** @brief Modes와 SmoothWater의 앞 Count개 항목을 사용한다. 뷰 수는 1~4개다. */
        struct FSurfaceSimulationViews
        {
            int                          Count = 1;
            std::array<int, 4>           Modes{static_cast<int>(DefaultSurfaceSimulationView), 3, 5, 8};
            std::array<std::uint32_t, 4> StateIndices{};
            std::array<bool, 4>          SmoothWater{};
            std::array<bool, 4>          SloshOptics{true, true, true, true};
            std::array<bool, 4>          SloshHeight{};
            std::array<bool, 4>          MaterialNormalMaps{true, true, true, true};
            std::array<bool, 4>          CellOutlines{};
            std::array<int, 4>           PaletteOverrides{-1, -1, -1, -1};
            std::array<int, 4>           TextureOverrides{-1, -1, -1, -1};
            std::array<int, 4>           EffectOverrides{-1, -1, -1, -1};
            int                          Palette = 0;
            int                          SmoothBedOverride = -1;
            bool                         bShowOverflow = true;
            bool                         bEffects = true;
            bool                         bTextures = true;
            bool                         bStudioStrip = true;
            bool                         bEnvironmentReflections = true;
            bool                         bWorldGrid = false;
            bool                         bWorldAxes = false;
            bool                         bSplitLowerRow = false;
            glm::vec2                    CameraFitSize{};
            glm::vec2                    CameraOffset{};
            float                        CameraAzimuth = -62.62F;
            float                        CameraElevation = 40.31F;
            float                        CameraDistanceScale = 1.F;
        };

        /** @brief 렌더링, 뷰 선택, 캡처에서 같은 픽셀 영역을 읽는다. */
        [[nodiscard]] VkRect2D
        SurfaceSimulationViewRect(VkExtent2D Extent, const FSurfaceSimulationViews &Views, int Index);

    #pragma endregion

    // ============================================================
    // 01. Frame Submission
    // ============================================================
    #pragma region 01_Frame_Submission
        // 시뮬레이션 입력 묶음을 프레임 제출과 함께 소비하고 요청한 화면을 캡처한다.

        /** @brief 직전에 그린 화면에서 수동 주입할 저장 영역과 그 영역의 형상 샘플을 나타낸다. */
        enum class EInjectionShape
        {
            Point,
            Circle,
            Rectangle
        };

        /** @brief 형상 차트의 미터 단위로 정의하는 주입 영역. */
        struct FInjectionBrush
        {
            EInjectionShape Shape = EInjectionShape::Point;
            float           RadiusM = 0.012F;
            glm::vec2       SizeM{0.028F, 0.016F};
            float           AngleRadians = 0.F;
        };

        /** @brief 각 뷰 안의 정규화 좌표로 투영한 예약 주입 영역이다. */
        struct FSurfaceInletOverlay
        {
            std::vector<std::array<glm::vec2, 4>> Quads;
            std::vector<std::array<glm::vec2, 2>> Edges;
        };

        struct FSurfaceInjectionFootprint
        {
            int View = 0;
            struct FRegionWeight
            {
                int   Region;
                float Fraction;
            };
            std::vector<FRegionWeight> Regions;
            struct FSample
            {
                std::array<glm::vec2, 4> Corners; // 전체 뷰포트 이미지 내 정규화 좌표
                std::uint32_t            BoundaryEdges;
                float                    Coverage;
            };
            std::vector<FSample> Samples;
        };

        /**
         * @brief 장면의 표시 리소스를 소유한다.
         * @pre Context와 생성 시 전달한 Window는 이 객체가 소멸할 때까지 유효해야 한다.
         */
        class FSurfaceSimulationDisplay
        {
            public:
            FSurfaceSimulationDisplay(const GPU::FVulkanContext                      &Context,
                                      FWindow                                        &Window,
                                      const Liquid::FSurfaceSimulationDomain         &Geometry,
                                      const Rendering::FSurfaceSimulationAppearance  &Appearance,
                                      std::span<const Rendering::FStateVisualization> States,
                                      bool                                            bSmoothDisplay);
            /** @brief 기존 에디터의 Vulkan context에 표시 자원을 만들고 swapchain은 생성하지 않는다. */
            FSurfaceSimulationDisplay(const GPU::FVulkanContext                      &Context,
                                      const Liquid::FSurfaceSimulationDomain         &Geometry,
                                      const Rendering::FSurfaceSimulationAppearance  &Appearance,
                                      std::span<const Rendering::FStateVisualization> States,
                                      bool                                            bSmoothDisplay);
            ~FSurfaceSimulationDisplay();

            /** @brief 표시 모드의 바닥 메시가 아직 없으면 만들고 이후 프레임에서 재사용한다. */
            void PrepareBedDisplay(const Liquid::FSurfaceSimulationDomain        &Geometry,
                                   const Rendering::FSurfaceSimulationAppearance &Appearance,
                                   const FSurfaceSimulationViews                 &Views);

            /** @brief 에디터 ImGui가 사용할 텍스처를 준비한다. 크기가 같은 동안 재사용한다. */
            [[nodiscard]] VkDescriptorSet PrepareEditorTarget(VkExtent2D Extent, const FSurfaceSimulationViews &Views);

            /** @brief 표시 이미지의 정규화 좌표에서 바닥 윗면의 저장 영역을 찾는다. */
            [[nodiscard]] std::optional<FSurfaceInjectionFootprint>
            PickInjectionFootprint(glm::vec2 Position, const FInjectionBrush &Brush) const;

            /** @brief 완료한 표시 스냅샷에서 모든 계산 셀의 최저·최고 온도 [K]를 반환한다. */
            [[nodiscard]] std::optional<glm::vec2> TemperatureRangeK() const;

            /** @brief 표시 이미지 좌표를 바닥의 계산 셀에 대응시켜 완료한 스냅샷의 온도 [K]를 얻는다. */
            [[nodiscard]] std::optional<float> PickTemperatureK(glm::vec2 Position) const;

            /** @brief 이번 프레임에 표시할 출구를 해당 뷰의 정규화 좌표로 투영한다. */
            [[nodiscard]] std::vector<std::array<glm::vec2, 2>> ProjectOutletEdge(const FSurfaceSimulationViews &Views,
                                                                                  int View) const;

            /** @brief 예약 주입을 받는 형상 샘플의 윗면과 바깥 테두리를 투영한다. */
            [[nodiscard]] FSurfaceInletOverlay ProjectInletOverlay(const FSurfaceSimulationViews &Views, int View) const;

            /** @brief 에디터 main pass 밖에서 GPU 스텝과 표시·캡처 명령을 기록한다. */
            void RecordEditorFrame(VkCommandBuffer                            Commands,
                                   std::uint32_t                              FrameSlot,
                                   Liquid::FSurfaceSimulationGpu             &Simulation,
                                   std::span<const Liquid::FSurfaceStepInput> Inputs,
                                   const FSurfaceSimulationViews             &Views,
                                   const std::filesystem::path               &Capture,
                                   int                                        CaptureView = -1);

            /** @brief 제출된 캡처가 있으면 GPU 완료 후 RGB PNG로 저장한다. */
            void FinishEditorCapture();

            /**
             * @brief 입력 스텝을 기록하고 최신 상태로 화면을 그려 제출한다.
             * @pre FWindow를 전달해 만든 객체에서 호출한다.
             * @param Capture 비어 있지 않고 읽기 가능한 스왑체인이면 합성된 화면을 RGB PNG로 저장한다.
             * @return 제출하면 true를 반환한다. 이미지 획득 중 스왑체인을 재생성하면 false를 반환하므로 같은 Inputs를
             * 재시도한다.
             */
            bool                                         DrawFrame(Liquid::FSurfaceSimulationGpu             &Simulation,
                                                                   std::span<const Liquid::FSurfaceStepInput> Inputs,
                                                                   const FSurfaceSimulationViews             &Views,
                                                                   const std::filesystem::path               &Capture = {});
            [[nodiscard]] bool                           CanCapture() const;
            [[nodiscard]] FSurfaceSimulationDisplayStats Stats() const;

            private:
            class FImpl;
            std::unique_ptr<FImpl> Impl;
        };

    #pragma endregion

} // namespace MDSS::SurfaceSimulation
