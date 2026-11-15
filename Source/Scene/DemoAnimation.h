/**
 * @file DemoAnimation.h
 * @brief 버전이 지정된 Scene demo animation 자료형과 keyframe 평가 인터페이스를 선언한다.
 */

#pragma once

#include "SurfaceState/State/SurfaceInput.h"

#include <cstdint>
#include <filesystem>
#include <glm/vec4.hpp>
#include <optional>
#include <string>
#include <vector>

namespace MDSS
{
    class FScene;

    enum class EDemoAnimationProperty : std::uint8_t
    {
        Position,
        Rotation,
        Scale,
        CameraPosition,
        CameraTarget,
    };

    enum class EDemoAnimationInterpolation : std::uint8_t
    {
        Step,
        Linear,
        Slerp,
    };

    struct FDemoAnimationKey
    {
        float     Time = 0.0F;
        glm::vec4 Value{0.0F};
    };

    struct FDemoAnimationTrack
    {
        std::string                    Target;
        EDemoAnimationProperty         Property = EDemoAnimationProperty::Position;
        EDemoAnimationInterpolation    Interpolation = EDemoAnimationInterpolation::Linear;
        std::vector<FDemoAnimationKey> Keys;
    };

    struct FDemoAnimationClip
    {
        struct FWeatherCue
        {
            float RainStartSeconds = 0.0F;
            float RainEndSeconds = 0.0F;
            float CloudEndSeconds = 0.0F;
            float Intensity = 0.0F;
        };
        float                            DurationSeconds = 0.0F;
        bool                             bLoop = false;
        bool                             bAutoplay = false;
        std::vector<FDemoAnimationTrack> Tracks;
        std::optional<FWeatherCue>       Weather;
        struct FContactEvent
        {
            float                                  Time = 0.0F;
            std::string                            Target;
            std::string                            State;
            glm::vec3                              LocalPosition{0.0F};
            glm::vec3                              LocalNormal{0.0F, 0.0F, 1.0F};
            SurfaceState::ESurfaceContactOperation Operation = SurfaceState::ESurfaceContactOperation::Add;
            SurfaceState::ESurfaceContactShape     Shape = SurfaceState::ESurfaceContactShape::Circle;
            float                                  Radius = 0.0F;
            float                                  SecondaryRadius = 0.0F;
            float                                  RotationDegrees = 0.0F;
            float                                  Depth = 0.0F;
            float                                  Strength = 0.0F;
            float                                  Falloff = 1.0F;
        };
        std::vector<FContactEvent> ContactEvents;
    };

    /** @brief Parse and validate a JSON .DemoAnim against stable Scene object IDs. */
    // Animation loading and evaluation
    [[nodiscard]] FDemoAnimationClip LoadDemoAnimation(const std::filesystem::path& Path, const FScene& Scene);
    /** @brief Apply all tracks at the requested clip time without touching simulation state. */
    void ApplyDemoAnimation(FScene& Scene, const FDemoAnimationClip& Clip, float Time);
} // namespace MDSS
