/**
 * @file OverlaySurface.glsl
 * @brief Heat, Lava, WaterFilm, Mud overlay fragment shader들이 공유하는 coverage 기반 조명 경로다.
 */
#ifndef MDSS_OVERLAY_SURFACE
#define MDSS_OVERLAY_SURFACE

#include "Rendering/Surface/Lighting/MaterialParameters.glsl"
#include "Rendering/Surface/Lighting/Lighting.glsl"
#include "Rendering/Surface/Lighting/Effects/Lava.glsl"
#include "Rendering/Surface/Lighting/Effects/WaterFilm.glsl"

#ifdef OVERLAY_COMBINED
#define SURFACE_DEBUG_SET 1
#define RENDER_STATE_TEXTURE_SET 4
#include "Rendering/Surface/StateTexture/RenderStateSampling.glsl"
#include "Rendering/Overlay/Geometry/TexelGeometryData.glsl"

// ============================================================
// 00. Resource Interface
// ============================================================
// #region 00_Resource_Interface

layout(set = 2, binding = 0, std430) readonly buffer TComputedVertices
{
    TTexelGeometryVertex Values[];
} Computed;
#endif

layout(location = 0) in vec3 FragNormal;
layout(location = 1) in vec2 FragUV;
layout(location = 2) flat in uint FragSurfaceIndex;
layout(location = 3) in vec3 FragWorldPosition;
layout(location = 4) in float FragCoverage;

#ifdef OVERLAY_COMBINED
layout(location = 5) flat in uint FragIsSide;
layout(location = 6) flat in mat3 FragNormalMatrix;
#endif

layout(location = 0) out vec4 OutColor;

#ifdef OVERLAY_COMBINED
vec4 SamplePixelOverlayNormal(uint Surface, vec2 UV, vec3 Fallback);

bool SampleSeamPartnerStates(uint Surface, vec2 UV, uvec4 StateChannels, uint Channels,
                             bool UseRenderTexture, out vec4 PartnerStates,
                             out vec3 SeamNormal, out float SeamBlend);

bool HasVisibleOverlayState(vec4 States, uint EnabledEffects);
#endif

#include "Rendering/Overlay/Shading/OverlayAppearance.glsl"


#ifdef OVERLAY_COMBINED
void ShadeCombinedOverlay(float Cutoff);
vec4 SampleCombinedOverlayStates(uint EnabledEffects, bool CoverageSmoothing, bool PrecomputedSmoothing, bool UseRenderTexture);
vec3 ResolveCombinedOverlayNormal(inout vec4 States, uint ShadingDiagnostic, bool UseRenderTexture, out float NormalSampleWeight);
void CompositeOverlayLayers(vec4 States, uint EnabledEffects, float IceCoverage, float Cutoff, vec3 N, vec3 V, uint ShadingDiagnostic, float NormalSampleWeight);
#endif

// #endregion

// ============================================================
// 01. Main Process
// ============================================================
// #region 01_Main_Process

void main()
{
    // 아주 낮은 coverage는 버리고 재질별 branch에서 색, 조명, 투명도를 정한다.
    const float Cutoff = 0.02;
#ifndef OVERLAY_COMBINED
#ifdef OVERLAY_LAVA
    if (FragCoverage <= Material.DemoOptions.y) discard;
#else
    if (FragCoverage <= Cutoff) discard;
#endif
#endif
#ifdef OVERLAY_COMBINED
    ShadeCombinedOverlay(Cutoff);
#elif defined(OVERLAY_LAVA)
    if (FragCoverage <= Material.DemoOptions.y) discard;
    vec3 N = normalize(FragNormal);
    if (!gl_FrontFacing) N = -N;
    vec3 V = normalize(Material.CameraPosition.xyz - FragWorldPosition);
    vec3 Color = ShadeSurface(LavaColor(FragCoverage), N, V,
                              0.34, Material.AmbientLight);
    OutColor = vec4(min(Color + LavaEmission(FragCoverage, Material.DemoOptions.y), vec3(1.0)), 1.0);
#elif defined(OVERLAY_WATER)
    vec3 N = normalize(FragNormal);
    if (!gl_FrontFacing) N = -N;
    vec3 V = normalize(Material.CameraPosition.xyz - FragWorldPosition);
    OutColor = ShadeWaterFilmLayer(N, V, VisibleWaterCoverage(FragCoverage, FragUV));
#else
    vec3 N = normalize(FragNormal);
    if (!gl_FrontFacing) N = -N;
    vec3 V = normalize(Material.CameraPosition.xyz - FragWorldPosition);
    vec3 Color = ShadeSurface(MapEffectColor(1u, FragCoverage), N, V, Material.DemoOptions.z,
                              Material.AmbientLight, 0.0);
    OutColor = vec4(Color, 1.0);
#endif
}
// #endregion

// ============================================================
// 02. Detailed Implementation
// ============================================================
// #region 02_Detailed_Implementation

#ifdef OVERLAY_COMBINED

void ShadeCombinedOverlay(float Cutoff)
{
    uint EnabledEffects = Material.DemoExtraStateChannels.w;
    uint PerformanceFlags = Material.DemoExtraStateChannels.z;
    bool CoverageSmoothing = (PerformanceFlags & 1u) != 0u;

#if defined(MDSS_FRAGMENT_MODE)
    const bool PrecomputedSmoothing = MDSS_FRAGMENT_MODE == 2;
    const bool UseRenderTexture = MDSS_FRAGMENT_MODE != 0;
#else
    bool PrecomputedSmoothing = (PerformanceFlags & 2u) != 0u;
    bool UseRenderTexture = (PerformanceFlags & 4u) != 0u || PrecomputedSmoothing;
#endif

    vec4 States = SampleCombinedOverlayStates(EnabledEffects, CoverageSmoothing, PrecomputedSmoothing, UseRenderTexture);

    float IceCoverage = (EnabledEffects & 32u) != 0u && Material.ExtraDemoBindings.z < Material.StateChannelCount
        ? (CoverageSmoothing
            ? SampleSmoothedStateSaturation(FragSurfaceIndex, FragUV, Material.ExtraDemoBindings.z, Material.StateChannelCount)
            : SampleStateSaturation(FragSurfaceIndex, FragUV, Material.ExtraDemoBindings.z, Material.StateChannelCount)) : 0.0;
    IceCoverage = clamp(IceCoverage, 0.0, 1.0);
    // A render-texture sample can be empty right on an atlas boundary even
    // though the welded overlay vertex remains occupied.
    if (!HasVisibleOverlayState(States, EnabledEffects) && IceCoverage <= Cutoff && FragCoverage <= Cutoff) discard;
    uint ShadingDiagnostic = Material.DemoExtraStateChannels.y;
    float NormalSampleWeight;
    vec3 N = ResolveCombinedOverlayNormal(States, ShadingDiagnostic, UseRenderTexture, NormalSampleWeight);

    if (!HasVisibleOverlayState(States, EnabledEffects) && IceCoverage <= Cutoff) discard;
    if (!gl_FrontFacing) N = -N;

    if (ShadingDiagnostic == 2u)
    {
        OutColor = vec4(N * 0.5 + 0.5, 1.0);
        return;
    }

    if (ShadingDiagnostic == 3u)
    {
        OutColor = gl_FrontFacing ? vec4(0.0, 0.8, 0.0, 1.0) : vec4(0.9, 0.0, 0.0, 1.0);
        return;
    }
    // Only the lighting input changes. Geometry, coverage and the GGX BRDF stay
    // identical, so this mode separates normal variation from the reflection math.
    if (ShadingDiagnostic == 1u) N = vec3(0.0, 0.0, 1.0);
    vec3 V = normalize(Material.CameraPosition.xyz - FragWorldPosition);

    CompositeOverlayLayers(States, EnabledEffects, IceCoverage, Cutoff, N, V, ShadingDiagnostic, NormalSampleWeight);
}

vec4 SampleCombinedOverlayStates(uint EnabledEffects, bool CoverageSmoothing, bool PrecomputedSmoothing, bool UseRenderTexture)
{
    vec4 States = UseRenderTexture
        ? SampleRenderStates(FragSurfaceIndex, FragUV,
                             uvec4(Material.DemoStateChannels.xyz, Material.DemoExtraStateChannels.x),
                             Material.StateChannelCount)
        : SampleStateSaturations(FragSurfaceIndex, FragUV,
                                 uvec4(Material.DemoStateChannels.xyz, Material.DemoExtraStateChannels.x),
                                 Material.StateChannelCount);

    if (CoverageSmoothing && !PrecomputedSmoothing)
    {
        if ((EnabledEffects & 2u) != 0u)
            States.y = SampleSmoothedStateSaturation(FragSurfaceIndex, FragUV,
                                                     Material.DemoStateChannels.y, Material.StateChannelCount);
        if ((EnabledEffects & 4u) != 0u)
            States.z = SampleSmoothedStateSaturation(FragSurfaceIndex, FragUV,
                                                     Material.DemoStateChannels.z, Material.StateChannelCount);
        if ((EnabledEffects & 8u) != 0u)
            States.w = SampleSmoothedStateSaturation(FragSurfaceIndex, FragUV,
                                                     Material.DemoExtraStateChannels.x, Material.StateChannelCount);
    }

    return States;
}

vec3 ResolveCombinedOverlayNormal(inout vec4 States, uint ShadingDiagnostic, bool UseRenderTexture, out float NormalSampleWeight)
{
    vec3 N = normalize(FragNormal);
    NormalSampleWeight = 1.0;

    if ((ShadingDiagnostic == 0u || ShadingDiagnostic == 2u ||
         ShadingDiagnostic == 6u || ShadingDiagnostic == 7u ||
         ShadingDiagnostic == 8u || ShadingDiagnostic == 9u) && FragIsSide == 0u)
    {
        vec4 PixelNormal = SamplePixelOverlayNormal(FragSurfaceIndex, FragUV, N);
        NormalSampleWeight = PixelNormal.w;
        N = PixelNormal.xyz;
    }

    if (FragIsSide == 0u)
    {
        vec4 PartnerStates;
        vec3 SeamNormal;
        float SeamBlend;
        if (SampleSeamPartnerStates(FragSurfaceIndex, FragUV,
                                    uvec4(Material.DemoStateChannels.xyz,
                                          Material.DemoExtraStateChannels.x),
                                    Material.StateChannelCount, UseRenderTexture,
                                    PartnerStates, SeamNormal, SeamBlend))
        {
            States = mix(States, 0.5 * (States + PartnerStates), SeamBlend);
            if (ShadingDiagnostic == 0u || ShadingDiagnostic == 2u ||
                ShadingDiagnostic == 6u || ShadingDiagnostic == 7u ||
                ShadingDiagnostic == 8u || ShadingDiagnostic == 9u)
            {
                // Both charts share the same source surface normal. The
                // computed height normals are independent on either side.
                N = normalize(mix(N, normalize(FragNormalMatrix * SeamNormal), SeamBlend));
                NormalSampleWeight = min(NormalSampleWeight, 1.0 - 0.20 * SeamBlend);
            }
        }
    }

    return N;
}

void CompositeOverlayLayers(vec4 States, uint EnabledEffects, float IceCoverage, float Cutoff, vec3 N, vec3 V, uint ShadingDiagnostic, float NormalSampleWeight)
{
    // The base surface has already been drawn. Emit one straight-alpha color
    // for this shared accumulation surface; water is composited over mud here.
    float MudCoverage = (EnabledEffects & 2u) != 0u ? clamp(States.y, 0.0, 1.0) : 0.0;

    if (ShadingDiagnostic == 7u)
    {
        float WaterCoverage = (EnabledEffects & 4u) != 0u ? clamp(States.z, 0.0, 1.0) : 0.0;
        float LavaCoverage = (EnabledEffects & 8u) != 0u ? clamp(States.w, 0.0, 1.0) : 0.0;
        OutColor = vec4(MudCoverage, LavaCoverage, WaterCoverage, 1.0);
        return;
    }
    float MudAlpha = smoothstep(Cutoff, 0.50, MudCoverage);
    vec3 MudNormal = ShadingDiagnostic == 1u || ShadingDiagnostic == 4u
        ? N : ApplyMudMicroBump(N, FragWorldPosition,
                               Material.DemoEffectOptions.y * smoothstep(0.75, 1.0, NormalSampleWeight));
    // At chart borders the bilinear footprint has fewer valid texels. A broad
    // specular lobe avoids exposing the fallback vertex-normal triangulation.
    float MudRoughness = mix(1.0, Material.DemoOptions.z,
                             smoothstep(0.90, 1.0, NormalSampleWeight));
    // A partly valid bilinear normal footprint at a chart boundary cannot
    // support a stable highlight. Fade it there and compress the remaining
    // GGX peak while preserving weak reflections on the interior.
    float NormalFootprintFade = FragIsSide == 0u
        ? smoothstep(0.75, 1.0, NormalSampleWeight) : 1.0;
    float MudSpecularLimit = 0.015 * NormalFootprintFade *
                             smoothstep(Cutoff, 0.50, MudCoverage);
    vec3 MudColor = ShadeSurface(MapEffectColor(1u, MudCoverage), MudNormal, V,
                                 MudRoughness, Material.AmbientLight, 0.0, MudSpecularLimit);
    if (ShadingDiagnostic == 8u || ShadingDiagnostic == 9u)
    {
        vec3 Specular = ShadeSurface(vec3(0.0), MudNormal, V, MudRoughness, 0.0,
                                    0.0, MudSpecularLimit) *
                        (1.0 - Material.AmbientLight);
        OutColor = ShadingDiagnostic == 8u
            ? vec4(clamp(MudColor - Specular, 0.0, 1.0), 1.0)
            : vec4(clamp(Specular * 4.0, 0.0, 1.0), 1.0);
        return;
    }
    vec3 PremultipliedColor = MudColor * MudAlpha;
    float SurfaceAlpha = MudAlpha;
    if (IceCoverage > Cutoff)
    {
        float IceAlpha = smoothstep(Cutoff, 0.20, IceCoverage);
        // Blue scattering tint, frosted grain and broad dielectric highlights.
        float Grain = 0.7*MudValueNoise(FragWorldPosition*4.0) + 0.3*MudValueNoise(FragWorldPosition*15.0);
        vec3 IceAlbedo = mix(vec3(0.18,0.48,0.66),vec3(0.70,0.89,0.94),0.45+0.35*Grain);
        vec3 IceNormal = ApplyMudMicroBump(N, FragWorldPosition, 0.035);
        vec3 IceColor = ShadeSurface(IceAlbedo, IceNormal, V, 0.24+0.12*Grain, Material.AmbientLight, 0.35, 0.25);
        PremultipliedColor = IceColor*IceAlpha + PremultipliedColor*(1.0-IceAlpha);
        SurfaceAlpha = IceAlpha + SurfaceAlpha*(1.0-IceAlpha);
    }

    float WaterCoverage = (EnabledEffects & 4u) != 0u
        ? VisibleWaterCoverage(clamp(States.z, 0.0, 1.0), FragUV) : 0.0;
    if (WaterCoverage > 0.0)
    {
        vec4 Water = ShadeWaterFilmLayer(N, V, WaterCoverage);
        PremultipliedColor = Water.rgb * Water.a + PremultipliedColor * (1.0 - Water.a);
        SurfaceAlpha = Water.a + SurfaceAlpha * (1.0 - Water.a);
    }

    // Lava keeps its existing highest material priority and remains opaque.
    if ((EnabledEffects & 8u) != 0u && States.w > Material.DemoOptions.y)
    {
        float LavaCoverage = clamp(States.w, 0.0, 1.0);
        vec3 LavaLit = ShadeSurface(LavaColor(LavaCoverage), N, V,
                                    0.34, Material.AmbientLight);
        PremultipliedColor = min(LavaLit + LavaEmission(LavaCoverage, Material.DemoOptions.y), vec3(1.0));
        SurfaceAlpha = 1.0;
    }

    if (SurfaceAlpha <= 1e-5) discard;
    OutColor = vec4(min(PremultipliedColor / SurfaceAlpha, vec3(1.0)),
                    ShadingDiagnostic == 6u ? 1.0 : SurfaceAlpha);
}

vec4 SamplePixelOverlayNormal(uint Surface, vec2 UV, vec3 Fallback)
{
    if (Surface >= uint(SurfaceRanges.Values.length())) return vec4(Fallback, 0.0);
    uvec4 Range = SurfaceRanges.Values[Surface];
    if (Range.y == 0u || Range.z == 0u) return vec4(Fallback, 0.0);

    vec2 UVClamped = clamp(UV, 0.0, 1.0);
    vec2 TexelPosition = UVClamped * vec2(Range.yz) - 0.5;
    ivec2 First = ivec2(floor(TexelPosition));
    vec2 Fraction = fract(TexelPosition);
    uint Anchor = FindSurfaceSamplingAnchor(Surface, UV, Range);

    if (Anchor >= uint(TexelSurfaceIndices.Values.length()) ||
        Anchor >= uint(TexelProfileIndices.Values.length()) ||
        Anchor >= uint(TexelChartIndices.Values.length()) ||
        TexelSurfaceIndices.Values[Anchor] != Surface)
        return vec4(Fallback, 0.0);
    uint Profile = TexelProfileIndices.Values[Anchor];
    uint Chart = TexelChartIndices.Values[Anchor];

    vec3 Sum = vec3(0.0);
    float WeightSum = 0.0;

    for (int Y = 0; Y < 2; ++Y)
    for (int X = 0; X < 2; ++X)
    {
        ivec2 XY = clamp(First + ivec2(X, Y), ivec2(0), ivec2(Range.yz) - 1);
        uint Texel = Range.x + uint(XY.y) * Range.y + uint(XY.x);
        if (Texel >= uint(Computed.Values.length()) ||
            Texel >= uint(TexelSurfaceIndices.Values.length()) ||
            Texel >= uint(TexelProfileIndices.Values.length()) ||
            Texel >= uint(TexelChartIndices.Values.length()) ||
            TexelSurfaceIndices.Values[Texel] != Surface ||
            TexelProfileIndices.Values[Texel] != Profile ||
            TexelChartIndices.Values[Texel] != Chart)
            continue;

        vec3 TexelNormal = Computed.Values[Texel].HeightAndNormal.yzw;
        if (dot(TexelNormal, TexelNormal) < 1e-12) continue;

        float Weight = (X == 0 ? 1.0 - Fraction.x : Fraction.x) *
                       (Y == 0 ? 1.0 - Fraction.y : Fraction.y);
        Sum += Weight * TexelNormal;
        WeightSum += Weight;
    }

    return WeightSum > 1e-6 && dot(Sum, Sum) > 1e-12
        ? vec4(normalize(FragNormalMatrix * normalize(Sum)), clamp(WeightSum, 0.0, 1.0))
        : vec4(Fallback, 0.0);
}

bool SampleSeamPartnerStates(uint Surface, vec2 UV, uvec4 StateChannels, uint Channels,
                             bool UseRenderTexture, out vec4 PartnerStates,
                             out vec3 SeamNormal, out float SeamBlend)
{
    PartnerStates = vec4(0.0);
    SeamNormal = vec3(0.0);
    SeamBlend = 0.0;

    if (Surface >= uint(SurfaceRanges.Values.length())) return false;
    uvec4 Range = SurfaceRanges.Values[Surface];
    if (Range.y == 0u || Range.z == 0u) return false;

    uint Center = FindSurfaceSamplingAnchor(Surface, UV, Range);
    if (Center >= uint(TexelSurfaceIndices.Values.length()) ||
        Center >= uint(TexelProfileIndices.Values.length()) ||
        Center >= uint(TexelChartIndices.Values.length()) ||
        Center >= uint(NeighborIndices.Values.length()) ||
        Center >= uint(Normals.Values.length()) ||
        Center >= uint(Positions.Values.length()) ||
        Center >= uint(RenderSamplingBoundaryFlags.Values.length()) ||
        RenderSamplingBoundaryFlags.Values[Center] == 0u ||
        TexelSurfaceIndices.Values[Center] != Surface) return false;

    uint Profile = TexelProfileIndices.Values[Center];
    uint Chart = TexelChartIndices.Values[Center];
    vec3 BaseNormal = normalize(Normals.Values[Center].xyz);
    vec3 BasePosition = Positions.Values[Center].xyz;
    float WeightSum = 0.0;
    vec3 PartnerNormalSum = vec3(0.0);

    for (uint K = 0u; K < 8u; ++K)
    {
        uint Other = NeighborIndices.Values[Center].Indices[K];
        if (Other >= uint(TexelSurfaceIndices.Values.length()) ||
            Other >= uint(TexelProfileIndices.Values.length()) ||
            Other >= uint(TexelChartIndices.Values.length()) ||
            Other >= uint(Normals.Values.length()) ||
            Other >= uint(Positions.Values.length()) ||
            TexelSurfaceIndices.Values[Other] != Surface ||
            TexelProfileIndices.Values[Other] != Profile ||
            TexelChartIndices.Values[Other] == Chart ||
            dot(BaseNormal, normalize(Normals.Values[Other].xyz)) < 0.5) continue;

        vec3 Delta = Positions.Values[Other].xyz - BasePosition;
        float Weight = 1.0 / max(dot(Delta, Delta), 1e-8);
        uint Local = Other - Range.x;
        ivec3 PartnerXY = ivec3(int(Local % Range.y), int(Local / Range.y), int(Surface));
        vec4 Value = UseRenderTexture
            ? texelFetch(RenderStateTexture, PartnerXY, 0)
            : StateSaturationsForProfile(Other, Channels, Profile, StateChannels);
        PartnerStates += Weight * Value;
        PartnerNormalSum += Weight * normalize(Normals.Values[Other].xyz);
        WeightSum += Weight;
    }

    if (WeightSum <= 0.0) return false;
    PartnerStates /= WeightSum;
    SeamNormal = normalize(BaseNormal + PartnerNormalSum / WeightSum);

    // A fully valid 2x2 bilinear footprint can still lie immediately beside
    // a chart border. Measure the nearest missing chart texel explicitly.
    vec2 TexelPosition = clamp(UV, 0.0, 1.0) * vec2(Range.yz) - 0.5;
    uint Local = Center - Range.x;
    ivec2 CenterXY = ivec2(int(Local % Range.y), int(Local / Range.y));
    float NearestGap = 3.0;

    for (int Y = -1; Y <= 1; ++Y)
    for (int X = -1; X <= 1; ++X)
    {
        ivec2 XY = CenterXY + ivec2(X, Y);
        bool Missing = any(lessThan(XY, ivec2(0))) ||
                       any(greaterThanEqual(XY, ivec2(Range.yz)));
        if (!Missing)
        {
            uint Neighbor = Range.x + uint(XY.y) * Range.y + uint(XY.x);
            Missing = Neighbor >= uint(TexelSurfaceIndices.Values.length()) ||
                      Neighbor >= uint(TexelChartIndices.Values.length()) ||
                      TexelSurfaceIndices.Values[Neighbor] != Surface ||
                      TexelChartIndices.Values[Neighbor] != Chart;
        }
        if (Missing)
            NearestGap = min(NearestGap, length(TexelPosition - vec2(XY)));
    }

    SeamBlend = 1.0 - smoothstep(0.5, 2.0, NearestGap);
    return true;
}

bool HasVisibleOverlayState(vec4 States, uint EnabledEffects)
{
    return ((EnabledEffects & 2u) != 0u && States.y > 0.02) ||
           ((EnabledEffects & 4u) != 0u && States.z > 0.02) ||
           ((EnabledEffects & 8u) != 0u && States.w > Material.DemoOptions.y);
}

#endif
// #endregion

#endif
