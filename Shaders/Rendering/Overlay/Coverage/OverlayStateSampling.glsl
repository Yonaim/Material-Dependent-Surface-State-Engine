// OverlayStateSampling.glsl — overlay Profile의 State saturation을 경계·coverage 조건에 맞춰 샘플링한다.
#ifndef MDSS_OVERLAY_STATE_SAMPLING
#define MDSS_OVERLAY_STATE_SAMPLING

struct TOverlayGPUProfileParameters
{
    vec4 CapacityInputAndTransfer;
    vec4 DecayAndGeometry;
    vec4 AccumulationThickness;
};

// ============================================================
// 00. Resource Interface
// ============================================================
// #region 00_Resource_Interface
// CPU가 전달하는 리소스 선언과 shader의 접근 경계를 모아 ABI 대응을 확인한다.

    // 00.00. Profile Bindings and Capacity Access
    // #region 00_00_Profile_Bindings_and_Capacity_Access
    // OverlayProfileParameters 버퍼의 값을 읽어 이 구획의 수치 계산에 사용한다.

    layout(std430, set = 0, binding = 1) readonly buffer TOverlayTexelProfileIndices
    {
        uint Values[];
    }

    OverlayTexelProfileIndices;

    layout(std430, set = 0, binding = 6) readonly buffer TOverlayProfileParameters
    {
        vec4 Values[];
    }

    OverlayProfileParameters;

    uint OverlayProfileRecordCount()
    {
        return uint(OverlayProfileParameters.Values.length()) / 3u;
    }

    vec4 OverlayProfileCapacityInputAndTransfer(uint Record)
    {
        return OverlayProfileParameters.Values[Record];
    }

    // #endregion

    // 00.01. State Mapping and Saturation Sampling
    // #region 00_01_State_Mapping_and_Saturation_Sampling
    // OverlayWorldTexelAreas·OverlayCurrentState·OverlayTexelProfileIndices 버퍼의 값을 읽어 이 구획의 수치 계산에 사용한다.

    layout(std430, set = 0, binding = 8) readonly buffer TOverlayCurrentState
    {
        float Values[];
    }

    OverlayCurrentState;

    layout(std430, set = 0, binding = 12) readonly buffer TOverlaySurfaceRanges
    {
        uvec4 Values[];
    }

    OverlaySurfaceRanges;

    layout(std430, set = 0, binding = 13) readonly buffer TOverlayTexelChartIndices
    {
        uint Values[];
    }

    OverlayTexelChartIndices;

    layout(std430, set = 0, binding = 19) readonly buffer TOverlayWorldTexelAreas
    {
        float Values[];
    }

    OverlayWorldTexelAreas;

    float OverlayStateSaturation(uint Texel, uint Channel, uint Channels, uint Profile)
    {
        if (Channels == 0u || Channel >= Channels)
        {
            return 0.0;
        }
    #if MDSS_GPU_VALIDATION
        if (Texel >= uint(OverlayWorldTexelAreas.Values.length()) ||
            Texel >= uint(OverlayCurrentState.Values.length()) / Channels ||
            Profile >= OverlayProfileRecordCount() / Channels || Texel >= uint(OverlayTexelProfileIndices.Values.length()))
        {
            return 0.0;
        }
    #endif
        float Capacity = OverlayProfileCapacityInputAndTransfer(Profile * Channels + Channel).x *
                         OverlayWorldTexelAreas.Values[Texel] * (256.0 * 256.0);
        float Amount = OverlayCurrentState.Values[Texel * Channels + Channel];

        if (Capacity <= 0.0 || Amount < 0.0)
        {
            return 0.0;
        }

        return clamp(Amount / Capacity, 0.0, 1.0);
    }

    // #endregion

// #endregion
#endif
