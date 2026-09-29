// Area-correct, profile-aware read adapter. Unsupported/absent States contribute zero.
#include "Surface/SurfaceStateData.glsl"
layout(std430, set = SURFACE_DEBUG_SET, binding = 13) readonly buffer TSurfaceTexelChartIndices { uint Values[]; } TexelChartIndices;
float StateSaturation(uint Texel, uint Channel, uint Channels, uint Surface)
{
    if (Channels == 0u || Channel >= Channels || Texel >= uint(TexelSurfaceIndices.Values.length()) ||
        TexelSurfaceIndices.Values[Texel] != Surface || Texel >= uint(TexelProfileIndices.Values.length())) return 0.0;
    uint Profile = TexelProfileIndices.Values[Texel];
    if (Profile >= uint(ProfileSupported.Values.length()) / Channels ||
        Profile >= uint(ProfileParameters.Values.length()) / Channels ||
        Texel >= uint(CurrentState.Values.length()) / Channels || Texel >= uint(WorldTexelAreas.Values.length())) return 0.0;
    uint Record = Profile * Channels + Channel;
    if (ProfileSupported.Values[Record] == 0u) return 0.0;
    float Capacity = ProfileParameters.Values[Record].CapacityInputAndTransfer.x * WorldTexelAreas.Values[Texel] * (256.0 * 256.0);
    float Amount = CurrentState.Values[Texel * Channels + Channel];
    if (Capacity <= 0.0 || isnan(Capacity) || isinf(Capacity) || Amount < 0.0 || isnan(Amount) || isinf(Amount)) return 0.0;
    return clamp(Amount / Capacity, 0.0, 1.0);
}
float SampleStateSaturation(uint Surface, vec2 UV, uint Channel, uint Channels)
{
    if (Surface >= uint(SurfaceRanges.Values.length()) || Channel >= Channels) return 0.0;
    uvec4 Range = SurfaceRanges.Values[Surface];
    if (Range.y == 0u || Range.z == 0u) return 0.0;
    uvec2 CenterXY = min(uvec2(clamp(UV, 0.0, 1.0) * vec2(Range.yz)), Range.yz - 1u);
    uint Center = Range.x + CenterXY.y * Range.y + CenterXY.x;
    if (Center >= uint(TexelSurfaceIndices.Values.length()) || TexelSurfaceIndices.Values[Center] != Surface ||
        Center >= uint(TexelChartIndices.Values.length()) || Center >= uint(TexelProfileIndices.Values.length())) return 0.0;
    uint Profile = TexelProfileIndices.Values[Center];
    if (Channels == 0u || Profile >= uint(ProfileSupported.Values.length()) / Channels ||
        ProfileSupported.Values[Profile * Channels + Channel] == 0u) return 0.0;
    uint Chart = TexelChartIndices.Values[Center];
    // Bilinear weights stay within the center's chart and assigned profile. Others contribute zero.
    vec2 Pixel = clamp(UV, 0.0, 1.0) * vec2(Range.yz) - 0.5;
    ivec2 Base = ivec2(floor(Pixel));
    vec2 F = fract(Pixel);
    float Value = 0.0;
    for (int Y = 0; Y < 2; ++Y) for (int X = 0; X < 2; ++X)
    {
        ivec2 XY = clamp(Base + ivec2(X,Y), ivec2(0), ivec2(Range.yz) - 1);
        float Weight = (X == 0 ? 1.0-F.x : F.x) * (Y == 0 ? 1.0-F.y : F.y);
        uint T = Range.x + uint(XY.y) * Range.y + uint(XY.x);
        if (T < uint(TexelChartIndices.Values.length()) && TexelChartIndices.Values[T] == Chart &&
            T < uint(TexelProfileIndices.Values.length()) && TexelProfileIndices.Values[T] == Profile)
            Value += Weight * StateSaturation(T, Channel, Channels, Surface);
    }
    return Value;
}
