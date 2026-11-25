/**
 * @file Heightfield.h
 * @brief 표면의 로컬 차트에 놓인 높이 표본을 정의한다.
 */

#pragma once

#include <vector>

namespace MDSS::Liquid
{
    // ============================================================
    // 00. Heightfield
    // ============================================================
    #pragma region 00_Heightfield
        // 균일한 XY 격자에서 셀 경계와 높이 표본에 접근한다.

        /** @brief 로컬 XY 차트의 가운데가 원점이다. 너비, 길이, 샘플 높이는 미터 단위다. */
        struct FHeightfield
        {
            int                Columns = 0;
            int                Rows = 0;
            float              Width = 0.F;
            float              Length = 0.F;
            std::vector<float> Heights;

            [[nodiscard]] float PitchX() const
            {
                return Width / static_cast<float>(Columns);
            }
            [[nodiscard]] float PitchY() const
            {
                return Length / static_cast<float>(Rows);
            }
            [[nodiscard]] float EdgeX(int Column) const
            {
                return Column * PitchX() - Width * 0.5F;
            }
            [[nodiscard]] float EdgeY(int Row) const
            {
                return Row * PitchY() - Length * 0.5F;
            }
            [[nodiscard]] float Height(int X, int Y) const
            {
                return Heights[Y * Columns + X];
            }
        };
    #pragma endregion

} // namespace MDSS::Liquid
