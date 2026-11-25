#version 450

// 표시 메시 정점을 월드 좌표로 옮기고 레이어별 수위와 적층 높이를 구성한다.
#extension GL_GOOGLE_include_directive : require

// ============================================================
// 00. Vertex Interface
// ============================================================
// #region 00_Vertex_Interface
    // FSurfaceSimulationVertex의 로컬 위치와 계산 셀을 읽고 SurfaceFragmentMain.glsl에 월드 위치 및 표시 진단값을 전달한다.

    layout(location = 0) in vec3 InPosition;
    layout(location = 1) in vec3 InNormal;
    layout(location = 2) in uint InCell;
    layout(location = 3) in uint InRegion;

    #include "SurfaceDrawParameters.glsl"
    #include "SurfaceDisplayField.glsl"
    #include "LiquidSurfaceReconstruction.glsl"
    #include "LiquidSlosh.glsl"
    #include "LavaAppearance.glsl"

    layout(location = 0) out vec3 World;
    layout(location = 1) out vec3 Normal;
    layout(location = 2) out vec3 Local;
    layout(location = 3) out float Depth;
    layout(location = 4) flat out vec4 DebugValue;
    layout(location = 5) flat out vec3 Activity;
    layout(location = 6) out float OverflowWetness;
    layout(location = 7) flat out uint CellId;
    layout(location = 8) out float ShoreAppearanceDepth;

// #endregion

// ============================================================
// 01. Layer Vertex Positions
// ============================================================
// #region 01_Layer_Vertex_Positions
    // 저장 수위에서 수면과 깊이를 구성하며, 젖은 연결부의 표시 수위는 양쪽에서 공유한다.

    vec3 rotate_y(vec3 p)
    {
        float s = Push.Pose.x;
        float c = Push.Pose.y;
        return vec3(c * p.x + s * p.z, p.y, -s * p.x + c * p.z);
    }

    float recent_overflow_mass(ivec2 sample_at, int axis)
    {
        ivec2 neighbor = sample_at + (axis == 0 ? ivec2(1, 0) : ivec2(0, 1));
        if (any(lessThan(sample_at, ivec2(0))) || any(greaterThanEqual(neighbor, Field.Counts.xy)))
            return 0.0;
        int contact = field_contact_indices(int(2 * (sample_at.y * Field.Counts.x + sample_at.x) + axis));
        if (contact < 0)
            return 0.0;
        float mass = 0.0;
        for (int direction = 0; direction < 2; ++direction)
        {
            OverflowTransferRecord flow = display_overflow(int(2 * contact + direction));
            float age = max(0.0, Push.CameraTime.w - flow.last_transfer_s);
            mass += flow.mass_kg * (1.0 - smoothstep(0.06, OVERFLOW_LIFETIME_S, age));
        }
        return mass;
    }

    float overflow_wetness(vec2 point)
    {
        // 같은 차트 모서리를 공유하는 삼각형은 같은 네 접촉을 읽어 젖음이 이어진다.
        ivec2 corner = ivec2(round((point + 0.5 * Field.SampleLayout.xy) / Field.SampleLayout.zw));
        ivec2 lower = corner - 1;
        float mass = recent_overflow_mass(lower, 0) + recent_overflow_mass(lower, 1) +
                     recent_overflow_mass(ivec2(lower.x, corner.y), 0) +
                     recent_overflow_mass(ivec2(corner.x, lower.y), 1);
        float areal_mass = 0.25 * mass / (Field.SampleLayout.z * Field.SampleLayout.w);
        return 0.72 * smoothstep(0.001, 0.07, areal_mass);
    }

    void main()
    {
        Local = InPosition;
        if (Push.Layer == SECTION_WINDOW_LAYER || Push.Layer == SECTION_WINDOW_BACK_LAYER ||
            Push.Layer == VESSEL_CONTENTS_LAYER)
        {
            // 측벽에 보이는 물질은 투명한 바깥 벽과 두께만큼 떨어져 있다.
            if (Push.Layer == VESSEL_CONTENTS_LAYER)
                Local.xy = clamp(Local.xy, Field.VesselBounds.xy, Field.VesselBounds.zw);
            World = rotate_y(Local);
            Normal = rotate_y(InNormal);
            gl_Position = Push.ViewProjection * vec4(World, 1.0);
            return;
        }
        SurfaceCellState water = display_cell(int(InCell));
        bool has_lava_deposit = (Push.AppearanceFlags & LAVA_DEPOSIT) != 0u &&
                           InNormal.z > 0.22 && (Push.Mode < 3u || Push.Mode == PBR_SURFACE_MODE);
        float lava_height = 0.0;
        if (has_lava_deposit)
        {
            bool raised_layer = Push.Layer == LAVA_DEPOSIT_LAYER || Push.Layer == 1u;
            bool has_snow = Field.IceProperties.z > 0.0;
            if (!has_snow)
                Local.z += sample_coating_masses(Local.xy).y / Field.IceProperties.y;
            if (raised_layer)
            {
                // 침착층과 물의 광로에 눈 표층을 그리는 패스와 같은 높이를 쓴다.
                if (has_snow)
                    Local.z += snow_layer_height(Local.xy);
                lava_height = lava_deposit_height(Local.xy);
                Local.z += lava_height;
            }
        }
        if (Field.FilmDisplayWord >= 0 && InPosition.z > Field.BedMetrics.z + 1e-6)
        {
            // 공용 모서리에 네 형상 표본의 평균을 적용해 층 두께가 달라진 곳도 메시가 이어진다.
            bool vessel_appearance = Field.SectionWindow.w > 0.0 && (Push.Mode < 3u || Push.Mode == PBR_SURFACE_MODE);
            Local.z = bed_height(vessel_appearance ? vessel_sample_point(InPosition.xy) : InPosition.xy);
        }
        World = rotate_y(Local);
        Normal = rotate_y(InNormal);
        vec2 local_water = display_region(int(InRegion));
        bool single_region = field_cell_region_counts(int(InCell)) == 1;
        bool phase_surface = single_region && Field.PhaseLevels.y > Field.PhaseLevels.x && Push.Layer <= 1u &&
                             (Push.Mode < 3u || Push.Mode == PBR_SURFACE_MODE);
        float level = phase_surface ? water.field.w : local_water.y;
        float visible_mass = local_water.x + (phase_surface ? water.substrate.y : 0.0);
        Depth = visible_mass > 0.0 ? level - World.z : -1.0;
        ShoreAppearanceDepth = Depth;
        bool smooth_wet_appearance = liquid_option_enabled(SMOOTH_LIQUID_BOUNDARY) && (Push.Mode <= 2u || Push.Mode == PBR_SURFACE_MODE);
        if (!phase_surface && smooth_wet_appearance && Push.Layer == 0u && InNormal.z > 0.22 && local_water.x > 0.0)
            ShoreAppearanceDepth = shore_appearance_depth(InPosition.xy, InRegion, level, World.z, false, false);
        DebugValue = vec4(water.substrate.xy * water.field.z, water.substrate.z, water.field.y);
        Activity = vec3(water.substrate.w, water.field.x, local_water.x);
        OverflowWetness = 0.0;
        if (Push.Layer == 0u && (Push.Mode <= 2u || Push.Mode == PBR_SURFACE_MODE) && InNormal.z > 0.0 &&
            (Push.AppearanceFlags & OVERFLOW_FILM_ON_BED) != 0u)
            OverflowWetness = overflow_wetness(InPosition.xy);
        CellId = InCell;
        if (Push.Layer == SNOW_LAYER)
        {
            Depth = snow_layer_height(InPosition.xy);
            World = rotate_y(InPosition + vec3(0.0, 0.0, Depth));
        }
        if (Push.Layer == 1u)
        {
            if (smooth_wet_appearance && local_water.x > 0.0 &&
                (Field.FilmDisplayWord >= 0 || smooth_single_water_surface()))
            {
                Depth = shore_appearance_depth(InPosition.xy, InRegion, level, World.z, true, false);
                World.z += Depth;
            }
            else
                World.z = level;
            Normal = vec3(0.0, 0.0, 1.0);
            if (!phase_surface && single_region && local_water.x > 0.0 && liquid_option_enabled(SLOSH_HEIGHT) &&
                slosh_amplitude() != 0.0)
            {
                vec3 slosh = water_height_slosh(int(InCell), Local.xy, level);
                World.z += slosh.z;
                Depth += slosh.z;
                Normal = normalize(vec3(-slosh.xy, 1.0));
            }
        }
        if (Push.Layer == LAVA_DEPOSIT_LAYER)
            Depth = lava_height;
        // 차트 조회, 수위와 수심 계산을 마친 뒤 시편을 화면에서 벌린다.
        if (Field.DisplayGapMeters > 0.0)
            World += rotate_y(vec3(display_gap_offset(cell_specimen(InCell)), 0.0, 0.0));
        gl_Position = Push.ViewProjection * vec4(World, 1.0);
    }

// #endregion
