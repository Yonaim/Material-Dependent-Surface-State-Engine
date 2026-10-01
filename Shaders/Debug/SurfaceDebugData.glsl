// Shared GPU evaluation for height previews, heatmaps and the Texel Inspector.
// These previews are diagnostic; optional solver feedback builds its own aggregate geometry.
#ifndef MDSS_SURFACE_DEBUG_DATA
#define MDSS_SURFACE_DEBUG_DATA
#ifndef SURFACE_DEBUG_SET
#define SURFACE_DEBUG_SET 1
#endif
#include "Surface/SurfaceStateData.glsl"

struct TDebugAccumulation
{
    float State; float Capacity; float Saturation; float ReferenceAmount;
    float Factor; float CavityFactor; float MesoHeight; float CavityDepth;
    float Fill; float Excess; float CavityHeight; float FollowingHeight;
    float Height; float Area; float AreaScale; float ThicknessPerAmount; float WorldToLocalHeight;
    uint Status; // 0 invalid, 1 unassigned, 2 unsupported, 3 valid, 4 nonfinite/invalid area
};

TDebugAccumulation DebugAccumulation(uint Texel, uint Channel, uint Channels,
                                    float AccumulationDisplayScale, mat3 NormalMatrix)
{
    TDebugAccumulation D = TDebugAccumulation(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0u);
    if (Texel >= uint(TexelSurfaceIndices.Values.length()) ||
        TexelSurfaceIndices.Values[Texel] == 0xffffffffu || Texel >= uint(GeometryScalars.Values.length())) return D;
    D.MesoHeight = GeometryScalars.Values[Texel].MesoVirtualHeight;
    D.CavityDepth = max(-D.MesoHeight, 0.0);
    if (isnan(D.MesoHeight) || isinf(D.MesoHeight)) { D.Status = 4u; return D; }
    D.Status = 1u;
    if (Texel >= uint(TexelProfileIndices.Values.length()) || TexelProfileIndices.Values[Texel] == 0xffffffffu) return D;
    D.Status = 2u;
    if (Channels == 0u || Channel >= Channels) return D;
    uint Profile = TexelProfileIndices.Values[Texel];
    if (Profile >= uint(ProfileSupported.Values.length()) / Channels ||
        Profile >= uint(ProfileParameters.Values.length()) / Channels ||
        Texel >= uint(CurrentState.Values.length()) / Channels) return D;
    uint Record = Profile * Channels + Channel;
    if (ProfileSupported.Values[Record] == 0u) return D;
    TSurfaceGPUProfileParameters P = ProfileParameters.Values[Record];
    D.State = CurrentState.Values[Texel * Channels + Channel];
    D.Factor = P.DecayAndGeometry.z;
    D.CavityFactor = P.DecayAndGeometry.w;
    D.ThicknessPerAmount = P.AccumulationThickness.x;
    D.Status = 4u;
    if (Texel >= uint(WorldTexelAreas.Values.length())) return D;
    D.Area = WorldTexelAreas.Values[Texel];
    D.AreaScale = D.Area * (256.0 * 256.0);
    D.Capacity = P.CapacityInputAndTransfer.x * D.AreaScale;
    if (isnan(D.State) || isinf(D.State) || D.State < 0.0 ||
        isnan(D.AreaScale) || isinf(D.AreaScale) || D.AreaScale <= 0.0 ||
        D.Capacity <= 0.0 || isnan(D.Capacity) || isinf(D.Capacity) ||
        isnan(D.Factor) || isinf(D.Factor) || D.Factor < 0.0 ||
        isnan(D.CavityFactor) || isinf(D.CavityFactor) || D.CavityFactor < 0.0 || D.CavityFactor > 1.0 ||
        isnan(D.ThicknessPerAmount) || isinf(D.ThicknessPerAmount) || D.ThicknessPerAmount < 0.0) return D;
    D.Saturation = D.State / D.Capacity;
    D.WorldToLocalHeight = length(NormalMatrix * normalize(Normals.Values[Texel].xyz));
    if (isnan(D.WorldToLocalHeight) || isinf(D.WorldToLocalHeight) || D.WorldToLocalHeight <= 0.0) return D;
    if (D.State == 0.0)
    {
        D.Status = 3u;
        return D;
    }
    // State is total texel amount. Convert to fixed-reference-area amount for thickness.
    // Capacity bounds the geometry contribution, while the State buffer retains excess for transport.
    D.ReferenceAmount = min(D.State, D.Capacity) / D.AreaScale;
    float Amount = D.ReferenceAmount * D.Factor;
    float CavityAmount = Amount * D.CavityFactor;
    D.Fill = min(CavityAmount, 1.0);
    D.Excess = max(CavityAmount - 1.0, 0.0);
    D.CavityHeight = D.Fill * D.CavityDepth * AccumulationDisplayScale;
    D.FollowingHeight = (Amount * (1.0 - D.CavityFactor) + D.Excess) *
                        D.ThicknessPerAmount * D.WorldToLocalHeight * AccumulationDisplayScale;
    D.Height = D.CavityHeight + D.FollowingHeight;
    if (isnan(D.Height) || isinf(D.Height) || isnan(D.Saturation) || isinf(D.Saturation)) return D;
    D.Status = 3u;
    return D;
}

// Least-squares height gradient in the macro tangent plane. Seam neighbors use mesh-local positions.
vec3 DebugFinalNormal(uint Texel, uint Channel, uint Channels, float AccumulationDisplayScale,
                      mat3 NormalMatrix, float HeightScale)
{
    vec3 Fallback = MesoNormals.Values[Texel].xyz;
    TDebugAccumulation Center = DebugAccumulation(Texel, Channel, Channels, AccumulationDisplayScale, NormalMatrix);
    if (Center.Status == 0u || Center.Status == 4u || Texel >= uint(Positions.Values.length()) ||
        Texel >= uint(Normals.Values.length()) || Texel >= uint(NeighborIndices.Values.length())) return Fallback;
    vec3 N = normalize(Normals.Values[Texel].xyz);
    vec3 U = normalize(cross(abs(N.z) < 0.9 ? vec3(0,0,1) : vec3(0,1,0), N));
    vec3 V = cross(N, U);
    float XX = 0.0, XY = 0.0, YY = 0.0, XH = 0.0, YH = 0.0;
    for (uint Slot = 0u; Slot < 8u; ++Slot)
    {
        uint Other = NeighborIndices.Values[Texel].Indices[Slot];
        if (Other >= uint(Positions.Values.length()) || Other >= uint(Normals.Values.length()) ||
            dot(N, Normals.Values[Other].xyz) < 0.5) continue;
        TDebugAccumulation Neighbor = DebugAccumulation(Other, Channel, Channels, AccumulationDisplayScale, NormalMatrix);
        if (Neighbor.Status == 0u || Neighbor.Status == 4u) continue;
        vec3 Delta = Positions.Values[Other].xyz - Positions.Values[Texel].xyz;
        float X = dot(Delta, U), Y = dot(Delta, V);
        float LengthSquared = X * X + Y * Y;
        if (LengthSquared <= 1e-16) continue;
        float Weight = 1.0 / LengthSquared;
        float DH = (Neighbor.MesoHeight + Neighbor.Height - Center.MesoHeight - Center.Height) * HeightScale;
        XX += X * X * Weight; XY += X * Y * Weight; YY += Y * Y * Weight;
        XH += X * DH * Weight; YH += Y * DH * Weight;
    }
    float Det = XX * YY - XY * XY;
    if (Det <= 1e-6 * max(XX * YY, 1e-12)) return Fallback;
    vec2 Gradient = vec2(YY * XH - XY * YH, XX * YH - XY * XH) / Det;
    return normalize(N - U * Gradient.x - V * Gradient.y);
}
vec3 DebugFinalNormal(uint Texel, uint Channel, uint Channels, float AccumulationDisplayScale, mat3 NormalMatrix)
{
    return DebugFinalNormal(Texel, Channel, Channels, AccumulationDisplayScale, NormalMatrix, 1.0);
}
#endif
