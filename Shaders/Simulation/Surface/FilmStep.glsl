// 아래층과 위층이 같은 스텝 시작 상태를 읽고, 반응 뒤 새 상태를 표시한다.

// ============================================================
// 00. Layer SurfaceCellState and Effective Bed
// ============================================================
// #region 00_Layer_State_and_Effective_Bed
    vec4 film_material(int layer)
    {
        return liquid_vec4(Data.FilmMaterialOffset + 10 * layer);
    }

    int film_entry(int layer, int region)
    {
        return layer * Data.RegionCount + region;
    }

    int film_word(int slot, int layer, int region)
    {
        return Data.FilmStateOffset + 3 * (slot * Data.LayerCount * Data.RegionCount + film_entry(layer, region));
    }

    vec3 film_state(int slot, int layer, int region)
    {
        int word = film_word(slot, layer, region);
        return vec3(liquid_float(word), liquid_float(word + 1), liquid_float(word + 2));
    }

    void write_film(int slot, int layer, int region, vec3 state)
    {
        int word = film_word(slot, layer, region);
        Data.Words[word] = floatBitsToUint(state.x);
        Data.Words[word + 1] = floatBitsToUint(state.y);
        Data.Words[word + 2] = floatBitsToUint(state.z);
    }

    float film_wetness(int layer, vec3 state)
    {
        float wet_water_fraction = liquid_float(Data.FilmMaterialOffset + 10 * layer + 9);
        return wet_water_fraction > 0.0 && state.x > 0.0 ?
               clamp(state.z / (state.x * wet_water_fraction), 0.0, 1.0) : 0.0;
    }

    float film_region_area(int region)
    {
        return Data.Shape.x * Data.Shape.y * float(load_regions(region).z);
    }

    // 수위가 아직 갱신되지 않은 반응 상태와 대조군의 아래층 질량으로 지지 형상을 복원한다.
    float film_floor_from_masses(int slot, int layer, int region, bool use_current, int omitted)
    {
        float floor_level = NO_FILM_FLOOR;
        for (int lower = 0; lower < layer; ++lower)
        {
            if (lower == omitted)
                continue;
            float mass = use_current ?
                         liquid_float(film_word(slot, lower, region))
                         : liquid_float(Data.FilmInitialMassOffset + film_entry(lower, region));
            if (mass > 0.0)
                floor_level = solve_level_for_density(load_regions(region), mass, film_material(lower).x, floor_level);
        }
        return floor_level;
    }

    bool use_controlled_floor(uint completed_step)
    {
        return (Data.FilmBedCoupling == 0 || Data.FilmOmittedSupport >= 0) &&
               float(completed_step) * Push.Seconds + 1e-6 >= Data.FilmBedFreezeStart;
    }

    float film_transport_floor(int slot, int layer, int region)
    {
        if (layer == Data.LayerCount - 1 && use_controlled_floor(Push.CompletedStep - 1u))
            return film_floor_from_masses(slot, layer, region, Data.FilmBedCoupling != 0, Data.FilmOmittedSupport);
        float floor_level = NO_FILM_FLOOR;
        for (int lower = 0; lower < layer; ++lower)
        {
            vec3 film = film_state(slot, lower, region);
            if (film.x > 0.0)
                floor_level = max(floor_level, film.y);
        }
        return floor_level;
    }

    float film_level(int layer, int region, float mass, float floor_level)
    {
        return solve_level_for_density(load_regions(region), mass, film_material(layer).x, floor_level);
    }

    float film_volume(int region, float level, float floor_level)
    {
        return level > floor_level ? volume(region, level) -
            (floor_level > NO_FILM_FLOOR ? volume(region, floor_level) : 0.0) : 0.0;
    }
// #endregion

// ============================================================
// 01. Conservative Face Transport
// ============================================================
// #region 01_Conservative_Face_Transport
    float film_face_mass(int slot, int layer, int index, ivec4 face)
    {
        if ((Data.FaceOperations & (face.y < 0 ? 2u : 1u)) == 0u)
            return 0.0;
        bool show_crossing = Push.DisplayInterval != 0u && layer == Data.LayerCount - 1 && face.y >= 0;
        vec4 material = film_material(layer);
        vec3 left_state = film_state(slot, layer, face.x);
        vec3 right_state = face.y < 0 ? vec3(0.0) : film_state(slot, layer, face.y);
        vec2 left = left_state.xy, right = right_state.xy;
        if (left.x <= 0.0 && right.x <= 0.0)
            return 0.0;
        int wet_material = Data.FilmMaterialOffset + 10 * layer + 7;
        if (liquid_float(wet_material + 2) > 0.0)
        {
            float wet = film_wetness(layer, face.y < 0 || left.y >= right.y ? left_state : right_state);
            material.y *= mix(liquid_float(wet_material), 1.0, wet);
            material.z = mix(liquid_float(wet_material + 1), material.z, wet);
        }
        float floor_left = film_transport_floor(slot, layer, face.x);
        float floor_right = face.y < 0 ? floor_left : film_transport_floor(slot, layer, face.y);
        vec2 distances = transport_face_distances(face);
        float integral = 0.0, lowest_sill = 1e10;
        int state_word = film_word(slot, layer, 0);
        for (int i = 0; i < face.w; ++i)
        {
            int contact = load_contacts(face.z + i);
            if (show_crossing)
                store_contact_weights(face.z + i, vec2(0.0));
            bool axis_x = contact % 2 == 0;
            int sample_index = contact / 2;
            int x = sample_index % Data.Counts.x, y = sample_index / Data.Counts.x;
            float left_bed = height_at(x, y);
            float right_bed = face.y < 0 ? left_bed :
                height_at(x + int(axis_x), y + int(!axis_x));
            float local_crest = max(left_bed, right_bed);
            float world_x = (float(x) + (axis_x ? 1.0 : 0.5)) * Data.Shape.x - 0.5 * Data.Shape.z;
            float altitude_offset = world_x * Push.Pose.x;
            float left_edge = max(left_bed * Push.Pose.y - altitude_offset, floor_left);
            float right_edge = max(right_bed * Push.Pose.y - altitude_offset, floor_right);
            float sill = max(left_edge, right_edge);
            lowest_sill = min(lowest_sill, sill);
            float right_level = face.y < 0 ? sill : right.y;
            float depth = max(0.0, max(left.y, right_level) - sill);
            float drop = max(left.y, sill) - max(right_level, sill);
            float distance = distances[axis_x ? 0 : 1];
            float mobility = transport_mobility(face, axis_x, drop / distance, depth, material, state_word, 3);
            float width = axis_x ? Data.Shape.y : Data.Shape.x;
            float contribution = width / distance * depth * depth * depth * drop * mobility;
            integral += contribution;
            float relief = max(0.00001, 0.06 * (axis_x ? Data.Shape.x : Data.Shape.y));
            bool recipient_dry = (contribution > 0.0 ? right.x : left.x) <= 0.0;
            if (show_crossing && contribution != 0.0 && (recipient_dry || min(left.y, right.y) + 1e-7 < sill))
            {
                ivec2 origin = ivec2(x, y);
                ivec2 direction = axis_x ? ivec2(1, 0) : ivec2(0, 1);
                bool exposed = abs(left_edge - right_edge) >= relief ||
                    (crest_falls_toward(origin, -direction, local_crest, relief) &&
                     crest_falls_toward(origin + direction, direction, local_crest, relief));
                if (exposed)
                    store_contact_weights(face.z + i, vec2(depth, abs(contribution)));
            }
        }
        if (show_crossing)
            store_face_integrals(index, abs(integral));
        float proposed = material.x * material.x * Gravity * Push.TransportSeconds * integral / (3.0 * material.y);
        if (proposed == 0.0)
            return 0.0;
        int donor = proposed > 0.0 ? face.x : face.y;
        int receiver = proposed > 0.0 ? face.y : face.x;
        vec2 from = proposed > 0.0 ? left : right;
        vec2 to = proposed > 0.0 ? right : left;
        float retain = receiver < 0 ? lowest_sill : max(lowest_sill, 0.5 * (from.y + to.y));
        float available = from.x - film_volume(donor, retain, proposed > 0.0 ? floor_left : floor_right) * material.x;
        int degree = max(4, load_face_counts(donor));
        if (receiver >= 0)
        {
            float capacity = film_volume(receiver, retain, proposed > 0.0 ? floor_right : floor_left) * material.x;
            available = min(available, capacity - to.x);
            degree = max(degree, load_face_counts(receiver));
        }
        return sign(proposed) * min(abs(proposed), max(0.0, available) / float(degree));
    }

    float film_candidate(int layer, int face)
    {
        return load_candidates(layer * Data.Counts.z + face);
    }

    float film_scale(int slot, int layer, int region)
    {
        int start = load_regions(region).w;
        int count = load_face_counts(region);
        float total = 0.0;
        for (int i = 0; i < count; ++i)
        {
            int index = load_region_faces(start + i);
            float candidate = film_candidate(layer, index);
            total += max(0.0, load_faces(index).x == region ? candidate : -candidate);
        }
        float mass = film_state(slot, layer, region).x;
        return total > mass ? mass / total : 1.0;
    }

    void gather_film(int slot, int layer, int region, inout float outlet)
    {
        int start = load_regions(region).w;
        int count = load_face_counts(region);
        vec3 state = film_state(slot, layer, region);
        for (int i = 0; i < count; ++i)
        {
            int index = load_region_faces(start + i);
            ivec4 face = load_faces(index);
            float candidate = film_candidate(layer, index);
            int donor = candidate >= 0.0 ? face.x : face.y;
            if (donor < 0)
                continue;
            float amount = candidate * load_out_scale(film_entry(layer, donor));
            if (Push.DisplayInterval != 0u && layer == Data.LayerCount - 1 &&
                face.y >= 0 && donor == region && amount != 0.0)
                record_overflow(index, face, amount);
            float mixed_water = 0.0;
            if (liquid_float(Data.FilmMaterialOffset + 10 * layer + 9) > 0.0)
            {
                vec3 donor_state = film_state(slot, layer, donor);
                mixed_water = donor_state.x > 0.0 ? amount * donor_state.z / donor_state.x : 0.0;
            }
            float direction = face.x == region ? -1.0 : 1.0;
            state.x += direction * amount;
            state.z += direction * mixed_water;
            if (face.y < 0)
                outlet += amount;
        }
        state.x = max(0.0, state.x);
        state.z = clamp(state.z, 0.0, state.x);
        write_film(slot ^ 1, layer, region, state);
    }
// #endregion

// ============================================================
// 02. Reactions and Display
// ============================================================
// #region 02_Reactions_and_Display
    void react_film(int slot, int region, float temperature, inout float dried)
    {
        store_liquid_vec4(Data.ReactionContactOffset + 4 * region, vec4(0.0));
        for (int i = 0; i < Data.FilmReactionCount; ++i)
        {
            int word = Data.FilmReactionOffset + 10 * i;
            int from = int(Data.Words[word]), to = int(Data.Words[word + 1]), water = int(Data.Words[word + 2]);
            float ratio = liquid_float(word + 4);
            vec3 source = film_state(slot, from, region);
            vec3 target = film_state(slot, to, region);
            if (ratio == 0.0)
            {
                float amount = liquid_float(word + 3) * film_region_area(region) * Push.Seconds;
                // 반응 방향은 같은 평형 온도의 어느 쪽에서 질량 변환이 일어나는지 지정한다.
                float drive = liquid_float(word + 9) * (temperature - liquid_float(word + 5));
                amount = min(source.x, amount * max(0.0, drive));
                source.x -= amount;
                target.x += amount;
            }
            else
            {
                float dry = target.z * (1.0 - exp(-liquid_float(word + 7) * Push.Seconds));
                target.xz -= vec2(dry);
                dried += dry;
                vec3 liquid = film_state(slot, water, region);
                bool eligible = source.x > 0.0 || target.x > 0.0;
                write_film(slot, to, region, target);
                bool control = water == Data.LayerCount - 1 && use_controlled_floor(Push.CompletedStep - 1u);
                float floor_level = film_floor_from_masses(slot, water, region, !control || Data.FilmBedCoupling != 0,
                                                          control ? Data.FilmOmittedSupport : -1);
                float level = liquid.x > 0.0 ? film_level(water, region, liquid.x, floor_level) : 0.0;
                float contact_area = record_reaction_contact(region, liquid.x, level, eligible, floor_level);
                if (contact_area > 0.0)
                {
                    if (float(Push.CompletedStep - 1u) * Push.Seconds + 1e-6 >= liquid_float(word + 8))
                    {
                        float capacity = max(0.0, (target.x - target.z) * ratio - target.z);
                        float rewet = min(min(liquid.x, capacity), liquid_float(word + 6) * contact_area * Push.Seconds);
                        liquid.x -= rewet;
                        target.xz += vec2(rewet);
                    }
                    float soil = min(min(source.x, liquid_float(word + 3) * contact_area * Push.Seconds), liquid.x / ratio);
                    float mixed = soil * ratio;
                    source.x -= soil;
                    liquid.x = max(0.0, liquid.x - mixed);
                    target.x += soil + mixed;
                    target.z += mixed;
                    write_film(slot, water, region, liquid);
                }
            }
            write_film(slot, from, region, source);
            write_film(slot, to, region, target);
        }
    }

    // 한 셀의 홈이 공유하는 바닥 상태는 한 invocation에서 한 번 갱신한다.
    void commit_film(int slot, int cell)
    {
        int scratch = slot ^ 1;
        int output_slot = Push.Sparse != 0u ? slot : scratch;
        int primary = Data.LayerCount - 1;
        SurfaceCellState state = load_states(slot * Data.Counts.y + cell);
        ivec2 regions = load_cell_regions(cell);
        CellUpdateParameters material = load_materials(cell);
        bool local_update = Push.TransportOnly == 0u;
        if (local_update)
            update_substrate_temperature(cell, material, state);

        float transported = 0.0;
        float contact_area = 0.0;
        bool quench = Data.SubstrateQuench.x > 0.0;
        bool water_update = local_update && Data.LocalRule == WATER_MATERIAL_RULE;
        for (int i = 0; i < regions.y; ++i)
        {
            int region = regions.x + i;
            for (int layer = 0; layer < Data.LayerCount; ++layer)
                gather_film(slot, layer, region, state.layer.z);
            if (local_update && Data.FilmReactionCount > 0)
                react_film(scratch, region, state.field.y, state.layer.w);
            float mass = film_state(scratch, primary, region).x;
            transported += mass;
            if (water_update)
            {
                bool control = use_controlled_floor(Push.CompletedStep - 1u);
                float floor_level = film_floor_from_masses(scratch, primary, region, !control || Data.FilmBedCoupling != 0,
                                                          control ? Data.FilmOmittedSupport : -1);
                float level = mass > 0.0 ? film_level(primary, region, mass, floor_level) : 0.0;
                bool eligible = material.rates.x > 0.0 || Data.SubstrateQuench.y > 0.0;
                float wet_area = record_reaction_contact(region, mass, level, eligible, floor_level);
                contact_area += quench && mass > 0.0 ? region_wet_area(region, level, THERMAL_CONTACT_DEPTH_M, floor_level)
                                                      : wet_area;
            }
        }

        state.layer.x = transported;
        float melted = 0.0;
        if (water_update)
        {
            if (Push.HasEditorInput != 0u)
                state.substrate.y += load_editor_ice_kg(cell);
            melted = update_water_material(cell, material, state, contact_area);
        }
        float before_removal = transported + melted;
        float retained = before_removal > 0.0 ? state.layer.x / before_removal : 0.0;
        ivec4 bounds = load_cells(cell);
        float melt_per_sample = melted / float((bounds.y - bounds.x) * (bounds.w - bounds.z));

        state.layer.y = -1e10;
        for (int i = 0; i < regions.y; ++i)
        {
            int region = regions.x + i;
            float occupied_level = NO_FILM_FLOOR;
            float liquid_floor = NO_FILM_FLOOR;
            vec3 color = vec3(0.0);
            float wetness = 0.0;
            for (int layer = 0; layer < Data.LayerCount; ++layer)
            {
                vec3 film = film_state(scratch, layer, region);
                if (layer == primary && water_update)
                    film.x = regions.y == 1 ? state.layer.x :
                             (film.x + melt_per_sample * float(load_regions(region).z)) * retained;
                if (layer == primary)
                    liquid_floor = use_controlled_floor(Push.CompletedStep - (local_update ? 0u : 1u))
                        ? film_floor_from_masses(scratch, layer, region, Data.FilmBedCoupling != 0, Data.FilmOmittedSupport)
                        : occupied_level;
                film.y = film_level(layer, region, film.x, layer == primary ? liquid_floor : occupied_level);
                write_film(output_slot, layer, region, film);
                if (layer == primary)
                {
                    state.layer.y = max(state.layer.y, film.y);
                    store_primary_liquid_region_state(output_slot * Data.RegionCount + region, film.xy);
                }
                else if (film.x > 0.0)
                {
                    occupied_level = max(occupied_level, film.y);
                    color = liquid_vec4(Data.FilmMaterialOffset + 10 * layer + 4).xyz;
                    wetness = film_wetness(layer, film);
                }
            }
            if (local_update && Data.DisplayLayers > 0)
            {
                int display_word = Data.FilmDisplayOffset + 6 * region;
                store_liquid_vec4(display_word, vec4(color, occupied_level));
                Data.Words[display_word + 4] = floatBitsToUint(wetness);
                Data.Words[display_word + 5] = floatBitsToUint(liquid_floor);
            }
        }
        store_states(output_slot * Data.Counts.y + cell, state);
    }

    void prepare_film(int slot, int region)
    {
        int layer = Data.LayerCount - 1;
        float floor_level = NO_FILM_FLOOR;
        // 자세가 바뀌면 한 영역의 아래층부터 복원한다.
        for (int lower_layer = 0; lower_layer < layer; ++lower_layer)
        {
            vec3 lower = film_state(slot, lower_layer, region);
            lower.y = film_level(lower_layer, region, lower.x, floor_level);
            write_film(slot, lower_layer, region, lower);
            if (lower.x > 0.0)
                floor_level = max(floor_level, lower.y);
        }
        vec3 film = film_state(slot, layer, region);
        float weight = Data.Counts.w < 0 ? load_scheduled_inlet_weights(region) : float(region == Data.Counts.w);
        film.x += Push.InputMass * weight;
        if (region == Push.DoseRegion)
            film.x += Push.DoseMassKg;
        if (Push.HasEditorInput != 0u)
            film.x += load_editor_liquid_kg(region);
        if (use_controlled_floor(Push.CompletedStep - 1u))
            floor_level = film_floor_from_masses(slot, layer, region, Data.FilmBedCoupling != 0, Data.FilmOmittedSupport);
        film.y = film_level(layer, region, film.x, floor_level);
        write_film(slot, layer, region, film);
    }
// #endregion
