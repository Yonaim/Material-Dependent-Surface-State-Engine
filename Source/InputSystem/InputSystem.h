/**
 * @file InputSystem.h
 * @brief Device/debug input adapters that produce Surface contact events.
 */

#pragma once

#include "SurfaceStateSystem/State/SurfaceInput.h"

#include <optional>

struct GLFWwindow;

namespace MDSS
{
    class TAssetManager;
    class TCamera;
    class TScene;

    class TInputSystem final
    {
    public:
        explicit TInputSystem(GLFWwindow* Window) noexcept;

        /** @brief Convert one uncaptured Space press into a center-camera ray contact, if one hits a Surface. */
        [[nodiscard]] std::optional<TSurfaceContactInput> PollDebugContact(const TScene& Scene,
                                                                           const TAssetManager& Assets,
                                                                           const TCamera& Camera,
                                                                           bool bInjectMode,
                                                                           TStateId State,
                                                                           float Strength,
                                                                           bool bKeyboardCaptured);

    private:
        GLFWwindow* Window = nullptr;
        bool        bWasSpaceDown = false;
    };
} // namespace MDSS
