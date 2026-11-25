// CPU 및 GPU 수송에서 얻은 셀 상태와 정적 형상을 읽어 화면 표시에 사용한다.

// ============================================================
// 00. Display Buffers
// ============================================================
// #region 00_Display_Buffers
    // DisplayState는 FWater의 48바이트 셀 배열이다. SurfaceRenderField는 FSurfaceRenderField의 로컬 차트와 샘플 격자를 읽는다.

    struct SurfaceCellState
    {
        // 표면 질량(kg), 수위(m), 외부 유출과 증발 누적량(kg).
        vec4 layer;
        // 흡수수와 고체 물(kg), 탄화율(0~1), 수증기 발생률(kg/s).
        vec4 substrate;
        // 불 활성도, 지정 온도(K), 역면적(1/m²), 합성 표시 수위(m).
        vec4 field;
    };

    struct OverflowTransferRecord
    {
        float mass_kg;
        float head_m;
        uint contact;
        uint receiver;
        uint interval;
        float last_transfer_s;
    };

    layout(std430, set = 0, binding = 0) readonly buffer DisplayState
    {
        uint Words[];
    } Display;

    layout(std430, set = 0, binding = 4) readonly buffer SurfaceRenderField
    {
        vec4 BoundsMin;
        vec4 BoundsMax;
        vec4 SampleLayout;
        ivec4 Counts;
        vec4 BedMetrics;
        vec4 IceProperties;
        ivec4 StateChannels[3];
        ivec4 StateGroups;
        vec4 StateVisuals[12];
        vec4 StateThicknessInverseMeters[3];
        vec4 LiquidOptics;
        vec4 SubstrateOptics;
        vec2 IceOptics;
        float SubstrateReliefMeters;
        float DisplayGapMeters;
        vec2 PhaseLevels;
        float CharContrast;
        int RegionCount;
        vec2 EnvironmentOptics;
        uint SpecimenCount;
        uint MetallicSpecimens;
        vec4 LiquidTint;
        vec4 SectionWindow;
        vec4 VesselBounds;
        vec4 SectionSolidBed;
        vec4 AbsorbedWetnessDepth;
        int CenterXOffset;
        int CenterYOffset;
        int HeightsOffset;
        int EdgeXOffset;
        int EdgeYOffset;
        int CellRegionCountsOffset;
        int SampleRegionsOffset;
        int RegionInverseAreasOffset;
        int ContactIndicesOffset;
        int FilmDisplayWord;
        int FilmDensitiesOffset;
        int FilmIceLayer;
        int ReactionContactWord;
        int AbsorbCapacityOffset;
        uint Words[];
    } Field;

    vec4 display_vec4(int w)
    {
        return uintBitsToFloat(uvec4(Display.Words[w], Display.Words[w + 1], Display.Words[w + 2], Display.Words[w + 3]));
    }

    SurfaceCellState display_cell(int index)
    {
        int w = index * 12;
        return SurfaceCellState(display_vec4(w), display_vec4(w + 4), display_vec4(w + 8));
    }

    vec2 display_region(int index)
    {
        int w = 12 * Field.Counts.z * Field.Counts.w + 2 * index;
        return uintBitsToFloat(uvec2(Display.Words[w], Display.Words[w + 1]));
    }

    vec4 display_film(int region)
    {
        return Field.FilmDisplayWord < 0 ? vec4(0.0) : display_vec4(Field.FilmDisplayWord + 6 * region);
    }

    float display_coating_wetness(int region)
    {
        return uintBitsToFloat(Display.Words[Field.FilmDisplayWord + 6 * region + 4]);
    }

    vec2 display_film_composition(int layer, int region)
    {
        int word = Field.FilmDisplayWord + 6 * Field.RegionCount + 3 * (layer * Field.RegionCount + region);
        return uintBitsToFloat(uvec2(Display.Words[word], Display.Words[word + 2]));
    }

    float display_film_mass(int layer, int region)
    {
        return display_film_composition(layer, region).x;
    }

    float display_effective_floor(int region)
    {
        return Field.FilmDisplayWord >= 0
               ? uintBitsToFloat(Display.Words[Field.FilmDisplayWord + 6 * region + 5]) : -1e10;
    }

    vec4 display_reaction_contact(int region)
    {
        return display_vec4(Field.ReactionContactWord + 4 * region);
    }

    float field_absorb_capacity(int cell)
    {
        return uintBitsToFloat(Field.Words[Field.AbsorbCapacityOffset + cell]);
    }

    float field_base_height(int index)
    {
        return uintBitsToFloat(Field.Words[Field.HeightsOffset + index]);
    }

    float film_density(int layer)
    {
        return uintBitsToFloat(Field.Words[Field.FilmDensitiesOffset + layer]);
    }

    OverflowTransferRecord display_overflow(int index)
    {
        int w = 12 * Field.Counts.z * Field.Counts.w + 2 * Field.RegionCount + 6 * index;
        return OverflowTransferRecord(uintBitsToFloat(Display.Words[w]), uintBitsToFloat(Display.Words[w + 1]),
                            Display.Words[w + 2], Display.Words[w + 3], Display.Words[w + 4],
                            uintBitsToFloat(Display.Words[w + 5]));
    }

    float field_centerx(int index) { return uintBitsToFloat(Field.Words[Field.CenterXOffset + index]); }
    float field_centery(int index) { return uintBitsToFloat(Field.Words[Field.CenterYOffset + index]); }
    float height_above_film_floor(float base, float chart_x, float floor_level)
    {
        return max(base, (floor_level + chart_x * Push.Pose.x) / Push.Pose.y);
    }

    float field_heights(int index)
    {
        float base = field_base_height(index);
        if (Field.FilmDisplayWord < 0)
            return base;
        float chart_x = (float(index % Field.Counts.x) + 0.5) * Field.SampleLayout.z - 0.5 * Field.SampleLayout.x;
        return height_above_film_floor(base, chart_x,
                                     display_film(int(Field.Words[Field.SampleRegionsOffset + index])).w);
    }
    float field_edgex(int index) { return uintBitsToFloat(Field.Words[Field.EdgeXOffset + index]); }
    float field_edgey(int index) { return uintBitsToFloat(Field.Words[Field.EdgeYOffset + index]); }
    int field_cell_region_counts(int index) { return int(Field.Words[Field.CellRegionCountsOffset + index]); }
    uint field_sample_regions(int index) { return Field.Words[Field.SampleRegionsOffset + index]; }
    float field_region_inverse_areas(int index) { return uintBitsToFloat(Field.Words[Field.RegionInverseAreasOffset + index]); }
    float field_cell_max_bed(int index)
    {
        int first_bound = Field.ContactIndicesOffset - Field.Counts.z * Field.Counts.w;
        return uintBitsToFloat(Field.Words[first_bound + index]);
    }

    int field_contact_indices(int index) { return int(Field.Words[Field.ContactIndicesOffset + index]); }

    int field_contact_slot(int index)
    {
        return field_contact_indices(2 * Field.Counts.x * Field.Counts.y + index);
    }

// #endregion

// ============================================================
// 01. Chart and Cell Coordinates
// ============================================================
// #region 01_Chart_and_Cell_Coordinates
    // 로컬 위치(m)를 높이 표본과 계산 셀 중심에 대응시킨다. 두 격자의 해상도는 독립적이다.

    int specimen_at(float chart_x)
    {
        float across = (chart_x / Field.SampleLayout.x + 0.5) * float(Field.SpecimenCount);
        return clamp(int(across), 0, int(Field.SpecimenCount) - 1);
    }

    int cell_specimen(uint cell)
    {
        return int(cell % uint(Field.Counts.z)) / (Field.Counts.z / int(Field.SpecimenCount));
    }

    float display_gap_offset(int specimen)
    {
        return (float(specimen) - 0.5 * float(Field.SpecimenCount - 1u)) * Field.DisplayGapMeters;
    }

    int chart_sample_at(vec2 point)
    {
        vec2 at = (point + 0.5 * Field.SampleLayout.xy) / Field.SampleLayout.zw;
        ivec2 sample_index = ivec2(clamp(at, vec2(0.0), vec2(Field.Counts.xy - 1)));
        return sample_index.y * Field.Counts.x + sample_index.x;
    }

    uint region_at(vec2 point)
    {
        return field_sample_regions(chart_sample_at(point));
    }

    vec2 vessel_sample_point(vec2 point)
    {
        vec2 margin = min(0.5 * Field.SampleLayout.zw, 0.25 * (Field.VesselBounds.zw - Field.VesselBounds.xy));
        return clamp(point, Field.VesselBounds.xy + margin, Field.VesselBounds.zw - margin);
    }

    float bed_height(vec2 point)
    {
        vec2 sample_at = (point + 0.5 * Field.SampleLayout.xy) / Field.SampleLayout.zw - 0.5;
        vec2 grid = clamp(sample_at, vec2(0.0), vec2(Field.Counts.xy - 1));
        ivec2 a = ivec2(grid);
        ivec2 b = min(a + 1, Field.Counts.xy - 1);
        vec2 f = fract(grid);
        return mix(mix(field_heights(int(a.y * Field.Counts.x + a.x)), field_heights(int(a.y * Field.Counts.x + b.x)), f.x),
                   mix(field_heights(int(b.y * Field.Counts.x + a.x)), field_heights(int(b.y * Field.Counts.x + b.x)), f.x), f.y);
    }

    float cell_coordinate(float position, bool along_x)
    {
        int last = (along_x ? Field.Counts.z : Field.Counts.w) - 1;
        int lo = 0;
        int hi = last;
        while (hi - lo > 1)
        {
            int midpoint = (lo + hi) / 2;
            float center = along_x ? field_centerx(int(midpoint)) : field_centery(int(midpoint));
            if (position < center)
                hi = midpoint;
            else
                lo = midpoint;
        }

        float a = along_x ? field_centerx(int(lo)) : field_centery(int(lo));
        float b = along_x ? field_centerx(int(hi)) : field_centery(int(hi));
        return float(lo) + clamp((position - a) / max(b - a, 1e-5), 0.0, float(hi - lo));
    }

// #endregion

// ============================================================
// 02. Display SurfaceCellState Sampling
// ============================================================
// #region 02_Display_State_Sampling
    // 셀 중심의 불 활성도, 탄화율, 온도(K), 얼음 면밀도(kg/m²)를 표시용으로 보간한다.

    vec4 display_state(ivec2 cell)
    {
        SurfaceCellState state = display_cell(int(cell.y * Field.Counts.z + cell.x));
        return vec4(state.field.x, state.substrate.z, state.field.y, state.substrate.y * state.field.z);
    }

    vec4 sample_display_state(vec2 point)
    {
        vec2 grid = vec2(cell_coordinate(point.x, true), cell_coordinate(point.y, false));
        ivec2 a = ivec2(grid);
        ivec2 b = min(a + 1, Field.Counts.zw - 1);
        vec2 f = smoothstep(0.0, 1.0, fract(grid));
        vec4 left = mix(display_state(a), display_state(ivec2(a.x, b.y)), f.y);
        vec4 right = mix(display_state(ivec2(b.x, a.y)), display_state(b), f.y);
        vec2 margin = 0.5 * Field.SampleLayout.xy - abs(point);
        vec4 value = mix(left, right, f.x);
        value.x *= smoothstep(-0.004, 0.002, min(margin.x, margin.y));
        return value;
    }

    vec2 surface_combustion(vec2 point)
    {
        return sample_display_state(point).xy;
    }

    vec2 coating_masses_at(ivec2 cell)
    {
        SurfaceCellState state = display_cell(int(cell.y * Field.Counts.z + cell.x));
        return state.substrate.xy * state.field.z;
    }

    vec2 sample_coating_masses(vec2 point)
    {
        vec2 grid = vec2(cell_coordinate(point.x, true), cell_coordinate(point.y, false));
        ivec2 a = ivec2(grid);
        ivec2 b = min(a + 1, Field.Counts.zw - 1);
        vec2 f = smoothstep(0.0, 1.0, fract(grid));
        // 시편의 흡수수는 표시 보간에서도 양쪽 경계를 넘어 섞이지 않는다.
        if (Field.SpecimenCount > 1u)
        {
            int width = Field.Counts.z / int(Field.SpecimenCount);
            int first = specimen_at(point.x) * width;
            a.x = clamp(a.x, first, first + width - 1);
            b.x = clamp(b.x, first, first + width - 1);
        }
        vec2 lo = mix(coating_masses_at(a), coating_masses_at(ivec2(b.x, a.y)), f.x);
        vec2 hi = mix(coating_masses_at(ivec2(a.x, b.y)), coating_masses_at(b), f.x);
        return mix(lo, hi, f.y);
    }

// #endregion

// ============================================================
// 03. Snow Height Reconstruction
// ============================================================
// #region 03_Snow_Height_Reconstruction
    // 정점과 프래그먼트가 같은 두께를 읽어 눈 윤곽과 음영의 경계를 맞춘다.

    float snow_hash(vec2 point)
    {
        vec3 p = fract(vec3(point.xyx) * 0.1031);
        p += dot(p, p.yzx + 33.33);
        return fract((p.x + p.y) * p.z);
    }

    float snow_grain(vec2 point)
    {
        vec2 cell = floor(point);
        vec2 f = smoothstep(0.0, 1.0, fract(point));
        float lo = mix(snow_hash(cell), snow_hash(cell + vec2(1.0, 0.0)), f.x);
        float hi = mix(snow_hash(cell + vec2(0.0, 1.0)), snow_hash(cell + vec2(1.0)), f.x);
        return mix(lo, hi, f.y);
    }

    float snow_layer_height(vec2 point)
    {
        vec2 warp = vec2(snow_grain(point * 310.0 + 3.0), snow_grain(point * 290.0 + 9.0)) - 0.5;
        float thickness = sample_coating_masses(point + 0.004 * warp).y / Field.IceProperties.z;
        float clump = snow_grain(point * 540.0 + 15.0);
        vec2 margin = 0.5 * Field.SampleLayout.xy - abs(point);
        // 천 끝과 얇은 눈 경계에서 높이를 0으로 보내 바닥 메시와 이어준다.
        return thickness * smoothstep(0.00005, mix(0.00021, 0.00057, clump), thickness) *
               smoothstep(0.0, 0.002, min(margin.x, margin.y));
    }

// #endregion
