// SurfaceDebug.frag — Surface 상태, mapping, Meso 및 선택 State 적층 미리보기를 출력한다.
#version 450
#extension GL_GOOGLE_include_directive : require

// ============================================================
// 00. Resource Interface
// ============================================================
// #region 00_Resource_Interface
// CPU가 전달하는 리소스 선언과 shader의 접근 경계를 모아 ABI 대응을 확인한다.

    // 00.00. Fragment Inputs and Material Controls
    // #region 00_00_Fragment_Inputs_and_Material_Controls
    // binding 2·1의 선언을 CPU descriptor와 대응시킨다.

    layout(location = 0) in vec3 FragNormal;
    layout(location = 1) in vec3 FragTangent;
    layout(location = 2) in float FragTangentSign;
    layout(location = 3) in vec2 FragUV;
    layout(location = 4) flat in uint FragSurfaceIndex;
    layout(location = 5) in vec3 FragMesoNormalWS;
    layout(location = 6) in vec3 FragWorldPosition;

    // CPU: RendererShared.h의 FMaterialUniform prefix; RendererMaterials.cpp에서 업로드한다.
    layout(set = 0, binding = 2) uniform MaterialParameters
    {
        vec4 BaseColor;
        uint RenderMode;
        uint FlipNormalY;
        float NormalStrength;
        float AmbientLight;
        uint DebugStateChannel;
        uint StateChannelCount;
        float DebugViewParameter;
        float ReliefShadingEnabled;
        vec4 DebugOptions;
        uvec4 DebugFlags;
    }

    Material;

    layout(set = 0, binding = 1) uniform sampler2D NormalTexture;

    #include "Debug/SurfaceDebugData.glsl"

    layout(push_constant) uniform TStaticMeshPushConstants
    {
        mat4 Model;
        mat4 ViewProjection;
    }

    Push;

    // #endregion

    // 00.01. Diagnostic Buffers and View Mode Contract
    // #region 00_01_Diagnostic_Buffers_and_View_Mode_Contract
    // binding 10·13·16의 선언을 CPU descriptor와 대응시킨다.

    layout(std430, set = 1, binding = 10) readonly buffer TSurfaceOutgoingFluxScale
    {
        float Values[];
    }

    OutgoingFluxScale;

    layout(std430, set = 1, binding = 13) readonly buffer TSurfaceTexelChartIndices
    {
        uint Values[];
    }

    TexelChartIndices;

    layout(std430, set = 1, binding = 16) readonly buffer TSurfaceTransferWeightDebugAverages
    {
        vec4 Values[];
    }

    TransferWeightDebugAverages;

    layout(location = 0) out vec4 OutColor;

    const uint InvalidIndex = 0xffffffffu;

    // 값은 ERenderViewMode enum의 항목과 일치해야 한다.
    const uint RENDER_MODE_SURFACE_STATE_HEATMAP = 6u;

    const uint RENDER_MODE_SURFACE_VALIDITY = 7u;

    const uint RENDER_MODE_SURFACE_ID = 8u;

    const uint RENDER_MODE_NEIGHBOR_COUNT = 9u;

    const uint RENDER_MODE_SURFACE_SEAM = 10u;

    const uint RENDER_MODE_OUTGOING_FLUX_SCALE = 11u;

    const uint RENDER_MODE_SOLVER_TRANSFER_WEIGHT = 12u;

    const uint RENDER_MODE_MESO_HEIGHT = 13u;

    const uint RENDER_MODE_MESO_OFFSET = 14u;

    const uint RENDER_MODE_MACRO_GEOMETRY = 15u;

    const uint RENDER_MODE_SURFACE_TEXEL_GRID = 16u;

    const uint RENDER_MODE_SURFACE_TEXEL_AREA = 17u;

    const uint RENDER_MODE_ACCUMULATION = 18u;

    const uint RENDER_MODE_FINAL_GEOMETRY = 19u;

    const uint RENDER_MODE_TOTAL_SIMULATION_HEIGHT = 21u;

    #include "Debug/Views/SurfaceDebugColors.glsl"

    mat3 AccumulationNormalMatrix();
    void ComputeDebugDerivatives(out vec2 UVFootprint, out float WorldAreaPerUV);
    void RenderTexelGrid(uvec4 Range, vec2 UVFootprint);
    void RenderTexelArea(uvec4 Range, float WorldAreaPerUV);
    void RenderValidity(bool bSimulationEnabled, bool bGeometryValid);
    void RenderHeightGeometry(uvec4 Range, vec2 UVFootprint);
    void RenderOutgoingFluxScale(uint TexelIndex, bool bSimulationEnabled);
    void RenderMesoHeight(uvec4 Range, vec2 UVFootprint, uint TexelIndex);
    void RenderMacroGeometry();
    void RenderSurfaceID();
    void RenderNeighbors(uint TexelIndex);
    void RenderTransferWeight(uint TexelIndex);
    void RenderAccumulation(uvec4 Range, vec2 UVFootprint, uint TexelIndex);
    void RenderStateHeatmap(uint TexelIndex, bool bSimulationEnabled);
    bool ResolveDebugSurfaceRange(out uvec4 Range);
    bool RenderUVViewsIfSelected(uvec4 Range, vec2 UVFootprint, float WorldAreaPerUV);
    void RenderMappedSurfaceView(uvec4 Range, vec2 UVFootprint);
    void RenderSelectedSolverView(uvec4 Range, vec2 UVFootprint, uint TexelIndex, bool bSimulationEnabled);

    // #endregion

// #endregion

// ============================================================
// 01. Main Process
// ============================================================
// #region 01_Main_Process
// Surface 상태, mapping, Meso 및 선택 State 적층 미리보기를 출력한다.

void main()
{
    vec2 UVFootprint;
    float WorldAreaPerUV;
    ComputeDebugDerivatives(UVFootprint, WorldAreaPerUV);

    uvec4 Range;

    if (!ResolveDebugSurfaceRange(Range))
    {
        return;
    }

    if (RenderUVViewsIfSelected(Range, UVFootprint, WorldAreaPerUV))
    {
        return;
    }

    RenderMappedSurfaceView(Range, UVFootprint);
}
// #endregion

// ============================================================
// 02. Detailed Implementation
// ============================================================
// #region 02_Detailed_Implementation
// Surface 상태, mapping, Meso 및 선택 State 적층 미리보기를 출력한다.


// Surface range validation preserves the fallback diagnostic color.
bool ResolveDebugSurfaceRange(out uvec4 Range)
{
    if (FragSurfaceIndex >= uint(SurfaceRanges.Values.length()))
    {
        OutColor = vec4(0.35, 0.35, 0.35, 1.0);
        return false;
    }

    Range = SurfaceRanges.Values[FragSurfaceIndex];

    if (Range.y == 0u || Range.z == 0u)
    {
        OutColor = vec4(0.35, 0.35, 0.35, 1.0);
        return false;
    }

    return true;
}

// Render purely UV-based diagnostics without sampling Solver buffers.
bool RenderUVViewsIfSelected(uvec4 Range, vec2 UVFootprint, float WorldAreaPerUV)
{
    // texel grid와 면적 모드는 simulation data 없이 UV와 화면 미분값으로 바로 진단한다.
    if (Material.RenderMode == RENDER_MODE_SURFACE_TEXEL_GRID)
    {
        RenderTexelGrid(Range, UVFootprint);
        return true;
    }

    if (Material.RenderMode == RENDER_MODE_SURFACE_TEXEL_AREA)
    {
        RenderTexelArea(Range, WorldAreaPerUV);
        return true;
    }

    return false;
}

// Resolve mapped texel eligibility before dispatching Solver-specific diagnostics.
void RenderMappedSurfaceView(uvec4 Range, vec2 UVFootprint)
{
    uvec2 TexelXY = min(uvec2(floor(clamp(FragUV, 0.0, 1.0) * vec2(Range.yz))), Range.yz - 1u);
    uint TexelIndex = Range.x + TexelXY.y * Range.y + TexelXY.x;
    bool bInBuffer = TexelIndex < uint(TexelSurfaceIndices.Values.length()) &&
                     TexelIndex < uint(TexelProfileIndices.Values.length());
    bool bGeometryValid = bInBuffer && TexelSurfaceIndices.Values[TexelIndex] == FragSurfaceIndex;
    bool bSimulationEnabled = bGeometryValid && TexelProfileIndices.Values[TexelIndex] != InvalidIndex;

    // geometry 유효성은 Surface 소속 여부, simulation 활성 여부는 profile 할당 여부로 구분한다.
    if (Material.RenderMode == RENDER_MODE_SURFACE_VALIDITY)
    {
        RenderValidity(bSimulationEnabled, bGeometryValid);
        return;
    }

    // UV cell 중심 샘플이 없는 곳도 세밀하게 만든 source boundary 면은 계속 표시한다.
    if (Material.RenderMode == RENDER_MODE_MESO_OFFSET || Material.RenderMode == RENDER_MODE_FINAL_GEOMETRY ||
        Material.RenderMode == RENDER_MODE_TOTAL_SIMULATION_HEIGHT)
    {
        RenderHeightGeometry(Range, UVFootprint);
        return;
    }

    if (!bGeometryValid)
    {
        OutColor = vec4(0.10, 0.10, 0.13, 1.0);
        return;
    }

    RenderSelectedSolverView(Range, UVFootprint, TexelIndex, bSimulationEnabled);
}

void RenderSelectedSolverView(uvec4 Range, vec2 UVFootprint, uint TexelIndex, bool bSimulationEnabled)
{
    // 선택한 Solver 항과 channel의 진단 값을 해당 색상 표현으로 연결한다.

    // 이후 모드는 texel에 대응하는 solver 데이터를 확인한 뒤 각 buffer를 읽는다.
    if (Material.RenderMode == RENDER_MODE_OUTGOING_FLUX_SCALE)
    {
        RenderOutgoingFluxScale(TexelIndex, bSimulationEnabled);
        return;
    }

    if (Material.RenderMode == RENDER_MODE_MESO_HEIGHT)
    {
        RenderMesoHeight(Range, UVFootprint, TexelIndex);
        return;
    }

    if (Material.RenderMode == RENDER_MODE_MACRO_GEOMETRY)
    {
        RenderMacroGeometry();
        return;
    }

    if (Material.RenderMode == RENDER_MODE_SURFACE_ID)
    {
        RenderSurfaceID();
        return;
    }

    if (Material.RenderMode == RENDER_MODE_NEIGHBOR_COUNT || Material.RenderMode == RENDER_MODE_SURFACE_SEAM)
    {
        RenderNeighbors(TexelIndex);
        return;
    }

    if (Material.RenderMode == RENDER_MODE_SOLVER_TRANSFER_WEIGHT)
    {
        RenderTransferWeight(TexelIndex);
        return;
    }

    if (Material.RenderMode == RENDER_MODE_ACCUMULATION)
    {
        RenderAccumulation(Range, UVFootprint, TexelIndex);
        return;
    }

    RenderStateHeatmap(TexelIndex, bSimulationEnabled);

}

    // 02.00. Screen Footprint and Basic View Helpers
    // #region 02_00_Screen_Footprint_and_Basic_View_Helpers
    // 화면 미분과 texel 크기를 이용해 grid·기본 진단의 표시 폭을 계산한다.

    void ComputeDebugDerivatives(out vec2 UVFootprint, out float WorldAreaPerUV)
    {
        // fwidth와 dFdx/dFdy는 fragment별 조기 반환 전에 uniform 분기 안에서 계산한다.
        UVFootprint = vec2(0.0);
        WorldAreaPerUV = 0.0;
        bool HeightMode =
            Material.RenderMode == RENDER_MODE_MESO_HEIGHT || Material.RenderMode == RENDER_MODE_MESO_OFFSET ||
            Material.RenderMode == RENDER_MODE_ACCUMULATION || Material.RenderMode == RENDER_MODE_FINAL_GEOMETRY ||
            Material.RenderMode == RENDER_MODE_TOTAL_SIMULATION_HEIGHT;
        if (Material.RenderMode == RENDER_MODE_SURFACE_TEXEL_GRID || (HeightMode && Material.DebugFlags.z != 0u))
        {
            UVFootprint = fwidth(FragUV);
        }
        else if (Material.RenderMode == RENDER_MODE_SURFACE_TEXEL_AREA)
        {
            vec2 UVdx = dFdx(FragUV);
            vec2 UVdy = dFdy(FragUV);
            vec3 PositionDx = dFdx(FragWorldPosition);
            vec3 PositionDy = dFdy(FragWorldPosition);
            float UVArea = abs(UVdx.x * UVdy.y - UVdx.y * UVdy.x);

            // 분자와 분모의 화면 면적이 상쇄되므로 카메라 거리와 무관한 월드 면적 비율을 얻는다.
            WorldAreaPerUV = UVArea > 0.0 ? length(cross(PositionDx, PositionDy)) / UVArea : 0.0;
        }
    }

    void RenderTexelGrid(uvec4 Range, vec2 UVFootprint)
    {
        OutColor = vec4(TexelGridColor(FragUV * vec2(Range.yz), UVFootprint * vec2(Range.yz)), 1.0);
        return;
    }

    void RenderTexelArea(uvec4 Range, float WorldAreaPerUV)
    {
        float Area = WorldAreaPerUV / (float(Range.y) * float(Range.z));
        bool ValidArea = Area > 0.0 && !isnan(Area) && !isinf(Area);
        OutColor = vec4(ValidArea ? TexelAreaColor(Area) : vec3(1.0, 0.18, 0.72), 1.0);
        return;
    }

    void RenderValidity(bool bSimulationEnabled, bool bGeometryValid)
    {
        vec3 Color = !bGeometryValid ? vec3(0.86, 0.12, 0.08)
                                     : (bSimulationEnabled ? vec3(0.10, 0.78, 0.24) : vec3(0.18, 0.48, 0.82));
        OutColor = vec4(Color, 1.0);
        return;
    }

    void RenderHeightGeometry(uvec4 Range, vec2 UVFootprint)
    {
        vec3 Normal = normalize(FragMesoNormalWS);
        float Diffuse = max(dot(Normal, normalize(vec3(0.35, 0.55, 1.0))), 0.0);
        OutColor = vec4(HeightGridColor(Material.BaseColor.rgb * (0.28 + 0.72 * Diffuse), Range, UVFootprint),
                        Material.BaseColor.a);
        return;
    }

    // #endregion

    // 02.01. Flux and Surface Geometry Views
    // #region 02_01_Flux_and_Surface_Geometry_Views
    // 계산 결과를 ProfileSupported 버퍼에 기록해 후속 GPU 단계에 전달한다.

    void RenderOutgoingFluxScale(uint TexelIndex, bool bSimulationEnabled)
    {
        if (Material.StateChannelCount == 0u || Material.DebugStateChannel >= Material.StateChannelCount)
        {
            OutColor = vec4(0.35, 0.35, 0.38, 1.0);
            return;
        }

        uint ScaleIndex = TexelIndex * Material.StateChannelCount + Material.DebugStateChannel;
        uint ProfileIndex = TexelProfileIndices.Values[TexelIndex];
        uint ProfileRecordIndex = ProfileIndex * Material.StateChannelCount + Material.DebugStateChannel;

        if (!bSimulationEnabled || ScaleIndex >= uint(OutgoingFluxScale.Values.length()) ||
            ProfileRecordIndex >= uint(ProfileSupported.Values.length()) ||
            ProfileSupported.Values[ProfileRecordIndex] == 0u)
        {
            OutColor = vec4(0.42, 0.42, 0.45, 1.0);
            return;
        }

        OutColor = vec4(OutgoingFluxScaleColor(OutgoingFluxScale.Values[ScaleIndex]), 1.0);
        return;
    }

    void RenderMesoHeight(uvec4 Range, vec2 UVFootprint, uint TexelIndex)
    {
        if (TexelIndex >= uint(GeometryScalars.Values.length()))
        {
            OutColor = vec4(0.35, 0.35, 0.35, 1.0);
            return;
        }

        float Height = GeometryScalars.Values[TexelIndex].MesoVirtualHeight;
        float SignedT = clamp(0.5 + atan(Height * 16.0) / 3.14159265, 0.0, 1.0);
        vec3 Negative = vec3(0.12, 0.52, 0.92);
        vec3 Zero = vec3(0.12, 0.13, 0.17);
        vec3 Positive = vec3(1.0, 0.42, 0.10);
        vec3 Color = SignedT < 0.5 ? mix(Negative, Zero, SignedT * 2.0) : mix(Zero, Positive, (SignedT - 0.5) * 2.0);
        OutColor = vec4(HeightGridColor(Color, Range, UVFootprint), 1.0);
        return;
    }

    void RenderMacroGeometry()
    {
        vec3 Normal = normalize(FragNormal);
        float Diffuse = abs(dot(Normal, normalize(vec3(0.35, 0.55, 1.0))));
        OutColor = vec4(Material.BaseColor.rgb * (0.28 + 0.72 * Diffuse), Material.BaseColor.a);
        return;
    }

    void RenderSurfaceID()
    {
        // Surface ID 뷰는 조명과 무관하게 구분할 수 있도록 선명한 고정 색상표를 사용한다.
        const vec3 SurfaceIdPalette[8] = vec3[8](vec3(1.00, 0.05, 0.05), // red
                                                 vec3(0.05, 0.25, 1.00), // blue
                                                 vec3(0.05, 0.90, 0.12), // green
                                                 vec3(1.00, 0.82, 0.02), // yellow
                                                 vec3(1.00, 0.04, 0.78), // magenta
                                                 vec3(0.00, 0.90, 0.95), // cyan
                                                 vec3(1.00, 0.35, 0.02), // orange
                                                 vec3(0.48, 0.08, 1.00)  // violet
        );
        vec3 Color = SurfaceIdPalette[FragSurfaceIndex % 8u];
        OutColor = vec4(Color, 1.0);
        return;
    }

    // #endregion

    // 02.02. Topology and Transfer Views
    // #region 02_02_Topology_and_Transfer_Views
    // NeighborIndices·TexelChartIndices·TransferWeightDebugAverages 버퍼의 값을 읽어 이 구획의 수치 계산에 사용한다.

    void RenderNeighbors(uint TexelIndex)
    {
        if (TexelIndex >= uint(NeighborIndices.Values.length()) || TexelIndex >= uint(TexelChartIndices.Values.length()))
        {
            OutColor = vec4(0.35, 0.35, 0.35, 1.0);
            return;
        }

        uint NeighborCount = 0u;
        bool bHasCrossChartNeighbor = false;
        uint Chart = TexelChartIndices.Values[TexelIndex];

        for (uint DirectionIndex = 0u; DirectionIndex < 8u; ++DirectionIndex)
        {
            uint Neighbor = NeighborIndices.Values[TexelIndex].Indices[DirectionIndex];

            if (Neighbor == InvalidIndex || Neighbor >= uint(TexelChartIndices.Values.length()))
            {
                continue;
            }

            ++NeighborCount;
            uint NeighborChart = TexelChartIndices.Values[Neighbor];
            bHasCrossChartNeighbor = bHasCrossChartNeighbor ||
                                     (Chart != InvalidIndex && NeighborChart != InvalidIndex && NeighborChart != Chart);
        }

        if (Material.RenderMode == RENDER_MODE_NEIGHBOR_COUNT)
        {
            float T = float(NeighborCount) / 8.0;
            OutColor = vec4(mix(vec3(0.08, 0.10, 0.18), vec3(0.95, 0.72, 0.12), T), 1.0);
        }
        else
        {
            OutColor = vec4(bHasCrossChartNeighbor ? vec3(1.0, 0.18, 0.72) : vec3(0.12, 0.16, 0.22), 1.0);
        }

        return;
    }

    void RenderTransferWeight(uint TexelIndex)
    {
        if (TexelIndex >= uint(TransferWeightDebugAverages.Values.length()))
        {
            OutColor = vec4(0.35, 0.35, 0.35, 1.0);
            return;
        }

        vec4 Values = TransferWeightDebugAverages.Values[TexelIndex];
        uint Component = uint(clamp(Material.DebugViewParameter, 0.0, 3.0));
        float Value = Component == 0u ? Values.x : (Component == 1u ? Values.y : (Component == 2u ? Values.z : Values.w));
        OutColor = vec4(TransferWeightColor(Value), 1.0);
        return;
    }

    // #endregion

    // 02.03. Accumulation Height Views
    // #region 02_03_Accumulation_Height_Views
    // 표시 높이와 Solver 높이의 차이를 같은 texel 기준으로 시각화한다.

    void RenderAccumulation(uvec4 Range, vec2 UVFootprint, uint TexelIndex)
    {
        // 누적 높이를 cavity fill, cavity 초과분, 표면 위 적층 높이로 나눠 표시한다.
        TDebugAccumulation D = DebugAccumulation(TexelIndex,
                                                 Material.DebugStateChannel,
                                                 Material.StateChannelCount,
                                                 Material.DebugOptions.z,
                                                 AccumulationNormalMatrix());
        if (D.Status != 3u)
        {
            vec3 Color =
                D.Status == 4u ? vec3(1, 0, 1) : (D.Status == 2u ? vec3(0.42, 0.42, 0.45) : vec3(0.18, 0.20, 0.24));
            OutColor = vec4(HeightGridColor(Color, Range, UVFootprint), 1);
            return;
        }

        uint Component = Material.DebugFlags.y;

        if (Component == 4u && (D.CavityDepth <= 1e-8 || D.Fill <= 0.0))
        {
            OutColor = vec4(HeightGridColor(vec3(0.12, 0.13, 0.15), Range, UVFootprint), 1);
            return;
        }

        float Value = Component == 1u ? D.CavityHeight
                                      : (Component == 2u ? D.FollowingHeight
                                                         : ((Component == 3u || Component == 4u) ? D.Fill : D.Height));
        float Limit = Component == 0u ? D.CapacityHeight
                                      : (Component == 1u ? D.CapacityCavityHeight
                                                         : (Component == 2u ? D.CapacityFollowingHeight : 1.0));
        Limit = max(Limit, 1e-12);
        vec3 Color = HeatColor(Value / Limit);

        if (Value > Limit)
        {
            Color = vec3(1, 0.25, 0.05);
        }

        OutColor = vec4(HeightGridColor(Color, Range, UVFootprint), 1);
        return;
    }

    mat3 AccumulationNormalMatrix()
    {
        return transpose(inverse(mat3(Push.Model)));
    }

    // #endregion

    // 02.04. State Heatmap Evaluation
    // #region 02_04_State_Heatmap_Evaluation
    // 계산 결과를 ProfileSupported 버퍼에 기록해 후속 GPU 단계에 전달한다.

    void RenderStateHeatmap(uint TexelIndex, bool bSimulationEnabled)
    {
        if (Material.RenderMode != RENDER_MODE_SURFACE_STATE_HEATMAP)
        {
            OutColor = vec4(0.35, 0.35, 0.38, 1.0);
            return;
        }

        // simulation 대상이 아닌 Surface도 형상 진단 뷰에는 표시한다.
        if (!bSimulationEnabled)
        {
            OutColor = vec4(0.18, 0.20, 0.24, 1.0);
            return;
        }

        if (Material.StateChannelCount == 0u)
        {
            OutColor = vec4(0.35, 0.35, 0.35, 1.0);
            return;
        }

        uint ProfileIndex = TexelProfileIndices.Values[TexelIndex];

        if (Material.DebugFlags.x != 0u)
        {
            float MaximumSaturation = 0.0;
            bool AnySupported = false;

            for (uint Channel = 0u; Channel < Material.StateChannelCount; ++Channel)
            {
                uint Record = ProfileIndex * Material.StateChannelCount + Channel;
                uint StateIndex = TexelIndex * Material.StateChannelCount + Channel;

                if (Record >= uint(ProfileSupported.Values.length()) || Record >= SurfaceProfileRecordCount() ||
                    StateIndex >= uint(CurrentState.Values.length()))
                {
                    OutColor = vec4(0.35, 0.35, 0.35, 1.0);
                    return;
                }

                if (ProfileSupported.Values[Record] == 0u)
                {
                    continue;
                }

                AnySupported = true;
                TDebugAccumulation D = DebugAccumulation(
                    TexelIndex, Channel, Material.StateChannelCount, Material.DebugOptions.z, AccumulationNormalMatrix());
                if (D.Status != 3u)
                {
                    OutColor = vec4(1, 0, 1, 1);
                    return;
                }

                MaximumSaturation = max(MaximumSaturation, D.Saturation);
            }

            if (!AnySupported)
            {
                OutColor = vec4(0.42, 0.42, 0.45, 1.0);
                return;
            }

            vec3 AllColor = MaximumSaturation > 1.0 ? vec3(1, 0.25, 0.05) : HeatColor(MaximumSaturation);
            OutColor = vec4(ApplyReliefLighting(AllColor, FragMesoNormalWS), 1.0);
            return;
        }

        if (Material.DebugStateChannel >= Material.StateChannelCount)
        {
            OutColor = vec4(0.35, 0.35, 0.35, 1.0);
            return;
        }

        uint ProfileRecordIndex = ProfileIndex * Material.StateChannelCount + Material.DebugStateChannel;
        uint StateIndex = TexelIndex * Material.StateChannelCount + Material.DebugStateChannel;

        if (ProfileRecordIndex >= uint(ProfileSupported.Values.length()) ||
            ProfileRecordIndex >= SurfaceProfileRecordCount() || StateIndex >= uint(CurrentState.Values.length()))
        {
            OutColor = vec4(0.35, 0.35, 0.35, 1.0);
            return;
        }

        if (ProfileSupported.Values[ProfileRecordIndex] == 0u)
        {
            OutColor = vec4(0.42, 0.42, 0.45, 1.0);
            return;
        }

        // 기본 heatmap은 현재 State를 profile capacity로 나눈 포화도를 색으로 나타낸다.
        TDebugAccumulation D = DebugAccumulation(TexelIndex,
                                                 Material.DebugStateChannel,
                                                 Material.StateChannelCount,
                                                 Material.DebugOptions.z,
                                                 AccumulationNormalMatrix());
        if (D.Status != 3u)
        {
            OutColor = vec4(1, 0, 1, 1);
            return;
        }

        float Value = D.Saturation;
        vec3 Color = HeatColor(Value);

        if (Value > 1.0)
        {
            Color = vec3(1, 0.25, 0.05);
        }

        OutColor = vec4(ApplyReliefLighting(Color, FragMesoNormalWS), 1.0);
    }

    // #endregion

// #endregion
