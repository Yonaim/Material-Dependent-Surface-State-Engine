// 계산 셀 상태와 독립적인 형상 샘플의 분해능을 같은 표면에서 비교한다.

// ============================================================
// 00. Grid Diagnostics
// ============================================================
// #region 00_Grid_Diagnostics
    // SurfaceSimulationDisplay.h의 뷰 ID를 사용하고 선택한 격자의 경계를 화면 픽셀 폭으로 그린다.

    const uint SIMULATION_GRID_VIEW = 11u;
    const uint GEOMETRY_GRID_VIEW = 12u;
    const uint LAYER_STATES_VIEW = 4u;
    const uint SUBSTRATE_STATES_VIEW = 6u;

    // SurfaceSimulationDisplay.cpp의 CellOutlineOverlay와 일치한다.
    const uint CELL_OUTLINE_OVERLAY = 1u << 4u;

    vec2 solver_cell_uv(vec2 point, uint cell)
    {
        int col = int(cell) % Field.Counts.z;
        int row = int(cell) / Field.Counts.z;
        vec2 low = vec2(field_edgex(int(col)), field_edgey(int(row)));
        vec2 high = vec2(field_edgex(int(col + 1)), field_edgey(int(row + 1)));
        return clamp((point - low) / (high - low), 0.0, 1.0);
    }

    float grid_line(vec2 coord, float width)
    {
        vec2 distance_to_line = abs(fract(coord + 0.5) - 0.5);
        vec2 pixels = distance_to_line / max(fwidth(coord), 1e-5);
        return 1.0 - smoothstep(width, width + 0.85, min(pixels.x, pixels.y));
    }

    vec3 heatmap_cell_grid(vec3 color, vec2 cell_uv)
    {
        return mix(color, vec3(0.68, 0.75, 0.82), 0.16 * grid_line(cell_uv, 0.05));
    }

    vec3 grid_view(vec3 point, vec2 cell_uv, uint cell, float shading, bool geometry_grid)
    {
        vec2 coordinates = (point.xy + Field.SampleLayout.xy * 0.5) / Field.SampleLayout.zw;
        vec2 index = floor(min(coordinates, vec2(Field.Counts.xy) - 0.001));
        if (!geometry_grid)
        {
            index = vec2(int(cell) % Field.Counts.z, int(cell) / Field.Counts.z);
            coordinates = cell_uv;
        }
        float checker = mod(index.x + index.y, 2.0);
        float elevation = clamp((point.z - Field.BedMetrics.x) / max(1e-5, Field.BedMetrics.y - Field.BedMetrics.x),
                                0.0, 1.0);
        vec3 face = mix(vec3(0.022, 0.046, 0.064), vec3(0.075, 0.135, 0.166), 0.76 * elevation + 0.24 * checker);
        vec3 line_color = geometry_grid ? vec3(0.14, 0.64, 0.81) : vec3(0.99, 0.65, 0.23);
        float line = grid_line(coordinates, geometry_grid ? 0.08 : 0.60);
        return mix(face * (0.8 + 0.2 * shading), line_color, line * (geometry_grid ? 0.73 : 0.91));
    }

// #endregion

// ============================================================
// 01. Cell SurfaceCellState Diagnostics
// ============================================================
// #region 01_Cell_State_Diagnostics
    // 색·범위·그룹 크기는 씬에 연결된 상태 정의에서 전달받는다.

    const uint AUTOMATIC_HEATMAP_PALETTE = 3u;
    const uint LAYER_THICKNESS_VIEW = 3u;
    const vec3 STATE_HEATMAP_BACKGROUND = vec3(0.027, 0.033, 0.046);
    const int SUBSTRATE_TEMPERATURE_CHANNEL = 4;
    const int FIRST_FILM_MASS_CHANNEL = 16;
    const int FIRST_SOLID_CHANNEL = 64;
    const int FIRST_MIXED_WATER_CHANNEL = 80;
    const int FIRST_MOISTURE_CHANNEL = 96;
    const uint CONTACT_AREA_VIEW = 15u;
    const uint EFFECTIVE_BED_VIEW = 16u;
    const uint SOLVER_DEPTH_VIEW = 17u;
    const uint OVERFLOW_TRANSFERS_VIEW = 18u;
    const uint ABSORPTION_FILL_VIEW = 19u;
    const float AMBIENT_TEMPERATURE_K = 293.15;

    float state_value(int source, vec2 point, uint cell)
    {
        SurfaceCellState state = display_cell(int(cell));
        if (source == 0)
        {
            uint region = region_at(point);
            return display_region(int(region)).x * field_region_inverse_areas(int(region));
        }
        if (source >= FIRST_SOLID_CHANNEL)
        {
            int region = int(region_at(point));
            int first = source >= FIRST_MOISTURE_CHANNEL ? FIRST_MOISTURE_CHANNEL :
                        (source >= FIRST_MIXED_WATER_CHANNEL ? FIRST_MIXED_WATER_CHANNEL : FIRST_SOLID_CHANNEL);
            vec2 composition = display_film_composition(source - first, region);
            if (first == FIRST_MOISTURE_CHANNEL)
                return composition.x > 0.0 ? 100.0 * composition.y / composition.x : 0.0;
            float mass = first == FIRST_MIXED_WATER_CHANNEL ? composition.y : composition.x - composition.y;
            return mass * field_region_inverse_areas(region);
        }
        if (source >= FIRST_FILM_MASS_CHANNEL)
        {
            int region = int(region_at(point));
            return display_film_mass(source - FIRST_FILM_MASS_CHANNEL, region) * field_region_inverse_areas(region);
        }
        if (source == 1)
            return state.substrate.y * state.field.z;
        if (source == 2)
            return state.substrate.x * state.field.z;
        if (source == SUBSTRATE_TEMPERATURE_CHANNEL)
            return state.field.y;
        return state.substrate.z;
    }

    float normalized_state(int index, vec2 point, uint cell)
    {
        int source = Field.StateChannels[index / 4][index % 4];
        float inverse_maximum = Field.StateVisuals[index].w;
        float value = state_value(source, point, cell);
        if (source == SUBSTRATE_TEMPERATURE_CHANNEL)
            return clamp((value - AMBIENT_TEMPERATURE_K) / (1.0 / inverse_maximum - AMBIENT_TEMPERATURE_K), 0.0, 1.0);
        return clamp(value * inverse_maximum, 0.0, 1.0);
    }

    float temperature_heatmap_value(vec2 point, uint cell, float temperature)
    {
        int first = Field.StateGroups.x;
        for (int index = first; index < first + Field.StateGroups.y; ++index)
            if (Field.StateChannels[index / 4][index % 4] == SUBSTRATE_TEMPERATURE_CHANNEL)
                return normalized_state(index, point, cell);
        return (temperature - 273.15) / 450.0;
    }

    float normalized_thickness(int index, vec2 point, uint cell, float liquid_depth)
    {
        int source = Field.StateChannels[index / 4][index % 4];
        // 유체의 자유수면은 저장 영역에서 복원한다. 고체 적층은 질량과 적층 밀도로 두께를 얻는다.
        if (source == 0)
        {
            if (Field.FilmDisplayWord >= 0)
                liquid_depth = film_diagnostic_depth(point);
            else if (liquid_option_enabled(SMOOTH_LIQUID_BOUNDARY))
            {
                // 셀의 수위와 렌더 메시를 섞기 전에 계산 형상 표본에서 실제 수심을 얻는다.
                uint owner = region_at(point);
                vec2 water = display_region(int(owner));
                float bed_world_z = bed_height(point) * Push.Pose.y - point.x * Push.Pose.x;
                liquid_depth = water.x > 0.0 ? shore_appearance_depth(point, owner, water.y, bed_world_z, true, true)
                                             : 0.0;
            }
            return clamp(liquid_depth * Field.StateThicknessInverseMeters[index / 4][index % 4], 0.0, 1.0);
        }
        float density = source >= FIRST_FILM_MASS_CHANNEL ? film_density(source - FIRST_FILM_MASS_CHANNEL) :
                        (Field.IceProperties.z > 0.0 ? Field.IceProperties.z : Field.IceProperties.y);
        return clamp(state_value(source, point, cell) / density * Field.StateThicknessInverseMeters[index / 4][index % 4], 0.0, 1.0);
    }

    vec3 state_group_heatmap(vec2 point, uint cell, int first, int count)
    {
        vec3 weighted_color = vec3(0.0);
        float total = 0.0;
        float maximum = 0.0;
        for (int index = first; index < first + count; ++index)
        {
            float value = normalized_state(index, point, cell);
            weighted_color += value * Field.StateVisuals[index].rgb;
            total += value;
            maximum = max(maximum, value);
        }
        // 겹친 상태의 색은 섞이되, 밝기는 가장 큰 정규화 값에 맞춰 상태 수에 따른 포화를 피한다.
        return mix(STATE_HEATMAP_BACKGROUND, weighted_color / max(total, 1e-6), maximum);
    }

    vec3 overflow_transfer_color(vec2 point)
    {
        vec2 grid = (point + 0.5 * Field.SampleLayout.xy) / Field.SampleLayout.zw;
        ivec2 sample_at = ivec2(clamp(floor(grid), vec2(0.0), vec2(Field.Counts.xy - 1)));
        vec3 color = STATE_HEATMAP_BACKGROUND;
        vec2 screen_pixel = clamp(fwidth(grid), vec2(1e-4), vec2(0.25));

        for (int axis = 0; axis < 2; ++axis)
        {
            ivec2 stride = axis == 0 ? ivec2(1, 0) : ivec2(0, 1);
            for (int side = 0; side < 2; ++side)
            {
                ivec2 origin = sample_at - side * stride;
                if (any(lessThan(origin, ivec2(0))) || any(greaterThanEqual(origin + stride, Field.Counts.xy)))
                    continue;
                int index = 2 * (origin.y * Field.Counts.x + origin.x) + axis;
                int contact = field_contact_indices(index);
                if (contact < 0)
                    continue;
                vec2 center = vec2(origin) + vec2(0.5) + 0.5 * vec2(stride);
                vec2 relative = grid - center;
                vec2 arrow = axis == 0 ? relative : relative.yx;
                for (int direction = 0; direction < 2; ++direction)
                {
                    OverflowTransferRecord flow = display_overflow(2 * contact + direction);
                    if (flow.mass_kg <= 0.0 || Push.CameraTime.w - flow.last_transfer_s >= OVERFLOW_LIFETIME_S)
                        continue;
                    float along = direction == 0 ? arrow.x : -arrow.x;
                    float cross = abs(arrow.y);
                    // 네 뷰를 함께 볼 때도 작은 월류 화살표에 최소한의 화면 폭을 남긴다.
                    float pixel = axis == 0 ? screen_pixel.y : screen_pixel.x;
                    float shaft_width = max(0.11, 0.65 * pixel);
                    float shaft = 1.0 - smoothstep(shaft_width, shaft_width + pixel, cross);
                    float head_width = max(0.0, (0.48 - along) * 0.72);
                    float head = 1.0 - smoothstep(head_width, head_width + pixel, cross);
                    float coverage = max(abs(along) < 0.40 ? shaft : 0.0,
                                         along > 0.015 && along < 0.48 ? head : 0.0);
                    float strength = 0.33 + 0.67 * sqrt(clamp(flow.mass_kg * 1e6, 0.0, 1.0));
                    color = max(color, mix(STATE_HEATMAP_BACKGROUND, heatmap(strength), coverage));
                }
            }
        }
        return color;
    }

    vec3 physical_diagnostic(vec2 point, uint cell)
    {
        if (Push.Mode == OVERFLOW_TRANSFERS_VIEW)
            return overflow_transfer_color(point);
        if (Push.Mode == ABSORPTION_FILL_VIEW)
        {
            float capacity = field_absorb_capacity(int(cell));
            return capacity > 0.0 ? heatmap(clamp(state_value(2, point, cell) / capacity, 0.0, 1.0))
                                  : STATE_HEATMAP_BACKGROUND;
        }

        vec2 grid = (point + 0.5 * Field.SampleLayout.xy) / Field.SampleLayout.zw;
        ivec2 sample_at = ivec2(clamp(floor(grid), vec2(0.0), vec2(Field.Counts.xy - 1)));
        int sample_index = sample_at.y * Field.Counts.x + sample_at.x;
        int region = int(field_sample_regions(sample_index));
        float base = field_base_height(sample_index);
        float local_bed = height_above_film_floor(base, point.x, display_effective_floor(region));
        float world_bed = local_bed * Push.Pose.y - point.x * Push.Pose.x;
        if (Push.Mode == EFFECTIVE_BED_VIEW)
            return heatmap(clamp((local_bed - Field.BedMetrics.x) /
                                 max(0.004, Field.BedMetrics.y - Field.BedMetrics.x), 0.0, 1.0));
        if (Push.Mode == SOLVER_DEPTH_VIEW)
        {
            vec2 water = display_region(region);
            float depth = water.x > 0.0 ? max(0.0, water.y - world_bed) : 0.0;
            return depth > 0.0 ? heatmap(clamp(depth / 0.004, 0.0, 1.0)) : STATE_HEATMAP_BACKGROUND;
        }
        vec4 contact = display_reaction_contact(region);
        float contact_bed = max(base * Push.Pose.y - point.x * Push.Pose.x, contact.w);
        if (contact.z == 0.0 || contact.x <= contact_bed)
            return STATE_HEATMAP_BACKGROUND;
        if (contact.z == 1.0)
            return vec3(0.38, 0.22, 0.085);
        float fraction = contact.y * field_region_inverse_areas(region);
        return mix(vec3(0.065, 0.26, 0.39), vec3(0.19, 0.92, 0.89), clamp(fraction, 0.0, 1.0));
    }

    vec3 surface_diagnostic(vec3 point, uint cell, bool side, float shading, float liquid_depth)
    {
        const vec3 background = STATE_HEATMAP_BACKGROUND;
        if (side)
            return background * shading;
        vec2 uv = solver_cell_uv(point.xy, cell);
        if (Push.Mode == SIMULATION_GRID_VIEW || Push.Mode == GEOMETRY_GRID_VIEW)
            return grid_view(point, uv, cell, shading, Push.Mode == GEOMETRY_GRID_VIEW);

        vec3 color = background;
        if (Push.Mode >= CONTACT_AREA_VIEW)
            color = physical_diagnostic(point.xy, cell);
        else if (Push.Mode == LAYER_STATES_VIEW || Push.Mode == SUBSTRATE_STATES_VIEW)
        {
            bool substrate = Push.Mode == SUBSTRATE_STATES_VIEW;
            color = state_group_heatmap(point.xy, cell, substrate ? Field.StateGroups.x : 0,
                                        substrate ? Field.StateGroups.y : Field.StateGroups.x);
        }
        else
        {
            int index = int((Push.AppearanceFlags >> 8u) & 0xffu);
            if (index < Field.StateGroups.x + Field.StateGroups.y)
            {
                float value = Push.Mode == LAYER_THICKNESS_VIEW
                              ? normalized_thickness(index, point.xy, cell, liquid_depth)
                              : normalized_state(index, point.xy, cell);
                color = Push.Palette == AUTOMATIC_HEATMAP_PALETTE
                        ? mix(background, Field.StateVisuals[index].rgb, value)
                        : heatmap(value);
            }
        }
        return (Push.AppearanceFlags & CELL_OUTLINE_OVERLAY) != 0u ? heatmap_cell_grid(color, uv) : color;
    }

// #endregion
