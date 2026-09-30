/**
 * @file Scene.h
 * @brief 주 카메라와 정적 메시 인스턴스 구성.
 */

#pragma once

#include "Scene/Camera.h"
#include "Scene/StaticMeshInstance.h"
#include "SurfaceStateSystem/Types/SurfaceMappingTypes.h"

#include <vector>
#include <filesystem>

namespace MDSS
{
    class TScene
    {
    public:
        TScene();

        [[nodiscard]] TCamera&       GetMainCamera() noexcept;
        [[nodiscard]] const TCamera& GetMainCamera() const noexcept;
        void SetSourcePath(std::filesystem::path Path);
        [[nodiscard]] const std::filesystem::path& GetSourcePath() const noexcept;
        [[nodiscard]] std::uint32_t GetSimulationResolution() const noexcept;
        /** @brief Scene의 모든 Surface에 적용할 해상도. Runtime 자원 교체는 Renderer가 처리한다. */
        void SetSimulationResolution(std::uint32_t Resolution);
        [[nodiscard]] float GetLitHeightDisplayScale() const noexcept;
        /** @brief Lit 및 표면 디버그 미리보기의 렌더링 전용 높이 배율. */
        void SetLitHeightDisplayScale(float Scale);

        /** @brief TScene 소유 목록에 정적 메시 인스턴스를 추가한다. */
        void                                                 AddStaticMeshInstance(TStaticMeshInstance Instance);
        [[nodiscard]] std::vector<TStaticMeshInstance>&       GetStaticMeshInstances() noexcept;
        [[nodiscard]] const std::vector<TStaticMeshInstance>& GetStaticMeshInstances() const noexcept;

    private:
        TCamera                          MainCamera;
        std::vector<TStaticMeshInstance> StaticMeshInstances;
        std::filesystem::path SourcePath;
        std::uint32_t SimulationResolution = SurfaceSimulationResolution;
        float LitHeightDisplayScale = 4.0F;
    };
} // namespace MDSS
