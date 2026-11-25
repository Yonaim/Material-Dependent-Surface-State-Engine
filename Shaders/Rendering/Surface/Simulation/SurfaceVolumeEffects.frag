#version 450

// 표면의 연소 활성도와 증발률에서 불꽃 및 응결 구름의 체적 밀도를 적분한다.
#extension GL_GOOGLE_include_directive : require

// ============================================================
// 00. Flame Interface
// ============================================================
// #region 00_Flame_Interface
    // SurfaceVolumeEffects.vert에서 온 로컬 위치(m)와 화면 출력, FDrawPush를 사용한다.

    layout(location = 0) in vec3 Local;
    layout(location = 0) out vec4 Color;

    #include "SurfaceDrawParameters.glsl"
    #include "SurfaceDisplayField.glsl"

// #endregion

// ============================================================
// 01. Volume Integration
// ============================================================
// #region 01_Volume_Integration
    // 바닥 위로 제한한 구간에서 불 활성도를 읽고 단계별 투과율로 색을 누적한다.

    const int MAX_STEPS = 46;
    const float STEP_METERS = 0.00115;

    float hash_noise(vec3 p)
    {
        p = fract(p * 0.1031);
        p += dot(p, p.yxz + 33.33);
        return fract((p.x + p.y) * p.z);
    }

    float noise3(vec3 p)
    {
        vec3 i = floor(p);
        vec3 f = fract(p);
        vec3 u = f * f * (3.0 - 2.0 * f);
        float lower = mix(mix(hash_noise(i), hash_noise(i + vec3(1, 0, 0)), u.x),
                          mix(hash_noise(i + vec3(0, 1, 0)), hash_noise(i + vec3(1, 1, 0)), u.x), u.y);
        float upper = mix(mix(hash_noise(i + vec3(0, 0, 1)), hash_noise(i + vec3(1, 0, 1)), u.x),
                          mix(hash_noise(i + vec3(0, 1, 1)), hash_noise(i + vec3(1, 1, 1)), u.x), u.y);
        return mix(lower, upper, u.z);
    }

    float steam_source(vec2 point)
    {
        vec2 margin = 0.5 * Field.SampleLayout.xy - abs(point);
        if (min(margin.x, margin.y) < 0.0)
            return 0.0;
        vec2 grid = vec2(cell_coordinate(point.x, true), cell_coordinate(point.y, false));
        ivec2 a = ivec2(grid);
        ivec2 b = min(a + 1, Field.Counts.zw - 1);
        vec2 f = smoothstep(0.0, 1.0, fract(grid));
        SurfaceCellState aa = display_cell(int(a.y * Field.Counts.z + a.x));
        SurfaceCellState ba = display_cell(int(a.y * Field.Counts.z + b.x));
        SurfaceCellState ab = display_cell(int(b.y * Field.Counts.z + a.x));
        SurfaceCellState bb = display_cell(int(b.y * Field.Counts.z + b.x));
        vec2 rate = mix(vec2(aa.substrate.w * aa.field.z, ab.substrate.w * ab.field.z),
                        vec2(ba.substrate.w * ba.field.z, bb.substrate.w * bb.field.z), f.x);
        return mix(rate.x, rate.y, f.y) * smoothstep(0.0, 0.005, min(margin.x, margin.y));
    }

    vec4 integrate_steam(vec3 eye, vec3 direction, float first, float last)
    {
        vec4 cloud = vec4(0.0);
        const int STEPS = 32;
        float dt = max(0.0017, (last - first) / float(STEPS));
        float jitter = 0.2 + 0.6 * hash_noise(vec3(gl_FragCoord.xy, 41.0));
        float time = Push.CameraTime.w;
        for (int i = 0; i < STEPS; ++i)
        {
            float distance = first + (float(i) + jitter) * dt;
            if (distance >= last || cloud.a > 0.90)
                break;
            vec3 p = eye + direction * distance;
            float height = p.z - bed_height(p.xy);
            vec2 margin = 0.5 * Field.SampleLayout.xy - abs(p.xy);
            if (height < -0.0008 && min(margin.x, margin.y) >= 0.0)
                break;
            if (height < -0.004 || height > 0.073)
                continue;

            // 위로 갈수록 응결 구름의 중심이 밀리고 증발원보다 넓어진다.
            vec3 flow = vec3(p.xy * 110.0, p.z * 83.0 - 5.3 * time);
            float roll = noise3(flow * 0.65);
            vec2 drift = vec2(0.37, -0.14) + (roll - 0.5) * vec2(0.36, 0.55);
            vec2 root = p.xy - max(height, 0.0) * drift;
            float z = p.z - bed_height(root);
            float spread = 0.27 * max(z, 0.0);
            float rate = 0.58 * steam_source(root) +
                         0.21 * (steam_source(root + vec2(0.4, 1.0) * spread) +
                                 steam_source(root - vec2(0.4, 1.0) * spread));
            float strength = 1.0 - exp(-3.3 * rate);
            if (strength < 0.007)
                continue;

            float billow = 0.70 * noise3(flow) + 0.23 * noise3(flow * 2.02 + 1.2) +
                           0.07 * noise3(flow * 4.19 - 4.0);
            float condensation = smoothstep(0.0035, 0.012, z);
            float dilution = 1.0 - smoothstep(0.019, 0.062, z);
            float density = strength * condensation * dilution * smoothstep(0.24, 0.73, billow);
            float alpha = 1.0 - exp(-58.0 * density * dt);
            vec3 mist = mix(vec3(0.72, 0.75, 0.78), vec3(0.94, 0.96, 0.98), smoothstep(0.25, 0.78, billow));
            float weight = (1.0 - cloud.a) * alpha;
            cloud += vec4(weight * mist, weight);
        }
        return cloud;
    }

    vec4 integrate_flame(vec3 eye, vec3 direction, float first, float last)
    {
        vec4 accumulated = vec4(0.0);
        float step_size = max(STEP_METERS, (last - first) / float(MAX_STEPS));
        float t = Push.CameraTime.w;
        const float max_height = 0.052;
        // Stable ray jitter hides sampling planes without ticking a screen-space pattern each frame.
        float jitter = 0.15 + 0.70 * hash_noise(vec3(gl_FragCoord.xy, 17.0));
        for (int index = 0; index < MAX_STEPS; ++index)
        {
            float distance = first + (float(index) + jitter) * step_size;
            if (distance > last || accumulated.a > 0.98)
                break;
            vec3 position = eye + direction * distance;
            float height = position.z - bed_height(position.xy);
            if (height < -0.0006)
                break;
            if (height > max_height)
                continue;

            // Rising hot gas sweeps the flame away from its surface footprint.
            float sway = sin(t * 6.0 + height * 170.0 + position.y * 98.0);
            vec2 root = position.xy - height * vec2(0.22 + 0.34 * sway, 0.085);
            root += vec2(-0.0015, 0.0018) * sin(t * 4.0 + position.y * 91.0);
            float drive = surface_combustion(root).x;
            if (drive < 0.022)
                continue;
            float power = smoothstep(0.022, 0.39, drive);
            float z = max(height, 0.0);
            vec3 flow = vec3(root * 174.0, z * 85.0 - t * 5.4);
            float twist = noise3(flow * 0.58 + vec3(0, 0, 9.0));
            flow.xy += vec2(0.85, -0.63) * (twist - 0.5) * smoothstep(0.0, 0.028, z);
            float coarse = noise3(flow);
            float detail = noise3(flow * 2.07 + vec3(7.4, -2.7, t * 1.3));
            float turbulence = coarse * 0.72 + detail * 0.28;

            // Small openings near the base merge and stretch into less dense tongues higher up.
            float relative_height = z / mix(max_height * 0.33, max_height, power);
            float shape = turbulence + 0.34 * power - 0.69 * relative_height;
            float density = smoothstep(0.34, 0.64, shape) * smoothstep(0.0004, 0.0045, height);
            density *= (0.07 + 0.93 * sqrt(power)) * (1.0 - smoothstep(0.80, 1.11, relative_height));
            float alpha = 1.0 - exp(-440.0 * density * step_size);

            float core = smoothstep(0.54, 0.91, shape) * (1.0 - smoothstep(0.18, 0.70, relative_height));
            vec3 orange = mix(vec3(1.0, 0.038, 0.002), vec3(1.0, 0.27, 0.014), density);
            vec3 color = mix(orange, vec3(1.0, 0.91, 0.57), core);
            float weight = (1.0 - accumulated.a) * alpha;
            accumulated.rgb += weight * color;
            accumulated.a += weight;
        }
        return accumulated;
    }

// #endregion

// ============================================================
// 02. Ray Bounds and Compositing
// ============================================================
// #region 02_Ray_Bounds_and_Compositing
    // 프록시 박스의 첫 교차에서만 적분하고 투명 합성용 색과 알파를 출력한다.

    void main()
    {
        float s = Push.Pose.x;
        float c = Push.Pose.y;
        vec3 camera = Push.CameraTime.xyz;
        vec3 eye = vec3(c * camera.x - s * camera.z, camera.y, s * camera.x + c * camera.z);
        vec3 direction = normalize(Local - eye);
        // Clamp components away from zero before the slab intersection.
        vec3 inverse_direction = mix(vec3(-1.0), vec3(1.0), greaterThanEqual(direction, vec3(0.0))) /
                                 max(abs(direction), vec3(1e-6));
        vec3 a = (Field.BoundsMin.xyz - eye) * inverse_direction;
        vec3 b = (Field.BoundsMax.xyz - eye) * inverse_direction;
        vec3 near_plane = min(a, b);
        vec3 far_plane = max(a, b);
        float entry = max(max(near_plane.x, near_plane.y), max(near_plane.z, 0.0));
        float exit = min(min(far_plane.x, far_plane.y), far_plane.z);
        // The proxy is rasterized with both faces. Only the front hit contributes to the image.
        if (exit <= entry || length(Local - eye) > entry + 0.0003)
            discard;

        vec4 effect = Push.Layer == STEAM_VOLUME_LAYER ? integrate_steam(eye, direction, entry, exit) :
                                                         integrate_flame(eye, direction, entry, exit);
        if (effect.a < 0.005)
            discard;
        Color = vec4(effect.rgb / effect.a, effect.a);
    }

// #endregion
