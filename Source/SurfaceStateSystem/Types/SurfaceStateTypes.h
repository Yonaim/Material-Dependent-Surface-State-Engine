/**
 * @file SurfaceStateTypes.h
 * @brief 상태 채널, profile parameter, transition과 검증 계약.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace MDSS
{
    /**
     * @brief Profile과 State 배열이 공유하는 기본 채널 순서.
     * @note Count는 배열 크기 용도이며 실제 상태 채널이 아니다.
     */
    enum class SurfaceStateChannel : std::uint32_t
    {
        Wetness = 0,
        Heat,
        Burn,
        Mud,
        Count
    };

    inline constexpr std::size_t SurfaceStateChannelCount = static_cast<std::size_t>(SurfaceStateChannel::Count);

    using SurfaceProfileIndex = std::uint32_t;
    using SurfaceInstanceID = std::uint32_t;

    inline constexpr std::uint32_t     InvalidSurfaceProfileIndex = 0xFFFFFFFFU;
    inline constexpr SurfaceInstanceID InvalidSurfaceInstanceID = 0xFFFFFFFFU;

    /**
     * @brief 한 State channel의 입력·전파·감쇠·적층 parameter.
     * @note ValidateSurfaceResponseProfileData가 finite/non-negative, positive capacity 및 unit interval을 검사한다.
     */
    struct SurfaceStateParameters
    {
        float StateCapacity = 1.0F;
        float InputFactor = 1.0F;
        float SaturationTransferRate = 0.0F;
        float GeometryTransferRate = 0.0F;
        float DecayRate = 0.0F;
        float CavityRetentionFactor = 0.0F;
        float AccumulationFactor = 0.0F;
        float CavityFillFactor = 0.0F;
    };

    /** @brief source Saturation 임계값에 따른 source-to-target 상태 전이. */
    struct SurfaceStateTransition
    {
        SurfaceStateChannel Source = SurfaceStateChannel::Wetness;
        SurfaceStateChannel Target = SurfaceStateChannel::Wetness;
        float               Threshold = 0.0F;
        float               TransitionRate = 0.0F;
    };

    /**
     * @brief 한 Profile의 네 채널별 parameter와 상태 전이 규칙.
     * @note States의 index는 SurfaceStateChannel 값과 일치한다.
     */
    struct SurfaceResponseProfileData
    {
        std::array<SurfaceStateParameters, SurfaceStateChannelCount> States{};
        std::vector<SurfaceStateTransition>                          Transitions;
    };

    using SurfaceStateValues = std::array<float, SurfaceStateChannelCount>;

    /**
     * @brief 채널 enum에 대응하는 소문자 SRProfile 이름을 반환한다.
     * @param Channel 이름을 얻을 유효 채널.
     * @return 정적 수명의 채널 이름 문자열.
     * @throws std::invalid_argument Channel이 실제 채널이 아닌 경우.
     */
    [[nodiscard]] std::string_view GetSurfaceStateChannelName(SurfaceStateChannel Channel);

    /**
     * @brief 소문자 SRProfile 이름을 채널 enum으로 변환한다.
     * @param Name 변환할 채널 이름.
     * @throws std::invalid_argument Name이 알려진 네 채널 중 하나가 아닌 경우.
     */
    [[nodiscard]] SurfaceStateChannel ParseSurfaceStateChannel(std::string_view Name);

    /**
     * @brief 채널 enum을 State 배열 index로 변환한다.
     * @param Channel index를 구할 유효 채널.
     * @throws std::invalid_argument Channel이 실제 채널이 아닌 경우.
     */
    [[nodiscard]] std::size_t GetSurfaceStateChannelIndex(SurfaceStateChannel Channel);

    /**
     * @brief Profile의 모든 State parameter와 transition 불변조건을 검사한다.
     * @param Data 검증할 Profile 데이터.
     * @throws std::invalid_argument 값이 유한하지 않거나 허용 범위를 벗어나면 발생한다.
     */
    void ValidateSurfaceResponseProfileData(const SurfaceResponseProfileData& Data);
} // namespace MDSS
