/**
 * @file InputSystem.h
 * @brief 장치 입력 및 디버그 입력에서 Surface 접촉 이벤트를 만드는 인터페이스를 선언한다.
 */

#pragma once

#include "SurfaceStateSystem/State/SurfaceInput.h"

#include <cstdint>
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

        /** @brief Convert one Space press into a center-camera ray contact unless text entry or a UI drag is active. */
        [[nodiscard]] std::optional<TSurfaceContactInput> PollDebugContact(const TScene&        Scene,
                                                                           const TAssetManager& Assets,
                                                                           const TCamera&       Camera,
                                                                           bool                 bInjectMode,
                                                                           TStateId             State,
                                                                           float                Strength,
                                                                           float                Radius,
                                                                           float                Falloff,
                                                                           std::uint32_t        TexelSearchRadius,
                                                                           bool                 bHotkeySuppressed);

    private:
        GLFWwindow* Window = nullptr;
        bool        bWasSpaceDown = false;
    };
} // namespace MDSS
