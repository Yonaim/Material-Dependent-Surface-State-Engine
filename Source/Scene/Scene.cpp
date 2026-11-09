/**
 * @file Scene.cpp
 * @brief 주 카메라와 정적 메시 인스턴스 구성.
 */

#include "Scene/Scene.h"

#include "Logger/Logger.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace MDSS
{
// ============================================================
// 00. Scene Lifecycle and Camera Access
// ============================================================
#pragma region 00_Scene_Lifecycle_and_Camera_Access

    FScene::FScene() : MainCamera({2.0F, -3.3F, 2.0F}, {0.0F, 0.0F, 0.0F}, 50.0F), InitialCamera(MainCamera)
    {
        FLogger::Debug("TScene", "Main camera created at default position.");
    }

    FCamera& FScene::GetMainCamera() noexcept
    {
        return MainCamera;
    }

    const FCamera& FScene::GetMainCamera() const noexcept
    {
        return MainCamera;
    }

    const FCamera& FScene::GetInitialCamera() const noexcept
    {
        return InitialCamera;
    }

#pragma endregion

// ============================================================
// 01. Scene Configuration
// ============================================================
#pragma region 01_Scene_Configuration

    void FScene::SetSourcePath(std::filesystem::path Path)
    {
        SourcePath = std::move(Path);
    }

    const std::filesystem::path& FScene::GetSourcePath() const noexcept
    {
        return SourcePath;
    }

    std::uint32_t FScene::GetSimulationResolution() const noexcept
    {
        return SimulationResolution;
    }

    void FScene::SetSimulationResolution(std::uint32_t Resolution)
    {
        if (!SurfaceState::IsSurfaceSimulationResolution(Resolution))
        {
            throw std::invalid_argument("Simulation resolution must be 128, 256 or 512.");
        }

        SimulationResolution = Resolution;
    }

    float FScene::GetLitHeightDisplayScale() const noexcept
    {
        return LitHeightDisplayScale;
    }

    void FScene::SetLitHeightDisplayScale(float Scale)
    {
        if (!std::isfinite(Scale) || Scale < 0.0F || Scale > 100.0F)
        {
            throw std::invalid_argument("Lit height display scale must be finite and between 0 and 100.");
        }

        LitHeightDisplayScale = Scale;
    }

#pragma endregion

// ============================================================
// 02. Initial State and Contacts
// ============================================================
#pragma region 02_Initial_State_and_Contacts

    void FScene::CaptureInitialState()
    {
        InitialCamera = MainCamera;
        InitialTransforms.clear();
        InitialTransforms.reserve(StaticMeshInstances.size());

        for (const FStaticMeshInstance& Instance : StaticMeshInstances)
            InitialTransforms.push_back(Instance.GetTransform());
    }

    void FScene::RestoreInitialState()
    {
        MainCamera = InitialCamera;
        const std::size_t Count = std::min(StaticMeshInstances.size(), InitialTransforms.size());

        for (std::size_t Index = 0; Index < Count; ++Index)
            StaticMeshInstances[Index].GetTransform() = InitialTransforms[Index];
        if (DemoAnimation)
        {
            DemoAnimationTime = 0.0F;
            bDemoAnimationPlaying = false;
            ApplyDemoAnimation(*this, *DemoAnimation, DemoAnimationTime);
        }
    }

#pragma endregion

// ============================================================
// 03. Demo Animation State
// ============================================================
#pragma region 03_Demo_Animation_State

    void FScene::SetDemoAnimation(std::filesystem::path Path, FDemoAnimationClip Clip)
    {
        DemoAnimationPath = std::move(Path);
        DemoAnimation = std::move(Clip);
        DemoAnimationTime = 0.0F;
        bDemoAnimationPlaying = DemoAnimation->bAutoplay;
    }

    const std::filesystem::path& FScene::GetDemoAnimationPath() const noexcept
    {
        return DemoAnimationPath;
    }

    bool FScene::HasDemoAnimation() const noexcept
    {
        return DemoAnimation.has_value();
    }

    bool FScene::HasDemoContactEvents() const noexcept
    {
        return DemoAnimation && !DemoAnimation->ContactEvents.empty();
    }

    bool FScene::IsDemoAnimationPlaying() const noexcept
    {
        return bDemoAnimationPlaying;
    }

    float FScene::GetDemoAnimationTime() const noexcept
    {
        return DemoAnimationTime;
    }

    float FScene::GetDemoAnimationDuration() const noexcept
    {
        return DemoAnimation ? DemoAnimation->DurationSeconds : 0.0F;
    }

    const FDemoAnimationClip::FWeatherCue* FScene::GetDemoWeather() const noexcept
    {
        return DemoAnimation && DemoAnimation->Weather ? &*DemoAnimation->Weather : nullptr;
    }

    void FScene::PlayDemoAnimation()
    {
        if (!DemoAnimation)
            return;
        if (DemoAnimationTime >= DemoAnimation->DurationSeconds)
            RestartDemoAnimation();
        bDemoAnimationPlaying = true;
    }

    void FScene::PauseDemoAnimation() noexcept
    {
        bDemoAnimationPlaying = false;
    }

    void FScene::AddInitialContact(FSceneInitialContact Contact)
    {
        InitialContacts.push_back(std::move(Contact));
    }

    const std::vector<FSceneInitialContact>& FScene::GetInitialContacts() const noexcept
    {
        return InitialContacts;
    }

    void FScene::RestartDemoAnimation()
    {
        if (!DemoAnimation)
            return;
        bDemoAnimationPlaying = false;
        DemoAnimationTime = 0.0F;
        ApplyDemoAnimation(*this, *DemoAnimation, DemoAnimationTime);
    }

    std::vector<FDemoAnimationClip::FContactEvent> FScene::AdvanceDemoAnimation(float DeltaTime)
    {
        std::vector<FDemoAnimationClip::FContactEvent> Fired;

        if (!DemoAnimation || !bDemoAnimationPlaying || !std::isfinite(DeltaTime) || DeltaTime <= 0.0F)
            return Fired;
        const float PreviousTime = DemoAnimationTime;
        DemoAnimationTime += DeltaTime;

        if (DemoAnimation->bLoop)
            DemoAnimationTime = std::fmod(DemoAnimationTime, DemoAnimation->DurationSeconds);
        else if (DemoAnimationTime >= DemoAnimation->DurationSeconds)
        {
            DemoAnimationTime = DemoAnimation->DurationSeconds;
            bDemoAnimationPlaying = false;
        }

        ApplyDemoAnimation(*this, *DemoAnimation, DemoAnimationTime);

        for (const auto& Event : DemoAnimation->ContactEvents)
        {
            if ((Event.Time > PreviousTime || (PreviousTime == 0.0F && Event.Time == 0.0F)) &&
                Event.Time <= DemoAnimationTime)
                Fired.push_back(Event);
        }

        return Fired;
    }

#pragma endregion

// ============================================================
// 04. Mesh Instance Management
// ============================================================
#pragma region 04_Mesh_Instance_Management

    void FScene::AddStaticMeshInstance(FStaticMeshInstance Instance)
    {
        StaticMeshInstances.push_back(std::move(Instance));
        InitialTransforms.push_back(StaticMeshInstances.back().GetTransform());
        FLogger::Debug(
            "TScene",
            "Static mesh instance added. TScene instance count=" + std::to_string(StaticMeshInstances.size()) + ".");
    }

    std::vector<FStaticMeshInstance>& FScene::GetStaticMeshInstances() noexcept
    {
        return StaticMeshInstances;
    }

    const std::vector<FStaticMeshInstance>& FScene::GetStaticMeshInstances() const noexcept
    {
        return StaticMeshInstances;
    }
#pragma endregion
} // namespace MDSS
