/**
 * @file DemoAnimation.h
 * @brief Versioned Scene demo animation data and keyframe evaluation.
 */

#pragma once

#include <glm/vec4.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace MDSS
{
    class TScene;

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

    struct TDemoAnimationKey
    {
        float Time = 0.0F;
        glm::vec4 Value{0.0F};
    };

    struct TDemoAnimationTrack
    {
        std::string Target;
        EDemoAnimationProperty Property = EDemoAnimationProperty::Position;
        EDemoAnimationInterpolation Interpolation = EDemoAnimationInterpolation::Linear;
        std::vector<TDemoAnimationKey> Keys;
    };

    struct TDemoAnimationClip
    {
        float DurationSeconds = 0.0F;
        bool bLoop = false;
        std::vector<TDemoAnimationTrack> Tracks;
    };

    /** @brief Parse and validate a JSON .DemoAnim against stable Scene object IDs. */
    [[nodiscard]] TDemoAnimationClip LoadDemoAnimation(const std::filesystem::path& Path, const TScene& Scene);
    /** @brief Apply all tracks at the requested clip time without touching simulation state. */
    void ApplyDemoAnimation(TScene& Scene, const TDemoAnimationClip& Clip, float Time);
} // namespace MDSS
