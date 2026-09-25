/**
 * @file SurfaceInput.h
 * @brief Surface State에 전달하는 discrete contact 입력 데이터.
 */

#pragma once

#include "SurfaceStateSystem/Types/SurfaceStateTypes.h"

#include <glm/glm.hpp>

namespace MDSS
{
    /**
     * @brief 한 Surface instance와 채널에 대한 discrete contact event.
     * @note 위치·방향·반경을 texel-space InputDelta로 변환하는 것은 solver의 책임이다.
     */
    struct SurfaceContactInput
    {
        SurfaceInstanceID TargetInstance = InvalidSurfaceInstanceID;
        StateId           State = InvalidStateId;
        glm::vec3         WorldPosition{0.0F};
        glm::vec3         WorldDirection{0.0F};
        float             Radius = 0.0F;
        float             Strength = 0.0F;
        float             Falloff = 1.0F;
    };
} // namespace MDSS
