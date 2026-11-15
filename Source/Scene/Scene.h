/**
 * @file Scene.h
 * @brief 주 카메라와 정적 메시 인스턴스 구성.
 */

#pragma once

#include "Scene/Camera.h"
#include "Scene/DemoAnimation.h"
#include "Scene/StaticMeshInstance.h"
#include "SurfaceState/Types/SurfaceMappingTypes.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace MDSS
{
    struct FSceneInitialContact
    {
        std::string Target;
        std::string State;
        glm::vec3   WorldPosition{0.0F};
        float       Radius = 0.0F;
        float       Strength = 0.0F;
        float       Falloff = 1.0F;
    };

    class FScene
    {
    public:
        // Scene lifecycle and configuration
        FScene();

        [[nodiscard]] FCamera&                     GetMainCamera() noexcept;
        [[nodiscard]] const FCamera&               GetMainCamera() const noexcept;
        [[nodiscard]] const FCamera&               GetInitialCamera() const noexcept;
        void                                       SetSourcePath(std::filesystem::path Path);
        [[nodiscard]] const std::filesystem::path& GetSourcePath() const noexcept;
        [[nodiscard]] std::uint32_t                GetSimulationResolution() const noexcept;
        /** @brief Scene의 모든 Surface에 적용할 해상도. Runtime 자원 교체는 Renderer가 처리한다. */
        void                SetSimulationResolution(std::uint32_t Resolution);
        [[nodiscard]] float GetLitHeightDisplayScale() const noexcept;
        /** @brief Lit 및 표면 디버그 미리보기의 렌더링 전용 높이 배율. */
        void SetLitHeightDisplayScale(float Scale);
        /** @brief 현재 Camera·object Transform을 Scene의 재시작 기준으로 저장한다. */
        // Initial state and contact management
        void CaptureInitialState();
        /** @brief 저장된 초기 Transform과 animation key를 적용하고 animation 시간을 되돌린다. */
        void RestoreInitialState();

        // Demo animation state
        void SetDemoAnimation(std::filesystem::path Path, FDemoAnimationClip Clip);
        [[nodiscard]] const std::filesystem::path&             GetDemoAnimationPath() const noexcept;
        [[nodiscard]] bool                                     HasDemoAnimation() const noexcept;
        [[nodiscard]] bool                                     HasDemoContactEvents() const noexcept;
        [[nodiscard]] bool                                     IsDemoAnimationPlaying() const noexcept;
        [[nodiscard]] float                                    GetDemoAnimationTime() const noexcept;
        [[nodiscard]] float                                    GetDemoAnimationDuration() const noexcept;
        [[nodiscard]] const FDemoAnimationClip::FWeatherCue*   GetDemoWeather() const noexcept;
        void                                                   PlayDemoAnimation();
        void                                                   PauseDemoAnimation() noexcept;
        void                                                   RestartDemoAnimation();
        std::vector<FDemoAnimationClip::FContactEvent>         AdvanceDemoAnimation(float DeltaTime);
        void                                                   AddInitialContact(FSceneInitialContact Contact);
        [[nodiscard]] const std::vector<FSceneInitialContact>& GetInitialContacts() const noexcept;

        /** @brief FScene 소유 목록에 정적 메시 인스턴스를 추가한다. */
        // Static mesh instances
        void                                                  AddStaticMeshInstance(FStaticMeshInstance Instance);
        [[nodiscard]] std::vector<FStaticMeshInstance>&       GetStaticMeshInstances() noexcept;
        [[nodiscard]] const std::vector<FStaticMeshInstance>& GetStaticMeshInstances() const noexcept;

    private:
        FCamera                           MainCamera;
        std::vector<FStaticMeshInstance>  StaticMeshInstances;
        std::filesystem::path             SourcePath;
        std::uint32_t                     SimulationResolution = SurfaceState::SurfaceSimulationResolution;
        float                             LitHeightDisplayScale = 4.0F;
        std::filesystem::path             DemoAnimationPath;
        std::optional<FDemoAnimationClip> DemoAnimation;
        float                             DemoAnimationTime = 0.0F;
        bool                              bDemoAnimationPlaying = false;
        std::vector<FSceneInitialContact> InitialContacts;
        FCamera                           InitialCamera;
        std::vector<FTransform>           InitialTransforms;
    };
} // namespace MDSS
