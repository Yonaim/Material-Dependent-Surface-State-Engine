// Profile은 [0, 1] 무차원 계수를 저장하며 실제 속도는 여기서 계산한다.
// CPU transport step bound와 동일한 보정값을 사용한다 (ADR 0033).
#include "SurfaceState/Types/SurfaceSolverRates.h"

const float BaseSaturationTransferRate = MDSS_BASE_SATURATION_TRANSFER_RATE; // State / second

const float BaseGeometryTransferRate = MDSS_BASE_GEOMETRY_TRANSFER_RATE; // State / (world-length * second)

// ============================================================
// 00. Shared Shader Helpers
// ============================================================

uint stateIndex(uint TexelIndex, uint ChannelIndex)
{
    return TexelIndex * Solver.StateChannelCount + ChannelIndex;
}

uint profileRecordIndex(uint TexelIndex, uint ChannelIndex)
{
    return TexelProfileIndices.Values[TexelIndex] * Solver.StateChannelCount + ChannelIndex;
}

bool isValidTexel(uint TexelIndex)
{
    return TexelIndex < Solver.LocalTexelCount &&
           TexelSurfaceIndices.Values[TexelIndex] != InvalidSurfaceId &&
           TexelProfileIndices.Values[TexelIndex] != InvalidSurfaceId;
}

uint neighborIndex(uint TexelIndex, uint DirectionIndex)
{
    return NeighborIndices.Values[TexelIndex * SurfaceNeighborCount + DirectionIndex];
}

uint reverseNeighborDirectionIndex(uint TexelIndex, uint DirectionIndex)
{
    return (ReverseNeighborDirectionIndices.Values[TexelIndex] >> (DirectionIndex * 4u)) & 0xfu;
}

float transferWeight(uint SourceTexel, uint DirectionIndex)
{
    if (!useHalfDynamicWeights())
        return uintBitsToFloat(TransferWeights.Values[SourceTexel * SurfaceNeighborCount + DirectionIndex]);
    uint PairIndex = DirectionIndex >> 1u;
    uint Packed = TransferWeights.Values[SourceTexel * (SurfaceNeighborCount / 2u) + PairIndex];
    vec2 Pair = unpackHalf2x16(Packed);
    return (DirectionIndex & 1u) == 0u ? Pair.x : Pair.y;
}

bool supportsChannel(uint TexelIndex, uint ChannelIndex);

float accumulationHeight(uint TexelIndex)
{
    if (!isValidTexel(TexelIndex)) return 0.0;
    float AreaScale = texelAreaScale(TexelIndex);
    if (AreaScale <= 0.0 || !solverFinite(AreaScale)) return 0.0;
    float CavityAmount = 0.0;
    float FollowingHeightWorld = 0.0;
    float CavityThicknessWeighted = 0.0;
    for (uint ChannelIndex = 0u; ChannelIndex < Solver.StateChannelCount; ++ChannelIndex)
    {
        if (!supportsChannel(TexelIndex, ChannelIndex)) continue;
        uint Record = profileRecordIndex(TexelIndex, ChannelIndex);
        TSurfaceGPUProfileParameters P = loadProfileParameters(Record);
        float Factor = P.DecayAndGeometry.z;
        float CavityFactor = P.DecayAndGeometry.w;
        float ThicknessPerAmount = P.AccumulationThickness.x;
        float State = CurrentState.Values[stateIndex(TexelIndex, ChannelIndex)];
        if (!solverFinite(State) || State <= 0.0 ||
            !solverFinite(Factor) || Factor <= 0.0 ||
            !solverFinite(CavityFactor) ||
            !solverFinite(ThicknessPerAmount) || ThicknessPerAmount < 0.0) continue;
        // 형상에 반영되는 양은 Capacity로 제한하고, 초과량은 전달 계산을 위해 State에 유지한다.
        float Capacity = P.CapacityInputAndTransfer.x * AreaScale;
        if (!solverFinite(Capacity) || Capacity <= 0.0) continue;
        State = min(State, Capacity);
        float Amount = (State / AreaScale) * Factor;
        float CavityContribution = Amount * clamp(CavityFactor, 0.0, 1.0);
        CavityAmount += CavityContribution;
        CavityThicknessWeighted += CavityContribution * ThicknessPerAmount;
        FollowingHeightWorld += Amount * (1.0 - clamp(CavityFactor, 0.0, 1.0)) * ThicknessPerAmount;
    }
    float MesoHeight = GeometryScalars.Values[TexelIndex].MesoVirtualHeight;
    // Fill toward the highest Meso point in this chart, including positive
    // relief. At full fill the Meso term becomes a level surface.
    float FillDepth = max(MesoNormals.Values[TexelIndex].w - MesoHeight, 0.0);
    float CavityHeight = min(CavityAmount, 1.0) * FillDepth;
    // cavity 용량을 초과한 기여는 각 State의 cavity 비중에 따른 두께로 표면 위에 쌓인다.
    if (CavityAmount > 1.0)
        FollowingHeightWorld += (CavityAmount - 1.0) * (CavityThicknessWeighted / CavityAmount);
    mat3 NormalMatrix = mat3(Solver.NormalMatrixAndUpColumns[0].xyz,
                             Solver.NormalMatrixAndUpColumns[1].xyz,
                             Solver.NormalMatrixAndUpColumns[2].xyz);
    vec3 LocalNormal = normalize(Normals.Values[TexelIndex].xyz);
    // local normal 방향 변위를 world geometric normal 방향의 요청 두께와 맞춘다.
    float WorldToLocalHeight = length(NormalMatrix * LocalNormal);
    float FollowingHeight = FollowingHeightWorld * WorldToLocalHeight;
    float Height = CavityHeight + FollowingHeight;
    return !solverFinite(Height) ? 0.0 : max(Height, 0.0);
}

vec3 effectiveLocalPosition(uint TexelIndex)
{
    // 기본 Meso offset에 accumulation height를 선택적으로 더한 local-space 위치다.
    vec3 Position = Positions.Values[TexelIndex].xyz +
                    Normals.Values[TexelIndex].xyz * GeometryScalars.Values[TexelIndex].MesoVirtualHeight;
    if ((Solver.Flags & (1u << 6u)) != 0u)
        Position += Normals.Values[TexelIndex].xyz * accumulationFloat(TexelIndex);
    return Position;
}

vec3 effectiveLocalNormal(uint TexelIndex)
{
    // accumulation 형상이 켜져 있으면 이웃 높이의 least-squares gradient로 normal을 보정한다.
    if ((Solver.Flags & (1u << 6u)) == 0u) return MesoNormals.Values[TexelIndex].xyz;
    vec3 Fallback = MesoNormals.Values[TexelIndex].xyz;
    vec3 N = normalize(Normals.Values[TexelIndex].xyz);
    vec3 U = normalize(cross(abs(N.z) < 0.9 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0), N));
    vec3 V = cross(N, U);
    float CenterHeight = GeometryScalars.Values[TexelIndex].MesoVirtualHeight + accumulationFloat(TexelIndex);
    float XX = 0.0, XY = 0.0, YY = 0.0, XH = 0.0, YH = 0.0;
    for (uint DirectionIndex = 0u; DirectionIndex < SurfaceNeighborCount; ++DirectionIndex)
    {
        uint Other = neighborIndex(TexelIndex, DirectionIndex);
        if (Other == InvalidTexelIndex || Other >= Solver.LocalTexelCount || !isValidTexel(Other) ||
            dot(N, Normals.Values[Other].xyz) < 0.5) continue;
        vec3 Delta = Positions.Values[Other].xyz - Positions.Values[TexelIndex].xyz;
        float X = dot(Delta, U), Y = dot(Delta, V);
        float LengthSquared = X * X + Y * Y;
        if (LengthSquared <= 1.0e-16) continue;
        float OtherHeight = GeometryScalars.Values[Other].MesoVirtualHeight + accumulationFloat(Other);
        float Weight = 1.0 / LengthSquared;
        float DH = OtherHeight - CenterHeight;
        XX += X * X * Weight; XY += X * Y * Weight; YY += Y * Y * Weight;
        XH += X * DH * Weight; YH += Y * DH * Weight;
    }
    float Det = XX * YY - XY * XY;
    if (Det <= 1.0e-6 * max(XX * YY, 1.0e-12)) return Fallback;
    vec2 Gradient = vec2(YY * XH - XY * YH, XX * YH - XY * XH) / Det;
    vec3 Result = normalize(N - U * Gradient.x - V * Gradient.y);
    return !solverFinite(Result) ? Fallback : Result;
}

float concavityWeight(uint TexelIndex)
{
    // 동적 형상이 꺼져 있으면 전처리 값을, 켜져 있으면 Geometry Update cache를 사용한다.
    return (Solver.Flags & (1u << 6u)) != 0u
               ? DynamicConcavityWeights.Values[TexelIndex]
               : GeometryScalars.Values[TexelIndex].ConcavityWeight;
}

bool supportsChannel(uint TexelIndex, uint ChannelIndex)
{
    // 유효 texel이고 양의 면적이며, 해당 texel의 profile이 channel을 지원해야 한다.
    if (!isValidTexel(TexelIndex) || ChannelIndex >= Solver.StateChannelCount)
    {
        return false;
    }
    return WorldTexelAreas.Values[TexelIndex] > 0.0 &&
           ProfileSupported.Values[profileRecordIndex(TexelIndex, ChannelIndex)] != 0u;
}

float stateCapacity(uint TexelIndex, uint ChannelIndex)
{
    return profileCapacityInputAndTransfer(profileRecordIndex(TexelIndex, ChannelIndex)).x *
           texelAreaScale(TexelIndex);
}

float saturation(uint TexelIndex, uint ChannelIndex)
{
    float Capacity = stateCapacity(TexelIndex, ChannelIndex);
    // Capacity는 포화 기준량이다. 보유량이 Capacity를 넘으면 초과 비율도 이웃 전달량에 반영한다.
    return CurrentState.Values[stateIndex(TexelIndex, ChannelIndex)] / Capacity;
}

float decayAmountPrepared(uint TexelIndex, float Current,
                          TSurfaceGPUProfileParameters Parameters, float AreaScale)
{
    if ((Solver.Flags & (1u << 2u)) != 0u)
    {
        return 0.0;
    }
    float DecayRate = Parameters.DecayAndGeometry.x;
    float CavityRetentionFactor = Parameters.DecayAndGeometry.y;
    float ConcavityWeight = concavityWeight(TexelIndex);
    float CavityFill = clamp(min(Current / AreaScale, Parameters.CapacityInputAndTransfer.x) *
                             Parameters.DecayAndGeometry.z * Parameters.DecayAndGeometry.w, 0.0, 1.0);
    float Retention = (Solver.Flags & (1u << 3u)) != 0u
                          ? 1.0
                          : 1.0 - ConcavityWeight * CavityRetentionFactor * (1.0 - CavityFill);
    return min(Current, max(0.0, DecayRate * AreaScale * Retention * Solver.DeltaTime));
}

float decayAmount(uint TexelIndex, uint ChannelIndex)
{
    uint RecordIndex = profileRecordIndex(TexelIndex, ChannelIndex);
    TSurfaceGPUProfileParameters Parameters = loadProfileParameters(RecordIndex);
    float Current = CurrentState.Values[stateIndex(TexelIndex, ChannelIndex)];
    return decayAmountPrepared(TexelIndex, Current, Parameters, texelAreaScale(TexelIndex));
}

// Pass 1에서는 source마다 한 번 준비해 모든 이웃과 channel 계산에 재사용한다.
// Pass 2는 유입 source의 형상을 준비해 방향별 flux를 다시 계산한다.
mat3 SolverModelLinear;
