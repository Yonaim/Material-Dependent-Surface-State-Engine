// ============================================================
// 00. Shared Shader Helpers
// ============================================================
// #region 00_Shared_Shader_Helpers

vec3 SolverUp;
vec3 SourcePosition;
vec3 SourceGravityDirection;
bool SourceGeometryValid;

void prepareSourceGeometry(uint SourceTexel)
{
    // 중력의 표면 접선 성분과 source 위치를 한 번 계산해 이웃별 geometry drive에 재사용한다.
    SourceGeometryValid = false;

    if ((Solver.Flags & (1u << 0u)) != 0u)
    {
        return;
    }

    SolverUp = vec3(Solver.NormalMatrixAndUpColumns[0].w,
                    Solver.NormalMatrixAndUpColumns[1].w,
                    Solver.NormalMatrixAndUpColumns[2].w);
    if (length(SolverUp) <= GeometryEpsilon)
    {
        return;
    }

    SolverModelLinear =
        mat3(Solver.ModelLinearColumns[0].xyz, Solver.ModelLinearColumns[1].xyz, Solver.ModelLinearColumns[2].xyz);
    mat3 SolverNormalMatrix = mat3(Solver.NormalMatrixAndUpColumns[0].xyz,
                                   Solver.NormalMatrixAndUpColumns[1].xyz,
                                   Solver.NormalMatrixAndUpColumns[2].xyz);
    bool DynamicGeometryEnabled = (Solver.Flags & (1u << 6u)) != 0u;
    vec3 LocalNormal = DynamicGeometryEnabled
                           ? DynamicGeometry.Values[SourceTexel * 2u + 1u].xyz
                           : ((Solver.Flags & (1u << 4u)) != 0u ? Normals.Values[SourceTexel].xyz
                                                                : MesoNormals.Values[SourceTexel].xyz);
    vec3 Normal = SolverNormalMatrix * LocalNormal;
    float NormalLength = length(Normal);

    if (NormalLength <= GeometryEpsilon || !solverFinite(NormalLength))
    {
        return;
    }

    Normal /= NormalLength;
    vec3 Gravity = Solver.GravityWorld.xyz;
    vec3 GravityOnSurface = Gravity - Normal * dot(Gravity, Normal);
    float SurfaceGravityLength = length(GravityOnSurface);

    if (SurfaceGravityLength <= GeometryEpsilon || !solverFinite(SurfaceGravityLength))
    {
        return;
    }

    SourceGravityDirection = GravityOnSurface / SurfaceGravityLength;
    SourcePosition =
        DynamicGeometryEnabled ? DynamicGeometry.Values[SourceTexel * 2u].xyz : effectiveLocalPosition(SourceTexel);
    SourceGeometryValid = true;
}

float geometryDrive(uint TargetTexel)
{
    if (!SourceGeometryValid)
    {
        return 0.0;
    }

    vec3 TargetPosition = effectiveLocalPosition(TargetTexel);

    // 위치 차이를 사용하므로 model translation은 높이 차와 이웃 방향 계산에서 상쇄된다.
    vec3 NeighborDirection = SolverModelLinear * (TargetPosition - SourcePosition);
    float NeighborLength = length(NeighborDirection);

    if (NeighborLength <= GeometryEpsilon || !solverFinite(NeighborLength))
    {
        return 0.0;
    }

    float HeightDrive = abs(dot(NeighborDirection, SolverUp));
    float DirectionDrive = clamp(dot(SourceGravityDirection, NeighborDirection / NeighborLength), 0.0, 1.0);
    return HeightDrive * DirectionDrive;
}

float geometryDriveToPosition(vec3 TargetPosition)
{
    if (!SourceGeometryValid)
    {
        return 0.0;
    }

    // 위치 차이를 사용하므로 model translation은 높이 차와 이웃 방향 계산에서 상쇄된다.
    vec3 NeighborDirection = SolverModelLinear * (TargetPosition - SourcePosition);
    float NeighborLength = length(NeighborDirection);

    if (NeighborLength <= GeometryEpsilon || !solverFinite(NeighborLength))
    {
        return 0.0;
    }

    float HeightDrive = abs(dot(NeighborDirection, SolverUp));
    float DirectionDrive = clamp(dot(SourceGravityDirection, NeighborDirection / NeighborLength), 0.0, 1.0);
    return HeightDrive * DirectionDrive;
}

// #endregion
