// 저장 영역의 연결을 확인해 수면, 젖은 외관, 두께 진단에 쓰는 깊이를 복원한다.

// ============================================================
// 00. Connected Water Reconstruction
// ============================================================
// #region 00_Connected_Water_Reconstruction
    bool smooth_single_water_surface()
    {
        return liquid_option_enabled(SMOOTH_LIQUID_BOUNDARY) && Field.FilmDisplayWord < 0 &&
               Field.PhaseLevels.y <= Field.PhaseLevels.x &&
               (Push.AppearanceFlags & (LAVA_DEPOSIT | INCANDESCENT_LIQUID)) == 0u;
    }

    struct WaterSample
    {
        uint region;
        float mass;
        float level;
        float bed;
        float x;
        float base;
        float floor_level;
    };

    WaterSample water_sample(ivec2 sample_coord)
    {
        int index = sample_coord.y * Field.Counts.x + sample_coord.x;
        uint region = field_sample_regions(int(index));
        vec2 state = display_region(int(region));
        float x = (float(sample_coord.x) + 0.5) * Field.SampleLayout.z - 0.5 * Field.SampleLayout.x;
        float base = field_base_height(index);
        float floor_level = display_effective_floor(int(region));
        float bed = height_above_film_floor(base, x, floor_level);
        return WaterSample(region, state.x, state.y, bed, x, base, floor_level);
    }

    float sample_water_depth(WaterSample sample_state)
    {
        float bed_world_z = sample_state.bed * Push.Pose.y - sample_state.x * Push.Pose.x;
        return sample_state.mass > 0.0 ? max(0.0, sample_state.level - bed_world_z) : 0.0;
    }

    bool is_wet(WaterSample sample_state)
    {
        return sample_water_depth(sample_state) > 0.0;
    }

    float contact_sill(WaterSample a, WaterSample b)
    {
        float base = max(a.base, b.base) * Push.Pose.y - 0.5 * (a.x + b.x) * Push.Pose.x;
        return max(base, max(a.floor_level, b.floor_level));
    }

    uint reachable_samples(uint seeds, uint links[4])
    {
        uint reachable = seeds;
        for (int pass = 0; pass < 3; ++pass)
            for (uint i = 0u; i < 4u; ++i)
                if ((reachable & (1u << i)) != 0u)
                    reachable |= links[i];
        return reachable;
    }

    float shore_appearance_depth(vec2 point, uint owner, float owner_level, float owner_bed_world_z,
                                 bool connected_surface, bool sample_depth_diagnostic)
    {
        vec2 sample_at = (point + 0.5 * Field.SampleLayout.xy) / Field.SampleLayout.zw - 0.5;
        vec2 grid = clamp(sample_at, vec2(0.0), vec2(Field.Counts.xy - 1));
        ivec2 lo = ivec2(grid);
        ivec2 hi = min(lo + 1, Field.Counts.xy - 1);
        vec2 fraction = fract(grid);
        bool single_water_surface = connected_surface && !sample_depth_diagnostic && Field.FilmDisplayWord < 0;
        // 선형 수위를 그대로 복원해 연속 경사면의 수면에 표본 단위 요철이 생기지 않게 한다.
        bool linear = sample_depth_diagnostic || (connected_surface && Field.FilmDisplayWord < 0);
        vec2 f = linear ? fraction : smoothstep(0.0, 1.0, fraction);
        WaterSample nodes[4] = WaterSample[4](water_sample(lo), water_sample(ivec2(hi.x, lo.y)),
                                             water_sample(ivec2(lo.x, hi.y)), water_sample(hi));
        bool same_region = nodes[0].region == owner && nodes[1].region == owner &&
                           nodes[2].region == owner && nodes[3].region == owner;
        if (!sample_depth_diagnostic && same_region)
            return owner_level - owner_bed_world_z;

        uint seeds = 0u;
        bool wet[4];
        for (uint i = 0u; i < 4u; ++i)
        {
            wet[i] = is_wet(nodes[i]);
            if (nodes[i].region == owner && (wet[i] || (single_water_surface && nodes[i].mass > 0.0)))
                seeds |= 1u << i;
        }
        if (seeds == 0u)
            return sample_depth_diagnostic ? 0.0 : owner_level - owner_bed_world_z;

        uint pool_links[4] = uint[4](1u, 2u, 4u, 8u);
        uint shore_links[4] = uint[4](1u, 2u, 4u, 8u);
        const uvec2 edges[4] = uvec2[4](uvec2(0, 1), uvec2(0, 2), uvec2(1, 3), uvec2(2, 3));
        for (int i = 0; i < 4; ++i)
        {
            uint a = edges[i].x, b = edges[i].y;
            // 떨어진 시편 사이의 물가는 차트상으로 이웃해도 서로 이어지지 않는다.
            if (Field.SpecimenCount > 1u && specimen_at(nodes[a].x) != specimen_at(nodes[b].x))
                continue;
            bool both_wet = wet[a] && wet[b];
            float sill = contact_sill(nodes[a], nodes[b]);
            float bank = abs(nodes[a].bed - nodes[b].bed);
            float lower_level = min(nodes[a].level, nodes[b].level);
            float upper_level = max(nodes[a].level, nodes[b].level);
            bool flowing_bank = connected_surface && upper_level > sill &&
                                bank < 0.35 * min(Field.SampleLayout.z, Field.SampleLayout.w);
            bool connected_pool = both_wet &&
                                  (lower_level > sill || (flowing_bank && (Field.FilmDisplayWord >= 0 || sample_depth_diagnostic)));
            // 물가에서 한쪽 표본이 말라도 양쪽 저장 영역에 물이 있고 높은 쪽 수위가
            // 완만한 접촉을 넘으면 공용 수위를 복원한다. 마른 턱은 기존 기준으로 차단한다.
            bool flowing_shore = single_water_surface && flowing_bank && (wet[a] || wet[b]) &&
                                 min(nodes[a].mass, nodes[b].mass) > 0.0;
            if (connected_pool || flowing_shore)
            {
                pool_links[a] |= 1u << b;
                pool_links[b] |= 1u << a;
            }
            // 물가의 양쪽 표본이 이미 젖었다면 턱에 닿는 쪽의 수위를 사용해 얇은 윤곽을 잇는다.
            if (nodes[a].region == nodes[b].region || (both_wet && upper_level > sill))
            {
                shore_links[a] |= 1u << b;
                shore_links[b] |= 1u << a;
            }
        }
        uint pool = reachable_samples(seeds, pool_links);
        uint shore = reachable_samples(seeds, shore_links);

        vec4 weights = vec4((1.0 - f.x) * (1.0 - f.y), f.x * (1.0 - f.y), (1.0 - f.x) * f.y, f.x * f.y);
        vec2 pool_sum = vec2(0.0);
        vec2 shore_sum = vec2(0.0);
        float sampled_depth = 0.0;
        float disconnected_weight = 0.0;
        for (uint i = 0u; i < 4u; ++i)
        {
            vec2 contribution = weights[i] * vec2(nodes[i].level, 1.0);
            if ((pool & (1u << i)) != 0u)
            {
                pool_sum += contribution;
                if (sample_depth_diagnostic)
                    sampled_depth += weights[i] * sample_water_depth(nodes[i]);
            }
            else if (nodes[i].region != owner)
                disconnected_weight += weights[i];
            if ((shore & (1u << i)) != 0u)
                shore_sum += contribution;
        }

        float pool_level = pool_sum.y > 0.0 ? pool_sum.x / pool_sum.y : owner_level;
        float shore_level = shore_sum.y > 0.0 ? shore_sum.x / shore_sum.y : owner_level;
        float depth = pool_level - owner_bed_world_z;
        if (sample_depth_diagnostic)
        {
            // 수위만 고른 뒤 바닥을 빼면 마른 경사면에도 표본 폭의 쐐기가 생긴다.
            // 실제 표본 수심을 먼저 보간하고, 수위가 바닥 아래인 곳과 분리된 홈으로의 번짐을 막는다.
            float boundary_coverage = 1.0 - smoothstep(0.0, 0.5, disconnected_weight);
            return min(sampled_depth, max(0.0, depth)) * boundary_coverage;
        }
        if (connected_surface)
            return depth;
        float shore_depth = shore_level - owner_bed_world_z;
        // 바닥에 쓰이는 젖은 외관의 보간 폭을 수면 복원과 별도로 제한한다.
        depth = mix(shore_depth, depth, smoothstep(0.50, 1.0, pool_sum.y));
        return min(depth, 0.0) + max(depth, 0.0) * smoothstep(0.0, 1.0, shore_sum.y);
    }

    float film_diagnostic_depth(vec2 point)
    {
        vec2 chart = (point + 0.5 * Field.SampleLayout.xy) / Field.SampleLayout.zw;
        ivec2 at = ivec2(clamp(chart, vec2(0.0), vec2(Field.Counts.xy) - 0.001));
        WaterSample sample_state = water_sample(at);
        float base = field_base_height(at.y * Field.Counts.x + at.x);
        float physical_bed = max(base * Push.Pose.y - point.x * Push.Pose.x,
                                 display_effective_floor(int(sample_state.region)));
        float depth = sample_state.level - physical_bed;
        if (sample_state.mass <= 0.0 || depth <= 0.0)
            return 0.0;
        if (!liquid_option_enabled(SMOOTH_LIQUID_BOUNDARY))
            return depth;

        // 샘플마다 수송용 바닥을 빼고 나서 연결된 수심을 보간한다.
        // 표시용 얼음 형상은 Fixed Initial Bed에서 수송용 바닥과 다를 수 있다.
        float sampled_depth = shore_appearance_depth(point, sample_state.region, sample_state.level, physical_bed,
                                                     true, true);
        // 보간된 표시 바닥이 수면 위로 올라온 곳에는 두께를 칠하지 않는다.
        float visible_depth = sample_state.level - (bed_height(point) * Push.Pose.y - point.x * Push.Pose.x);
        return min(sampled_depth, max(0.0, visible_depth));
    }

// #endregion
