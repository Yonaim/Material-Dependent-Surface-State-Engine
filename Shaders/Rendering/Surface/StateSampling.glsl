/**
 * @file StateSampling.glsl
 * @brief texel 면적과 profile 지원 여부를 반영해 State를 읽는 공통 함수다. 할당되지 않았거나 지원되지 않는 State는 0으로 처리한다.
 */
#include "Surface/SurfaceStateData.glsl"

layout(std430, set = SURFACE_DEBUG_SET, binding = 13) readonly buffer TSurfaceTexelChartIndices { uint Values[]; } TexelChartIndices;

float StateSaturationForProfile(uint Texel, uint Channel, uint Channels, uint Profile)
{
    // 호출자가 이미 검증한 Surface/Profile/chart mapping을 재사용하는 neighbor sampling 경로다.
    if (Channels == 0u || Channel >= Channels) return 0.0;
#if MDSS_GPU_VALIDATION
    if (Texel >= uint(WorldTexelAreas.Values.length()) ||
        Texel >= uint(CurrentState.Values.length()) / Channels ||
        Profile >= uint(ProfileParameters.Values.length()) / Channels) return 0.0;
#endif
    float Capacity = ProfileParameters.Values[Profile * Channels + Channel].CapacityInputAndTransfer.x *
                     WorldTexelAreas.Values[Texel] * (256.0 * 256.0);
    float Amount = CurrentState.Values[Texel * Channels + Channel];
#if MDSS_GPU_VALIDATION
    if (Capacity <= 0.0 || isnan(Capacity) || isinf(Capacity) || Amount < 0.0 || isnan(Amount) || isinf(Amount)) return 0.0;
#else
    if (Capacity <= 0.0 || Amount < 0.0) return 0.0;
#endif
    return clamp(Amount / Capacity, 0.0, 1.0);
}

float StateSaturation(uint Texel, uint Channel, uint Channels, uint Surface)
{
    // 총량을 profile capacity로 나눠 [0, 1] 포화도로 변환한다.
    if (Channels == 0u || Channel >= Channels) return 0.0;
#if MDSS_GPU_VALIDATION
    if (Texel >= uint(TexelSurfaceIndices.Values.length()) ||
        Texel >= uint(TexelProfileIndices.Values.length())) return 0.0;
#endif
    if (TexelSurfaceIndices.Values[Texel] != Surface) return 0.0;
    uint Profile = TexelProfileIndices.Values[Texel];
#if MDSS_GPU_VALIDATION
    if (Profile >= uint(ProfileSupported.Values.length()) / Channels ||
        Profile >= uint(ProfileParameters.Values.length()) / Channels ||
        Texel >= uint(CurrentState.Values.length()) / Channels || Texel >= uint(WorldTexelAreas.Values.length())) return 0.0;
#endif
    uint Record = Profile * Channels + Channel;
    if (ProfileSupported.Values[Record] == 0u) return 0.0;
    return StateSaturationForProfile(Texel, Channel, Channels, Profile);
}

float SampleStateSaturation(uint Surface, vec2 UV, uint Channel, uint Channels)
{
    // 중심 texel의 chart와 profile 내부에서만 bilinear sample을 누적한다.
    if (Channel >= Channels) return 0.0;
#if MDSS_GPU_VALIDATION
    if (Surface >= uint(SurfaceRanges.Values.length())) return 0.0;
#endif
    uvec4 Range = SurfaceRanges.Values[Surface];
    if (Range.y == 0u || Range.z == 0u) return 0.0;
    uvec2 CenterXY = min(uvec2(clamp(UV, 0.0, 1.0) * vec2(Range.yz)), Range.yz - 1u);
    uint Center = Range.x + CenterXY.y * Range.y + CenterXY.x;
#if MDSS_GPU_VALIDATION
    if (Center >= uint(TexelSurfaceIndices.Values.length()) ||
        Center >= uint(TexelChartIndices.Values.length()) || Center >= uint(TexelProfileIndices.Values.length())) return 0.0;
#endif
    if (TexelSurfaceIndices.Values[Center] != Surface) return 0.0;
    uint Profile = TexelProfileIndices.Values[Center];
#if MDSS_GPU_VALIDATION
    if (Channels == 0u || Profile >= uint(ProfileSupported.Values.length()) / Channels ||
        ProfileSupported.Values[Profile * Channels + Channel] == 0u) return 0.0;
#else
    if (ProfileSupported.Values[Profile * Channels + Channel] == 0u) return 0.0;
#endif
    uint Chart = TexelChartIndices.Values[Center];
    // 중심 texel과 chart/profile이 다른 이웃은 bilinear 합산에서 제외한다.
    vec2 Pixel = clamp(UV, 0.0, 1.0) * vec2(Range.yz) - 0.5;
    ivec2 Base = ivec2(floor(Pixel));
    vec2 F = fract(Pixel);
    float Value = 0.0;
    for (int Y = 0; Y < 2; ++Y) for (int X = 0; X < 2; ++X)
    {
        ivec2 XY = clamp(Base + ivec2(X,Y), ivec2(0), ivec2(Range.yz) - 1);
        float Weight = (X == 0 ? 1.0-F.x : F.x) * (Y == 0 ? 1.0-F.y : F.y);
        uint T = Range.x + uint(XY.y) * Range.y + uint(XY.x);
#if MDSS_GPU_VALIDATION
        if (T < uint(TexelChartIndices.Values.length()) && TexelChartIndices.Values[T] == Chart &&
            T < uint(TexelProfileIndices.Values.length()) && TexelProfileIndices.Values[T] == Profile)
#else
        if (TexelChartIndices.Values[T] == Chart && TexelProfileIndices.Values[T] == Profile)
#endif
            Value += Weight * StateSaturationForProfile(T, Channel, Channels, Profile);
    }
    return Value;
}

vec4 StateSaturationsForProfile(uint Texel, uint Channels, uint Profile, uvec4 StateChannels)
{
    // SurfaceLit의 네 demo State가 같은 texel/profile을 읽으므로 metadata는 공유하고 값만 연속해서 읽는다.
    vec4 Values = vec4(0.0);
    for (uint Index = 0u; Index < 4u; ++Index)
    {
        uint Channel = StateChannels[Index];
        if (Channel >= Channels) continue;
#if MDSS_GPU_VALIDATION
        if (Profile >= uint(ProfileSupported.Values.length()) / Channels ||
            Profile >= uint(ProfileParameters.Values.length()) / Channels ||
            Texel >= uint(CurrentState.Values.length()) / Channels ||
            Texel >= uint(WorldTexelAreas.Values.length())) continue;
#endif
        uint Record = Profile * Channels + Channel;
        if (ProfileSupported.Values[Record] == 0u) continue;
        Values[Index] = StateSaturationForProfile(Texel, Channel, Channels, Profile);
    }
    return Values;
}

vec4 SampleStateSaturations(uint Surface, vec2 UV, uvec4 StateChannels, uint Channels)
{
    // 네 채널에 공통인 중심 Surface, chart/profile 경계와 bilinear 가중치를 한 번만 계산한다.
    if (Channels == 0u) return vec4(0.0);
#if MDSS_GPU_VALIDATION
    if (Surface >= uint(SurfaceRanges.Values.length())) return vec4(0.0);
#endif
    uvec4 Range = SurfaceRanges.Values[Surface];
    if (Range.y == 0u || Range.z == 0u) return vec4(0.0);
    uvec2 CenterXY = min(uvec2(clamp(UV, 0.0, 1.0) * vec2(Range.yz)), Range.yz - 1u);
    uint Center = Range.x + CenterXY.y * Range.y + CenterXY.x;
#if MDSS_GPU_VALIDATION
    if (Center >= uint(TexelSurfaceIndices.Values.length()) ||
        Center >= uint(TexelChartIndices.Values.length()) ||
        Center >= uint(TexelProfileIndices.Values.length())) return vec4(0.0);
#endif
    if (TexelSurfaceIndices.Values[Center] != Surface) return vec4(0.0);
    uint Profile = TexelProfileIndices.Values[Center];
    uint Chart = TexelChartIndices.Values[Center];

    vec2 Pixel = clamp(UV, 0.0, 1.0) * vec2(Range.yz) - 0.5;
    ivec2 Base = ivec2(floor(Pixel));
    vec2 F = fract(Pixel);
    vec4 Values = vec4(0.0);
    for (int Y = 0; Y < 2; ++Y) for (int X = 0; X < 2; ++X)
    {
        ivec2 XY = clamp(Base + ivec2(X,Y), ivec2(0), ivec2(Range.yz) - 1);
        float Weight = (X == 0 ? 1.0-F.x : F.x) * (Y == 0 ? 1.0-F.y : F.y);
        uint T = Range.x + uint(XY.y) * Range.y + uint(XY.x);
#if MDSS_GPU_VALIDATION
        if (T >= uint(TexelChartIndices.Values.length()) || T >= uint(TexelProfileIndices.Values.length())) continue;
#endif
        if (TexelChartIndices.Values[T] == Chart && TexelProfileIndices.Values[T] == Profile)
            Values += Weight * StateSaturationsForProfile(T, Channels, Profile, StateChannels);
    }
    return Values;
}
