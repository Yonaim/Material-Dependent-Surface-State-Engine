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
    struct TSceneInitialContact
    {
        std::string Target;
        std::string State;
        glm::vec3   WorldPosition{0.0F};
        float       Radius = 0.0F;
        float       Strength = 0.0F;
        float       Falloff = 1.0F;
    };

    class TScene
    {
    public:
        TScene();

        [[nodiscard]] TCamera&                     GetMainCamera() noexcept;
        [[nodiscard]] const TCamera&               GetMainCamera() const noexcept;
        [[nodiscard]] const TCamera&               GetInitialCamera() const noexcept;
        void                                       SetSourcePath(std::filesystem::path Path);
        [[nodiscard]] const std::filesystem::path& GetSourcePath() const noexcept;
        [[nodiscard]] std::uint32_t                GetSimulationResolution() const noexcept;
        /** @brief Scene의 모든 Surface에 적용할 해상도. Runtime 자원 교체는 Renderer가 처리한다. */
        void                SetSimulationResolution(std::uint32_t Resolution);
        [[nodiscard]] float GetLitHeightDisplayScale() const noexcept;
        /** @brief Lit 및 표면 디버그 미리보기의 렌더링 전용 높이 배율. */
        void SetLitHeightDisplayScale(float Scale);
        /** @brief 현재 Camera·object Transform을 Scene의 재시작 기준으로 저장한다. */
        void CaptureInitialState();
        /** @brief 저장된 초기 Transform과 animation key를 적용하고 animation 시간을 되돌린다. */
        void RestoreInitialState();
        void SetDemoAnimation(std::filesystem::path Path, TDemoAnimationClip Clip);
        [[nodiscard]] const std::filesystem::path&             GetDemoAnimationPath() const noexcept;
        [[nodiscard]] bool                                     HasDemoAnimation() const noexcept;
        [[nodiscard]] bool                                     IsDemoAnimationPlaying() const noexcept;
        [[nodiscard]] float                                    GetDemoAnimationTime() const noexcept;
        [[nodiscard]] float                                    GetDemoAnimationDuration() const noexcept;
        void                                                   PlayDemoAnimation();
        void                                                   PauseDemoAnimation() noexcept;
        void                                                   RestartDemoAnimation();
        void                                                   AdvanceDemoAnimation(float DeltaTime);
        void                                                   AddInitialContact(TSceneInitialContact Contact);
        [[nodiscard]] const std::vector<TSceneInitialContact>& GetInitialContacts() const noexcept;

        /** @brief TScene 소유 목록에 정적 메시 인스턴스를 추가한다. */
        void                                                  AddStaticMeshInstance(TStaticMeshInstance Instance);
        [[nodiscard]] std::vector<TStaticMeshInstance>&       GetStaticMeshInstances() noexcept;
        [[nodiscard]] const std::vector<TStaticMeshInstance>& GetStaticMeshInstances() const noexcept;

    private:
        TCamera                           MainCamera;
        std::vector<TStaticMeshInstance>  StaticMeshInstances;
        std::filesystem::path             SourcePath;
        std::uint32_t                     SimulationResolution = SurfaceState::SurfaceSimulationResolution;
        float                             LitHeightDisplayScale = 4.0F;
        std::filesystem::path             DemoAnimationPath;
        std::optional<TDemoAnimationClip> DemoAnimation;
        float                             DemoAnimationTime = 0.0F;
        bool                              bDemoAnimationPlaying = false;
        std::vector<TSceneInitialContact> InitialContacts;
        TCamera                           InitialCamera;
        std::vector<TTransform>           InitialTransforms;
    };
} // namespace MDSS
