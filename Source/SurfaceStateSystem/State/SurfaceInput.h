/**
 * @file SurfaceInput.h
 * @brief Surface State에 전달하는 discrete contact 입력 데이터.
 */

#pragma once

#include "SurfaceStateSystem/Types/SurfaceStateTypes.h"

#include <cstdint>
#include <glm/glm.hpp>

namespace MDSS
{
    /**
     * @brief 한 Surface instance와 채널에 대한 discrete contact event.
     * @note 입력 출처가 제출하며 Surface Input 처리 단계가 texel-space InputDelta로 변환한다.
     */
    struct TSurfaceContactInput
    {
        TSurfaceInstanceID TargetInstance = InvalidSurfaceInstanceID;
        TStateId           State = InvalidStateId;
        glm::vec3          WorldPosition{0.0F};
        glm::vec3          WorldDirection{0.0F};
        float              Radius = 0.0F;
        /** @brief One event's amount per fixed SurfaceStateReferenceArea, before falloff and InputFactor. */
        float Strength = 0.0F;
        float Falloff = 1.0F;
        /** @brief UV→texel 중심 보정에서 같은 Triangle을 탐색할 축별 격자 범위. 2이면 기본 ±2 texel이다. */
        std::uint32_t TexelSearchRadius = 2;
        bool          bHasSimulationMapping = false;
        std::uint32_t TargetTriangle = 0;
        glm::vec2     SimulationUV{0.0F};
    };
} // namespace MDSS
