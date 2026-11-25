/**
 * @file Experiments.h
 * @brief Surface Simulation 상태 내보내기와 CPU 및 GPU 비교 실험 API를 정의한다.
 */

#pragma once

#include "Application/Editor/SurfaceSimulation/SurfaceSceneBuilder.h"
#include <string>

namespace MDSS::GPU
{
    class FVulkanContext;
}

namespace MDSS::SurfaceSimulation
{
    struct FExperimentResult
    {
        std::string        Report;
        std::vector<float> SurfaceMilliliters;
        std::vector<float> AbsorbedGrams;
        std::vector<float> DepthMillimeters;
        int                Columns = 0;
        int                Rows = 0;
    };

    void CheckReferenceContracts(const GPU::FVulkanContext   *Context,
                                 const std::filesystem::path &Output,
                                 bool                         bProfileBasins);
    /**
     * @brief 장면을 재생해 기준 계산을 검사하고 질량 장부와 최종 상태를 보고한다.
     * @param Context 지정하면 같은 스텝에서 GPU 상태를 CPU 기준과 비교한다.
     */
    [[nodiscard]] FExperimentResult RunExperiment(const FSurfaceSceneSettings &Settings,
                                                  const GPU::FVulkanContext   *Context,
                                                  const std::filesystem::path &Output,
                                                  bool                         bAssertScenario);
    /** @brief 지정한 자세에서 현재 셀 상태를 샘플 격자로 펼쳐 내보낸다. */
    [[nodiscard]] FExperimentResult ExportState(const FSurfaceSceneSettings           &Settings,
                                                const FSurfaceSceneRuntime            &Scene,
                                                const Liquid::FSurfaceSimulationState &State,
                                                Liquid::FSurfaceTilt                   Pose,
                                                const std::filesystem::path           &Output);

    /** @brief 저장된 실험 데이터로 PNG를 그리고 전체 데이터 내보내기에는 SVG를 추가한다. */
    void GenerateExperimentPlot(const std::filesystem::path &Report, bool bKeepData);
} // namespace MDSS::SurfaceSimulation
