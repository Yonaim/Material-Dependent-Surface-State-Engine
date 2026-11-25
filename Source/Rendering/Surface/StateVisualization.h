/**
 * @file StateVisualization.h
 * @brief 저작 상태와 표면 진단에서 읽는 계산 슬롯을 연결한다.
 */
#pragma once

#include <glm/vec3.hpp>
#include <string>

namespace MDSS::Rendering
{
    // 선택자는 화면 이름과 독립적인 GPU 읽기 계약이다.
    enum class ESurfaceStateChannel
    {
        LiquidMass,
        IceMass,
        AbsorbedMass,
        CharFraction,
        Temperature,
        Count,
        FirstFilmMass = 16,
        FirstFilmSolidMass = 64,
        FirstFilmWaterMass = 80,
        FirstFilmWaterFraction = 96
    };

    struct FStateVisualization
    {
        bool operator==(const FStateVisualization &) const = default;

        ESurfaceStateChannel Channel;
        std::string          Id;
        std::string          Name;
        std::string          DisplayUnit;
        glm::vec3            Color;
        float                DisplayMaximum;
        bool                 bSubstrate;
        float                ThicknessDisplayMaximumMm = 0.F;

        [[nodiscard]] bool HasFilmMass() const noexcept
        {
            return Channel >= ESurfaceStateChannel::FirstFilmMass && Channel < ESurfaceStateChannel::FirstFilmSolidMass;
        }

        [[nodiscard]] bool HasArealMass() const noexcept
        {
            return Channel == ESurfaceStateChannel::LiquidMass || Channel == ESurfaceStateChannel::IceMass ||
                   Channel == ESurfaceStateChannel::AbsorbedMass || HasFilmMass() ||
                   (Channel >= ESurfaceStateChannel::FirstFilmSolidMass &&
                    Channel < ESurfaceStateChannel::FirstFilmWaterFraction);
        }

        [[nodiscard]] bool HasLayerThickness() const noexcept
        {
            return Channel == ESurfaceStateChannel::LiquidMass || Channel == ESurfaceStateChannel::IceMass ||
                   HasFilmMass();
        }
    };
} // namespace MDSS::Rendering
