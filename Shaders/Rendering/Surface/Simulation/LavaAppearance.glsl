// 용암 접촉의 표시용 두께와 응고층·액상 용암의 외관을 계산한다.

// ============================================================
// 00. Lava Contact Appearance
// ============================================================
// #region 00_Lava_Contact_Appearance
    float lava_deposit_height(vec2 point)
    {
        vec2 uv = point / Field.SampleLayout.xy + 0.5;
        float axis = 0.52 + 0.11 * sin(6.0 * uv.x - 0.52);
        float edge = snow_grain(point * 360.0 + 37.0) - 0.5;
        float lobes = snow_grain(point * vec2(106.0, 147.0) + 24.0);
        float swell_distance = (uv.x - 0.48) / 0.063;
        float swell = exp(-swell_distance * swell_distance);
        float time = Push.CameraTime.w;
        float front = mix(0.09, 0.59, smoothstep(0.23, 1.7, time));
        float width = 0.038 + 0.024 * lobes + 0.016 * swell;
        float across = abs(uv.y - axis + 0.019 * (lobes - 0.5)) / width;
        float toe = front - 0.061 * pow(across, 1.65) + 0.026 * (lobes - 0.5);
        float banks = 1.0 - smoothstep(0.57, 1.0, across + 0.19 * edge);
        float length_mask = smoothstep(0.062, 0.12, uv.x) * (1.0 - smoothstep(toe - 0.033, toe, uv.x));
        // 형상 표본에는 흐름의 둥근 윤곽과 mm 단위 기복을 남긴다.
        float ridges = snow_grain(point * vec2(260.0, 510.0) + 5.0);
        float thickness = 0.0020 + 0.0025 * lobes + 0.0016 * ridges + 0.0012 * swell;
        return thickness * banks * length_mask * smoothstep(0.23, 0.50, time);
    }

    vec3 shade_lava(vec2 point, vec3 normal, vec3 view, vec3 light, float heat)
    {
        float coarse = snow_grain(point * vec2(220.0, 520.0));
        float rubble = snow_grain(point * 1580.0 + 15.0);
        // 냉각이 진행될수록 발광부 사이의 어두운 표면이 넓어진다.
        float fold = sin(1100.0 * point.y + 5.0 * coarse + 1.7 * sin(250.0 * point.x));
        float cracks = pow(1.0 - abs(fold), 3.4);
        float glowing = smoothstep(0.26, 0.77, heat + 0.33 * coarse + 0.29 * cracks - 0.27);
        glowing *= smoothstep(0.02, 0.4, heat);
        float diffuse = max(0.0, dot(normal, light));
        float specular = pow(max(0.0, dot(normal, normalize(view + light))), mix(14.0, 43.0, rubble));
        float stone = clamp(0.65 * coarse + 0.35 * rubble - 0.15 * cracks, 0.0, 1.0);
        vec3 crust = mix(vec3(0.040, 0.035, 0.031), vec3(0.153, 0.140, 0.125), stone);
        crust = crust * (0.52 + 0.48 * diffuse) + vec3(0.075) * specular;
        vec3 emission = mix(vec3(0.65, 0.045, 0.007), vec3(1.0, 0.52, 0.045), heat * cracks);
        return mix(crust, emission, glowing);
    }

    vec3 shade_molten_lava(vec2 point, float depth)
    {
        float grain = snow_grain(point * vec2(220.0, 520.0));
        float fold = sin(1100.0 * point.y + 5.0 * grain + 1.7 * sin(250.0 * point.x));
        float streaks = pow(1.0 - abs(fold), 3.4);
        // 두께는 발광 대비에만 사용해 얇은 액상 가장자리에도 주황색을 유지한다.
        float depth_mask = smoothstep(0.00008, 0.0018, depth);
        float glow = clamp(0.18 + 0.29 * grain + 0.32 * streaks + 0.21 * depth_mask, 0.0, 1.0);
        return mix(vec3(0.88, 0.18, 0.025), vec3(1.0, 0.64, 0.12), glow);
    }

// #endregion
