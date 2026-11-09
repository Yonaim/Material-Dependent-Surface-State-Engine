/**
 * @file InputSystem.h
 * @brief 장치 입력 및 디버그 입력에서 Surface 접촉 이벤트를 만드는 인터페이스를 선언한다.
 */

#pragma once

#include "SurfaceState/State/SurfaceInput.h"

#include <cstdint>
#include <optional>

struct GLFWwindow;

namespace MDSS::Asset
{
    class FAssetManager;
}
namespace MDSS::SurfaceState
{
    class FSurfaceDataManager;
}

namespace MDSS
{

    class FCamera;
    class FScene;

    class FInputSystem final
    {
    public:
        // Window input polling
        explicit FInputSystem(GLFWwindow* Window) noexcept;

        /** @brief Convert one Space press into a center-camera ray contact unless text entry or a UI drag is active. */
        [[nodiscard]] std::optional<SurfaceState::FSurfaceContactInput>
        PollDebugContact(const FScene&                            Scene,
                         const Asset::FAssetManager&              Assets,
                         const SurfaceState::FSurfaceDataManager& SurfaceData,
                         const FCamera&                           Camera,
                         bool                                     bInjectMode,
                         SurfaceState::FStateId                   State,
                         float                                    Strength,
                         float                                    Radius,
                         float                                    Falloff,
                         SurfaceState::ESurfaceContactOperation   Operation,
                         SurfaceState::ESurfaceContactShape       Shape,
                         float                                    SecondaryRadius,
                         float                                    RotationDegrees,
                         std::uint32_t                            TexelSearchRadius,
                         bool                                     bHotkeySuppressed);

    private:
        GLFWwindow* Window = nullptr;
        bool        bWasSpaceDown = false;
    };
} // namespace MDSS
