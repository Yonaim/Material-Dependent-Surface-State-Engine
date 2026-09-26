/**
 * @file SurfaceInput.h
 * @brief Surface State에 전달하는 discrete contact 입력 데이터.
 */

#pragma once

#include "SurfaceStateSystem/Types/SurfaceStateTypes.h"

#include <glm/glm.hpp>

#include <cstdint>

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
        glm::vec3         WorldPosition{0.0F};
        glm::vec3         WorldDirection{0.0F};
        float             Radius = 0.0F;
        float             Strength = 0.0F;
        float             Falloff = 1.0F;
        bool              bHasSimulationMapping = false;
        std::uint32_t     TargetTriangle = 0;
        glm::vec2         SimulationUV{0.0F};
    };
} // namespace MDSS
