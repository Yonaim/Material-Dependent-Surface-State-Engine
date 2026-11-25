// 젖은 내부에서만 수면 높이를 움직이는 부피 보존 표시 모델.

// ============================================================
// 00. Conservative Height Slosh
// ============================================================
// #region 00_Conservative_Height_Slosh
    vec3 water_height_slosh(int cell, vec2 point, float level)
    {
        int column = cell % Field.Counts.z;
        int row = cell / Field.Counts.z;
        vec2 lower = vec2(field_edgex(column), field_edgey(row));
        vec2 upper = vec2(field_edgex(column + 1), field_edgey(row + 1));
        vec2 size = upper - lower;
        vec2 sample_count = round(size / Field.SampleLayout.zw);
        // 반대 부호의 내부 정점을 그리려면 좌우 두 점과 위아래 최소 한 점이 필요하다.
        if (any(lessThan(sample_count, vec2(3.0, 2.0))))
            return vec3(0.0);

        float highest_bed = field_cell_max_bed(cell) * Push.Pose.y -
                            min(lower.x * Push.Pose.x, upper.x * Push.Pose.x);
        float clearance = level - highest_bed;
        if (clearance <= 0.00001)
            return vec3(0.0);

        vec2 samples = round((point - lower) / Field.SampleLayout.zw);
        vec2 uv = clamp(samples / sample_count, 0.0, 1.0);
        vec2 envelope = uv * (1.0 - uv);
        vec2 square = envelope * envelope;
        float x = uv.x - 0.5;
        // 좌우 홀대칭이고 모든 셀 테두리에서 값과 기울기가 0이다.
        // 샘플 메시의 투영 삼각형 면적이 같으므로 두 로브의 적분이 상쇄된다.
        float scale = 1700.0 * slosh_amplitude() * min(0.55 * clearance, 0.0015);
        float height = scale * x * square.x * square.y;
        vec2 slope = scale * vec2((square.x + 2.0 * x * envelope.x * (1.0 - 2.0 * uv.x)) * square.y,
                                  x * square.x * 2.0 * envelope.y * (1.0 - 2.0 * uv.y)) /
                     (size * vec2(Push.Pose.y, 1.0));
        return vec3(slope, height);
    }

// #endregion
