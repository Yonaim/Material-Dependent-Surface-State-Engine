// ============================================================
// 00. Substrate Temperature
// ============================================================
// #region 00_Substrate_Temperature
    float combustion_temperature(int cell, SurfaceCellState water, float prescribed, float ambient)
    {
        int columns = Data.CombustionGridColumns;
        int column = cell % columns;
        int base = int(Push.ReadSlot) * Data.Counts.y + cell;
        float flame = water.field.x;
        if (column > 0)
            flame = max(flame, load_states(base - 1).field.x);
        if (column + 1 < columns)
            flame = max(flame, load_states(base + 1).field.x);
        if (cell >= columns)
            flame = max(flame, load_states(base - columns).field.x);
        if (cell + columns < Data.Counts.y)
            flame = max(flame, load_states(base + columns).field.x);
        // 같은 디스패치에서 쓰는 다음 슬롯을 읽지 않아 전파가 셀 처리 순서에 의존하지 않는다.
        float target = max(prescribed, ambient + 500.0 * min(1.0, 2.0 * flame));
        return water.field.y + (target - water.field.y) * min(1.0, 8.0 * Push.Seconds);
    }

    void update_substrate_temperature(int cell, CellUpdateParameters material, inout SurfaceCellState state)
    {
        float editor_temperature = Push.HasEditorInput != 0u ? load_editor_temperature_k(cell) : 0.0;
        if (editor_temperature > 0.0)
            state.field.y = editor_temperature;
        else if (Data.SubstrateQuench.x <= 0.0)
        {
            const float ambient = 293.15;
            float target = Push.TemperatureTargetWeight == 1.0 ? material.thermal.x :
                           ambient + (material.thermal.x - ambient) * Push.TemperatureTargetWeight;
            float temperature = target + Push.TemperatureOffsetK;
            state.field.y = Data.CombustionGridColumns > 0 ? combustion_temperature(cell, state, temperature, ambient) :
                            temperature;
        }
    }
// #endregion

// ============================================================
// 01. Water CellUpdateParameters Rule
// ============================================================
// #region 01_Water_Material_Rule
    const uint WATER_MATERIAL_RULE = 1u; // ELocalMaterialRule::Water

    void quench_substrate(inout SurfaceCellState water, float area, float contact_area)
    {
        const float boiling_k = 373.15;
        const float vaporization_j_kg = 2.591e6;
        float excess_k = max(0.0, water.field.y - boiling_k);
        float capacity_j_k = Data.SubstrateQuench.x * area;
        float heat_j = min(Data.SubstrateQuench.y * contact_area * excess_k * Push.Seconds,
                           min(capacity_j_k * excess_k, water.layer.x * vaporization_j_kg));
        float vapor_kg = min(water.layer.x, heat_j / vaporization_j_kg);
        water.layer.x -= vapor_kg;
        water.layer.w += vapor_kg;
        water.substrate.w = vapor_kg / Push.Seconds;
        water.field.y -= vapor_kg * vaporization_j_kg / capacity_j_k;
    }

    float update_water_material(int cell, CellUpdateParameters material, inout SurfaceCellState water, float contact_area)
    {
        float area = cell_area(load_cells(cell));
        if (Data.SubstrateQuench.x > 0.0)
        {
            quench_substrate(water, area, contact_area);
            return 0.0;
        }
        float temperature = water.field.y;
        float melt = min(water.substrate.y, material.thermal.y * max(0.0, temperature - 273.15) * area * Push.Seconds);
        water.substrate.y -= melt;
        water.layer.x += melt;
        float frozen = min(water.layer.x, material.thermal.w * max(0.0, 273.15 - temperature) * area * Push.Seconds);
        water.layer.x -= frozen;
        water.substrate.y += frozen;
        float absorb = min(water.layer.x * material.rates.x * clamp(contact_area / area, 0.0, 1.0),
                           max(0.0, material.rates.z * area - water.substrate.x));
        water.substrate.x += absorb;
        water.layer.x -= absorb;
        float dry = water.substrate.x * material.rates.y;
        water.substrate.x -= dry;
        bool evaporate_absorbed = Data.EvaporateAbsorbedWater != 0u;
        float threshold_k = evaporate_absorbed ? 373.15 : 323.15;
        float vapor_budget_kg = material.thermal.z * max(0.0, temperature - threshold_k) * area * Push.Seconds;
        float surface_vapor_kg = min(water.layer.x, vapor_budget_kg);
        water.layer.x -= surface_vapor_kg;
        float absorbed_vapor_kg = evaporate_absorbed ? min(water.substrate.x, vapor_budget_kg - surface_vapor_kg) : 0.0;
        water.substrate.x -= absorbed_vapor_kg;
        float vapor_kg = surface_vapor_kg + absorbed_vapor_kg;
        water.layer.w += vapor_kg + dry;
        water.substrate.w = vapor_kg / Push.Seconds;
        float wet = clamp((water.layer.x + water.substrate.x) / (area * 0.4), 0.0, 1.0);
        float heat = clamp((temperature - 475.0) / 200.0, 0.0, 1.0);
        float activity = material.rates.w * (1.0 - water.substrate.z) * heat * (1.0 - wet);
        water.substrate.z = min(1.0, water.substrate.z + activity * Push.Seconds);
        water.field.x = clamp(activity, 0.0, 1.0);
        if (material.thermal.w > 0.0)
            water.field.w = restore_cell_level(cell, water.layer.x + water.substrate.y * (Data.LiquidProperties.x / Data.IceDensity));
        return melt;
    }

// #endregion
