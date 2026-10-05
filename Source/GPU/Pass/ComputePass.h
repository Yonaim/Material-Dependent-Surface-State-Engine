/**
 * @file ComputePass.h
 * @brief 향후 공통 Compute 기록 패턴을 둘 GPU Pass 확장 지점.
 */

#pragma once

namespace MDSS::GPU
{
    /**
     * @brief 아직 공통 실행 계약이 정해지지 않은 Compute Pass 확장 지점.
     *
     * SurfaceState solver처럼 도메인 고유 dispatch와 barrier는 각 기능이 계속 소유한다.
     */
    class TComputePass final
    {
    };
} // namespace MDSS::GPU
