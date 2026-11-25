// 바닥과 표면층의 표시 패스에서 공유하는 프래그먼트 진입점과 입력을 정의한다.

#extension GL_GOOGLE_include_directive : require

#include "SurfaceDrawParameters.glsl"
#include "SurfaceDisplayField.glsl"
#include "LiquidSurfaceReconstruction.glsl"
#include "LavaAppearance.glsl"

// ============================================================
// 00. Fragment Interface
// ============================================================
// #region 00_Fragment_Interface
    // Surface.vert의 월드 및 로컬 좌표를 받아 선형 색과 합성용 커버리지를 출력한다.

    layout(location = 0) in vec3 World;
    layout(location = 1) in vec3 Normal;
    layout(location = 2) in vec3 Local;
    layout(location = 3) in float Depth;
    layout(location = 4) flat in vec4 DebugValue;
    layout(location = 5) flat in vec3 Activity;
    layout(location = 6) in float OverflowWetness;
    layout(location = 7) flat in uint CellId;
    layout(location = 8) in float ShoreAppearanceDepth;
    layout(location = 0) out vec4 Color;

    #ifdef WATER_REFRACTION
    // SurfaceSimulationDisplay.cpp가 먼저 그린 불투명 색과 깊이다. 굴절 패스의 동일한 뷰포트에서 읽는다.
    layout(set = 0, binding = 1) uniform sampler2D OpaqueColor;
    layout(set = 0, binding = 2) uniform sampler2D OpaqueDepth;
    #endif
    // 등록한 표면 텍스처의 RGB는 알베도, 알파는 거칠기에 사용한다.
    layout(set = 0, binding = 3) uniform sampler2D SurfaceTexture;
    layout(set = 0, binding = 5) uniform sampler2D SurfaceNormalTexture;
    layout(set = 0, binding = 6) uniform sampler2D EnvironmentTexture;
    layout(set = 0, binding = 7) uniform sampler2D SnowColorTexture;
    layout(set = 0, binding = 8) uniform sampler2D SnowNormalTexture;

// #endregion

// ============================================================
// 01. Shading Helpers
// ============================================================
// #region 01_Shading_Helpers
    // 상태값을 표시 색으로 변환하고 로컬 좌표를 사용해 표면 텍스처를 안정적으로 샘플링한다.

    float molten_depth_at(vec2 point)
    {
        vec2 liquid = display_region(int(region_at(point)));
        float bed = bed_height(point) * Push.Pose.y - point.x * Push.Pose.x;
        return liquid.x > 0.0 ? max(0.0, liquid.y - bed) : 0.0;
    }

    vec3 heatmap(float value)
    {
        float t = clamp(value, 0.0, 1.0);
        vec3 lo, mid, hi;
        if (Push.Palette == 1u)
        {
            lo = vec3(0.005, 0.001, 0.015);
            mid = vec3(0.53, 0.07, 0.20);
            hi = vec3(0.99, 0.94, 0.52);
        }
        else if (Push.Palette == 2u)
        {
            lo = vec3(0.018, 0.09, 0.64);
            mid = vec3(0.91);
            hi = vec3(0.74, 0.025, 0.045);
        }
        else
        {
            lo = vec3(0.055, 0.006, 0.13);
            mid = vec3(0.020, 0.35, 0.29);
            hi = vec3(0.99, 0.89, 0.05);
        }
        return t < 0.5 ? mix(lo, mid, t * 2.0) : mix(mid, hi, (t - 0.5) * 2.0);
    }

    vec3 local_normal(vec3 normal)
    {
        float s = Push.Pose.x;
        float c = Push.Pose.y;
        return vec3(c * normal.x - s * normal.z, normal.y, s * normal.x + c * normal.z);
    }

    #include "SurfacePbr.glsl"
    #include "SurfaceDiagnostics.glsl"

    // SurfaceSimulationDisplay.cpp의 표시 플래그와 일치한다.
    const uint COMBUSTION_FIELD = 1u << 0u;
    const uint FIRE_LIGHTING = 1u << 1u;
    const uint ICE_LAYER_FIELD = 1u << 2u;
    const uint HEATER_LIGHTING = 1u << 3u;
    const uint NO_MATERIAL_NORMALS = 1u << 16u;

    void material_axes(vec3 local_axis, out vec3 axis_u, out vec3 axis_v)
    {
        vec3 axis = abs(local_axis);
        axis_u = vec3(1.0, 0.0, 0.0);
        axis_v = vec3(0.0, 1.0, 0.0);
        if (axis.x > axis.z && axis.x >= axis.y)
        {
            axis_u = vec3(0.0, 1.0, 0.0);
            axis_v = vec3(0.0, 0.0, 1.0);
        }
        else if (axis.y > axis.z)
            axis_v = vec3(0.0, 0.0, 1.0);
    }

    vec3 apply_material_normal(vec3 local_axis, vec3 axis_u, vec3 axis_v, vec3 detail, float strength)
    {
        // 바닥의 투영 UV가 뒤집힌 면에서도 텍스처의 V 방향과 접선 기준을 맞춘다.
        vec3 tangent = normalize(axis_u - dot(axis_u, local_axis) * local_axis);
        vec3 bitangent = cross(local_axis, tangent);
        bitangent *= dot(bitangent, axis_v) < 0.0 ? -1.0 : 1.0;
        vec3 n = normalize((tangent * detail.x + bitangent * detail.y) * strength +
                           local_axis * max(detail.z, 0.05));
        float s = Push.Pose.x;
        float c = Push.Pose.y;
        return vec3(c * n.x + s * n.z, n.y, -s * n.x + c * n.z);
    }

    vec3 shade_ice_coating(vec3 substrate, vec3 normal, vec3 view, vec3 light, float ice_kg_m2, float wet, bool thick_layer)
    {
        // 얼음 밀도로 환산한 평균 두께를 굴절 오프셋에 사용한다.
        float thickness = ice_kg_m2 / Field.IceProperties.y;
        vec3 ray = local_normal(refract(-view, normal, 1.0 / Field.IceProperties.x));
        float path = thickness / max(abs(ray.z), 0.35);
        if (Push.MaterialScale.a > 0.0)
        {
            vec2 sample_point = Local.xy + ray.xy * path;
            substrate = texture(SurfaceTexture, sample_point * Push.MaterialScale.a + vec2(0.5)).rgb * (1.0 - 0.40 * wet);
        }

        // 굴절된 지점의 바닥을 투과색으로 쓰고 얇은 경계의 산란을 표시한다.
        float diffuse = 0.37 + 0.83 * max(dot(normal, light), 0.0);
        vec3 bulk = ice_bulk_color(substrate * diffuse, path);
        vec2 pattern = Local.xy * vec2(181.0, 165.0);
        float grain = ice_grain(vec3(pattern, 0.47));
        float fractures = ice_cracks(pattern * 0.78 + vec2(0.24 * grain));
        float frost = smoothstep(0.54, 0.83, grain);
        float rim = smoothstep(0.010, 0.075, ice_kg_m2) * (1.0 - smoothstep(0.08, 0.23, ice_kg_m2));
        bulk = mix(bulk, vec3(0.40, 0.55, 0.61), 0.055 * frost + 0.12 * fractures + 0.19 * rim);
        if (thick_layer)
        {
            // 두꺼운 마개에서 내부 산란과 균열의 대비를 높여 재질과 윤곽을 읽을 수 있게 한다.
            float volume = smoothstep(0.001, 0.009, thickness);
            vec3 internal = ice_bulk_color(vec3(0.10, 0.23, 0.32), path);
            bulk = mix(bulk, internal, 0.50 * volume);
            bulk += volume * (0.10 * frost + 0.13 * fractures) * vec3(0.61, 0.83, 0.93);
        }
        return ice_interface_color(bulk, normal, view, light, mix(Field.IceOptics.x, Field.IceOptics.y, frost));
    }

    vec3 shade_hot_substrate(vec3 reflected, float temperature_k)
    {
        // 선형 RGB에서 적열부터 노란 백열까지 근사하고 건조한 금속의 반사 위에 발광을 더한다.
        float red = smoothstep(650.0, 1040.0, temperature_k);
        float orange = smoothstep(985.0, 1260.0, temperature_k);
        float white_hot = smoothstep(1220.0, 1460.0, temperature_k);
        vec3 emission = red * vec3(0.52, 0.008, 0.002) +
                        orange * vec3(0.62, 0.23, 0.012) +
                        white_hot * vec3(0.44, 0.57, 0.41);
        return reflected * (0.64 - 0.47 * red) + emission;
    }

    vec3 shade_snow_layer(vec3 light)
    {
        vec2 point = Local.xy;
        float height = snow_layer_height(point);
        // 깊이는 삼각형 높이를 따르고, 음영에는 연속 높이의 기울기를 써 격자 대각선이 드러나지 않게 한다.
        vec3 surface = World + vec3(Push.Pose.x, 0.0, Push.Pose.y) * (height - Depth);
        vec3 normal = normalize(cross(dFdx(surface), dFdy(surface)));
        if (dot(normal, Normal) < 0.0)
            normal = -normal;
        vec2 dx = dFdx(point);
        vec2 dy = dFdy(point);
        float footprint = max(length(dx), length(dy));
        float detail = 1.0 - smoothstep(0.00030, 0.00115, footprint);
        if (height <= 0.000025 || Depth <= 0.00001)
            discard;
        vec3 snow;
        float roughness = 0.86;
        float repeats = Push.MaterialScale.a;
        if (repeats > 0.0)
        {
            // 눈의 맵은 바닥 천과 별도다. 멀리서 픽셀보다 작은 노멀 요철은 줄인다.
            vec2 uv = point * repeats + 0.5;
            vec4 material = textureGrad(SnowColorTexture, uv, dx * repeats, dy * repeats);
            snow = material.rgb;
            roughness = material.a;
            if ((Push.AppearanceFlags & NO_MATERIAL_NORMALS) == 0u)
            {
                vec3 grains = 2.0 * textureGrad(SnowNormalTexture, uv, dx * repeats, dy * repeats).rgb - 1.0;
                normal = apply_material_normal(local_normal(normal), vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0),
                                               grains, 1.35 * detail);
            }
        }
        else
        {
            float clump = snow_grain(point * 540.0 + 15.0);
            float grain = snow_grain(point * 2150.0 + 21.0);
            snow = vec3(0.87, 0.91, 0.96) * (0.81 + 0.19 * clump + 0.19 * detail * (grain - 0.5));
        }
        vec3 view = normalize(Push.CameraTime.xyz - World);
        return shade_dielectric(snow, normal, view, light, roughness, dielectric_f0(Field.IceProperties.x));
    }

// #endregion

// ============================================================
// 02. Section Window
// ============================================================
// #region 02_Section_Window
    // 눈금판은 자기 앞의 한 줄을 읽는다. 투명 부분, 바닥, 각 층의 경계가 같은 기준 높이를 공유한다.

    float section_tick(float height, float along_edge, float span, float pixel_z)
    {
        float tick = round(height / Field.SectionWindow.z);
        float bar = (mod(tick, 5.0) < 0.5 ? 0.09 : 0.05) * span;
        return (1.0 - smoothstep(0.15, 0.95, abs(height - tick * Field.SectionWindow.z) / pixel_z)) *
               (1.0 - smoothstep(bar * 0.70, bar, along_edge));
    }

    vec4 shade_glass_vessel()
    {
        vec3 axis = local_normal(Normal);
        vec3 view = normalize(Push.CameraTime.xyz - World);
        float height = Local.z - Field.BedMetrics.x;
        bool wall = abs(axis.z) < 0.5;
        bool back = axis.z < -0.5 || (wall && dot(view, Normal) < 0.0);
        if (back != (Push.Layer == SECTION_WINDOW_BACK_LAYER))
            discard;

        float thickness = Field.SectionWindow.w;
        vec2 inner_span = Field.VesselBounds.zw - Field.VesselBounds.xy;
        vec2 wall_edge = min(Local.xy - Field.VesselBounds.xy, Field.VesselBounds.zw - Local.xy) + thickness;
        float along_edge = abs(axis.y) > 0.5 ? wall_edge.x : wall_edge.y;
        float fresnel = pow(1.0 - clamp(abs(dot(Normal, view)), 0.0, 1.0), 3.0);
        vec3 glass = mix(vec3(0.48, 0.67, 0.72), vec3(0.89, 0.96, 0.98), fresnel);
        vec4 color = vec4(glass, 0.07 + 0.34 * fresnel);
        if (!wall)
            return vec4(glass, axis.z > 0.0 ? 0.66 : 0.30);

        float z_pixel = max(fwidth(Local.z), 0.00005);
        float edge_distance = min(along_edge, min(Field.SectionWindow.y - height, height + thickness));
        float rim = 1.0 - smoothstep(0.00018, 0.00075, edge_distance);
        float tick = axis.y < -0.5 && height >= 0.0
                   ? section_tick(height, along_edge, inner_span.x + 2.0 * thickness, z_pixel) : 0.0;
        return mix(color, vec4(0.86, 0.96, 0.98, back ? 0.57 : 0.92), max(rim, tick));
    }

    vec4 shade_vessel_contents()
    {
        vec3 axis = local_normal(Normal);
        if (abs(axis.z) >= 0.5 || dot(Push.CameraTime.xyz - World, Normal) < 0.0)
            discard;

        int index = chart_sample_at(vessel_sample_point(Local.xy));
        int region = int(field_sample_regions(index));
        vec4 film = display_film(region);
        float ground = field_base_height(index);
        float solid_top = height_above_film_floor(ground, Local.x, film.w);
        if (Local.z < ground)
            discard;
        if (Local.z < solid_top)
            return vec4(film.rgb * (0.80 + 0.10 * abs(axis.y)), 1.0);

        vec2 liquid = display_region(region);
        if (liquid.x <= 0.0 || World.z > liquid.y)
            discard;

        vec3 fluid = Field.LiquidTint.a > 0.0 ? Field.LiquidTint.rgb : vec3(0.13, 0.64, 0.83);
        float z_pixel = max(fwidth(Local.z), 0.00005);
        float meniscus = 1.0 - smoothstep(0.15, 1.5, (liquid.y - World.z) / z_pixel);
        return vec4(mix(fluid * (0.64 + 0.10 * abs(axis.y)), vec3(1.0, 0.92, 0.76), 0.68 * meniscus), 0.91);
    }

    vec4 shade_section_window()
    {
        int sample_index = chart_sample_at(vec2(Local.x, Field.SectionWindow.x + 0.00001));
        int region = int(field_sample_regions(sample_index));
        float ground = field_base_height(sample_index);
        float solid_top = field_heights(sample_index);
        vec2 liquid = display_region(region);
        bool is_liquid = liquid.x > 0.0 && Local.z >= solid_top && World.z <= liquid.y;
        vec3 fluid = Field.LiquidTint.a > 0.0 ? Field.LiquidTint.rgb : vec3(0.13, 0.64, 0.83);
        float height = Local.z - Field.BedMetrics.x;
        float pane_edge = min(Local.x + 0.5 * Field.SampleLayout.x, 0.5 * Field.SampleLayout.x - Local.x);
        float fresnel = pow(1.0 - abs(dot(normalize(Push.CameraTime.xyz - World), Normal)), 4.0);
        float z_pixel = max(fwidth(Local.z), 0.00005);
        vec4 color = vec4(vec3(0.49, 0.72, 0.80), 0.10 + 0.22 * fresnel);
        if (Local.z < ground)
        {
            if (height >= 0.0 && Local.x >= Field.SectionSolidBed.x && Local.x < Field.SectionSolidBed.y)
            {
                // 바닥 재질과 같은 맵을 측면에 투영해 원래 바닥 단차의 위치를 보여준다.
                float repeats = max(Push.MaterialScale.a, 40.0);
                vec3 stone = Push.MaterialScale.a > 0.0
                           ? texture(SurfaceTexture, vec2(Local.x, Local.z) * repeats + 0.5).rgb
                           : Push.MaterialScale.rgb;
                color = vec4(mix(vec3(0.15, 0.17, 0.19), stone, 0.72) * 0.78, 1.0);
            }
            else
            {
                // 단면을 투과시켜도 실제 바닥 높이는 얇은 경계선으로 읽을 수 있다.
                float ground_edge = 1.0 - smoothstep(0.45, 1.8, (ground - Local.z) / z_pixel);
                vec3 glass = mix(vec3(0.38, 0.70, 0.79), vec3(0.73, 0.89, 0.94), ground_edge);
                color = vec4(glass, 0.20 + 0.18 * fresnel + 0.62 * ground_edge);
            }
        }
        else if (Local.z < solid_top)
            color = vec4(mix(fluid, vec3(1.0, 0.90, 0.71), 0.56), 0.92);
        else if (is_liquid)
            color = vec4(fluid, 0.73);

        float fluid_edge = is_liquid ? abs(liquid.y - World.z) : abs(solid_top - Local.z);
        if ((is_liquid || solid_top > ground) && Local.z >= ground)
            color = mix(color, vec4(mix(fluid, vec3(1.0), 0.55), 1.0),
                        0.9 * (1.0 - smoothstep(0.25, 1.5, fluid_edge / z_pixel)));

        float tick_alpha = section_tick(height, pane_edge, Field.SampleLayout.x, z_pixel);
        float rim = min(pane_edge, min(Field.SectionWindow.y - height, Local.z - Field.BedMetrics.z));
        float frame_alpha = 1.0 - smoothstep(0.00035, 0.0013, rim);
        color = mix(color, vec4(0.88, 0.97, 1.0, 0.97), max(frame_alpha, height >= 0.0 ? tick_alpha : 0.0));
        return color;
    }

// #endregion

// ============================================================
// 03. Fragment Shading
// ============================================================
// #region 03_Fragment_Shading
    // 레이어 및 진단 모드에 따라 바닥, 물, 얼음을 그리고 굴절 패스에서는 불투명 이미지를 참조한다.

    void main()
    {
        if (Push.Layer == VESSEL_CONTENTS_LAYER)
        {
            Color = shade_vessel_contents();
            return;
        }
        if (Push.Layer == SECTION_WINDOW_LAYER || Push.Layer == SECTION_WINDOW_BACK_LAYER)
        {
            Color = Field.SectionWindow.w > 0.0 ? shade_glass_vessel() : shade_section_window();
            return;
        }
        if (Field.SectionWindow.y > 0.0 && (Push.Mode < 3u || Push.Mode == PBR_SURFACE_MODE))
        {
            if (Field.SectionWindow.w > 0.0)
            {
                // 불투명 바닥은 관측용 용기가 대신 표시한다. 실제 고체층은 기존 재질 경로를 그대로 쓴다.
                if (any(lessThan(Local.xy, Field.VesselBounds.xy)) ||
                    any(greaterThan(Local.xy, Field.VesselBounds.zw)))
                    discard;
                if (Push.Layer == 0u && (local_normal(Normal).z < 0.22 ||
                    field_heights(chart_sample_at(vessel_sample_point(Local.xy))) -
                    field_base_height(chart_sample_at(vessel_sample_point(Local.xy))) <= 0.00001))
                    discard;
            }
            else if (Local.y < Field.SectionWindow.x)
                discard;
        }

        vec3 light = normalize(vec3(-0.35, -0.6, 1.0));
        if (Push.Layer == SNOW_LAYER)
        {
            Color = vec4(shade_snow_layer(light), 1.0);
            return;
        }
        if (Push.Layer == LAVA_DEPOSIT_LAYER)
        {
            float relief = lava_deposit_height(Local.xy);
            // 정점 표본 사이의 연속 높이를 조명 법선에 반영한다.
            vec3 surface = World + vec3(Push.Pose.x, 0.0, Push.Pose.y) * (relief - Depth);
            vec3 normal = normalize(cross(dFdx(surface), dFdy(surface)));
            if (dot(normal, Normal) < 0.0)
                normal = -normal;
            if (min(relief, Depth) < 0.00010)
                discard;
            vec3 view = normalize(Push.CameraTime.xyz - World);
            float heat = smoothstep(345.0, 620.0, sample_display_state(Local.xy).z);
            Color = vec4(shade_lava(Local.xy, normal, view, light, heat), 1.0);
            return;
        }
        bool overflow = Push.Layer == OVERFLOW_LAYER;
        // 저장 수위로 구한 깊이가 물가와 광로를 정한다. 스무딩은 물의 표시 부피를 바꾸지 않는다.
        float depth = Depth;
        bool phase_surface = !overflow && field_cell_region_counts(int(CellId)) == 1 && Field.PhaseLevels.y > Field.PhaseLevels.x;
        if (Push.Layer == 3u)
        {
            Color = vec4(ice_backdrop(Local.xy, 0.0), 1.0);
            return;
        }
        if (Push.Layer == 0u)
        {
            vec3 normal = normalize(Normal);
            vec3 surface_axis = local_normal(normal);
            bool side = abs(surface_axis.z) < 0.22;
            bool outer_wall = side || surface_axis.z < 0.0;
            float shading = 0.39 + 0.61 * max(dot(normal, light), 0.0);
            if (Push.Mode >= SIMULATION_GRID_VIEW || Push.Mode == LAYER_STATES_VIEW ||
                Push.Mode == SUBSTRATE_STATES_VIEW || Push.Mode == LAYER_THICKNESS_VIEW)
            {
                Color = vec4(surface_diagnostic(Local, CellId, surface_axis.z < 1e-5, shading, depth), 1.0);
                return;
            }
            if (Push.Mode == ELEVATION_VIEW)
            {
                float height = Local.z - Field.BedMetrics.x;
                float normalized_height = height / max(0.001, Field.BedMetrics.y - Field.BedMetrics.x);
                vec3 color = mix(vec3(0.024, 0.13, 0.23), vec3(0.89, 0.60, 0.22), clamp(normalized_height, 0.0, 1.0));
                float fine = contour(height, 0.005), major = contour(height, 0.020);
                color = mix(color * shading, vec3(0.012, 0.022, 0.033), fine * 0.62);
                Color = vec4(mix(color, vec3(0.95, 0.88, 0.68), major * 0.81), 1.0);
                return;
            }
            if (Push.Mode >= 3u && Push.Mode != PBR_VIEW)
            {
                float value = 0.0;
                if (Push.Mode == 5u)
                    value = temperature_heatmap_value(Local.xy, CellId, DebugValue.w);
                else if (Push.Mode == 7u)
                    value = 0.45 + Local.z * 90.0;
                else if (Push.Mode == 8u)
                    value = max(Activity.x * 22000.0, Activity.y * 4.0);
                vec3 debug_color = side ? vec3(0.065, 0.055, 0.075) : heatmap(value);
                debug_color *= shading;
                if (surface_axis.z > 1e-5 && (Push.AppearanceFlags & CELL_OUTLINE_OVERLAY) != 0u)
                    debug_color = heatmap_cell_grid(debug_color, solver_cell_uv(Local.xy, CellId));
                Color = vec4(debug_color, 1.0);
                return;
            }

            int specimen = cell_specimen(CellId);
            bool specimen_materials = (Push.AppearanceFlags & SPECIMEN_MATERIALS) != 0u;
            vec4 material = vec4(Push.MaterialScale.rgb, 0.8);
            if (Push.MaterialScale.a > 0.0)
            {
                vec3 axis_u, axis_v;
                material_axes(surface_axis, axis_u, axis_v);
                vec2 projection = vec2(dot(Local, axis_u), dot(Local, axis_v));
                vec2 material_uv = projection * Push.MaterialScale.a + vec2(0.5);
                if (specimen_materials)
                {
                    float count = float(Field.SpecimenCount);
                    vec2 chart = Field.SampleLayout.xy / vec2(count, 1.0);
                    vec3 sample_local = Local;
                    sample_local.x -= (float(specimen) + 0.5 - 0.5 * count) * chart.x;
                    vec2 size = vec2(dot(chart, axis_u.xy), dot(chart, axis_v.xy));
                    material_uv = vec2(dot(sample_local, axis_u), dot(sample_local, axis_v)) /
                                  mix(vec2(chart.x), size, greaterThan(size, vec2(0.0))) + 0.5;
                    if (!side)
                        material_uv = sample_local.xy / chart + 0.5;
                    material_uv.y = 1.0 - material_uv.y;
                    // 아틀라스의 마지막 픽셀까지만 읽어서 옆 시편의 맵이 섞이지 않게 한다.
                    vec2 inset = 0.5 * vec2(count, 1.0) / vec2(textureSize(SurfaceTexture, 0));
                    material_uv = clamp(material_uv, inset, 1.0 - inset);
                    material_uv.x = (material_uv.x + float(specimen)) / count;
                }
                else if ((Push.AppearanceFlags & CHART_ALBEDO) != 0u)
                {
                    vec2 chart = Field.SampleLayout.xy;
                    vec2 size = vec2(dot(chart, axis_u.xy), dot(chart, axis_v.xy));
                    // 측면의 세로 좌표는 상면에서 한 장에 해당하는 길이를 그대로 쓴다.
                    material_uv = projection / mix(vec2(chart.x), size, greaterThan(size, vec2(0.0))) + vec2(0.5);
                    if (!side)
                        material_uv = Local.xy / chart + vec2(0.5);
                    material_uv.y = 1.0 - material_uv.y;
                }
                // 직사각 타일의 직조 비율을 보존하고 재질 높이로 미세한 시차를 표현한다.
                if (!specimen_materials && (Push.AppearanceFlags & CHART_ALBEDO) == 0u)
                {
                    ivec2 pixels = textureSize(SurfaceTexture, 0);
                    vec2 repeat_scale = vec2(1.0, float(pixels.x) / float(pixels.y));
                    material_uv = projection * Push.MaterialScale.a * repeat_scale + 0.5;
                    if (Push.Mode == PBR_VIEW && Field.SubstrateReliefMeters > 0.0)
                    {
                        vec3 eye = local_normal(normalize(Push.CameraTime.xyz - World));
                        float depth = 1.0 - texture(SurfaceNormalTexture, material_uv).a;
                        vec3 tangent_eye = eye - surface_axis * dot(eye, surface_axis);
                        vec2 shift = vec2(dot(tangent_eye, axis_u), dot(tangent_eye, axis_v));
                        material_uv -= shift / max(dot(eye, surface_axis), 0.25) * depth *
                                       Field.SubstrateReliefMeters * Push.MaterialScale.a * repeat_scale;
                    }
                }
                material = texture(SurfaceTexture, material_uv);
                if ((Push.AppearanceFlags & CHART_ALBEDO) != 0u)
                    material.a = 0.8;
                if (Push.Mode == PBR_VIEW && Field.BedMetrics.w == 0.0 && Field.SubstrateOptics.w > 0.0 &&
                    (Push.AppearanceFlags & NO_MATERIAL_NORMALS) == 0u)
                    normal = apply_material_normal(surface_axis, axis_u, axis_v,
                                                   2.0 * texture(SurfaceNormalTexture, material_uv).rgb - 1.0,
                                                   Field.SubstrateOptics.w);
            }
            float film_ice_kg_m2 = 0.0;
            float coating_wetness = 0.0;
            if (!outer_wall && Field.FilmDisplayWord >= 0)
            {
                int region = int(region_at(Local.xy));
                vec4 coating = display_film(region);
                float ground = field_base_height(chart_sample_at(Local.xy));
                float thickness = height_above_film_floor(ground, Local.x, coating.w) - ground;
                if (Field.FilmIceLayer >= 0)
                    film_ice_kg_m2 = display_film_mass(Field.FilmIceLayer, region) * field_region_inverse_areas(region);
                if (film_ice_kg_m2 <= 0.0 || thickness <= 0.0)
                {
                    float cover = smoothstep(0.0, 0.0002, thickness);
                    // 혼합수에 의한 젖음은 위에 자유수가 사라진 뒤에도 남는다.
                    coating_wetness = display_coating_wetness(region) * cover;
                    material.rgb = mix(material.rgb, coating.rgb, cover);
                    normal = normalize(mix(normal, normalize(Normal), cover));
                    film_ice_kg_m2 = 0.0;
                }
            }
            bool steel = (Push.AppearanceFlags & METAL_SUBSTRATE) != 0u ||
                         (Field.MetallicSpecimens & (1u << uint(specimen))) != 0u;
            float dry_roughness = clamp(Field.SubstrateOptics.x + material.a - 0.8, 0.0, 1.0);
            float wet_depth = !outer_wall && depth > 0.0 ? max(0.0, ShoreAppearanceDepth) : 0.0;
            float wet = outer_wall ? 0.0 : clamp(DebugValue.x * 1.55 + wet_depth * 110.0, 0.0, 1.0);
            bool has_absorbed_wetness = !steel && Field.IceProperties.w > 0.0;
            if (has_absorbed_wetness)
            {
                // 각 위치의 흡수수는 깊이 방향으로 한 값만 저장하므로 몸체의 면들이 이를 공유한다.
                float absorbed = sample_coating_masses(Local.xy).x;
                if (Field.AbsorbedWetnessDepth.y > 0.0)
                {
                    float capacity = field_absorb_capacity(int(CellId));
                    float saturation = capacity > 0.0 ? clamp(absorbed / capacity, 0.0, 1.0) : 0.0;
                    float contrast = min(Field.AbsorbedWetnessDepth.x, min(saturation, 1.0 - saturation));
                    float elapsed = max(0.0, Push.CameraTime.w - Field.AbsorbedWetnessDepth.z);
                    contrast *= exp(-elapsed * Field.AbsorbedWetnessDepth.y);
                    float bottom = Field.BedMetrics.z;
                    float column_height = max(bed_height(Local.xy) - bottom, 1e-6);
                    float depth_fraction = outer_wall ? 1.0 - clamp((Local.z - bottom) / column_height, 0.0, 1.0) : 0.0;
                    wet = saturation + (1.0 - 2.0 * depth_fraction) * contrast;
                }
                else
                {
                    float absorbed_wet = pow(clamp(absorbed / Field.IceProperties.w, 0.0, 1.0), 0.70);
                    wet = max(absorbed_wet, smoothstep(0.0, 0.0006, wet_depth));
                }
            }
            if (!outer_wall)
                wet = max(max(wet, OverflowWetness), coating_wetness);
            vec3 base = material.rgb * (1.0 - wet * (has_absorbed_wetness ? 0.76 : 0.40));
            vec3 view = normalize(Push.CameraTime.xyz - World);
            if (Push.Mode == PBR_VIEW && Field.BedMetrics.w > 0.0)
            {
                vec3 ice = shade_ice(Local, normal, surface_axis, view, light, wet);
                if (!outer_wall && (Push.AppearanceFlags & LAVA_DEPOSIT) != 0u)
                {
                    // 돌 가장자리가 가리는 가까운 얼음 표면의 빛만 줄인다.
                    float nearby = lava_deposit_height(Local.xy + local_normal(light).xy * 0.004);
                    ice *= 1.0 - 0.23 * smoothstep(0.0003, 0.0030, nearby);
                }
                Color = vec4(ice, 1.0);
                return;
            }
            if (outer_wall)
            {
                float roughness = mix(dry_roughness, Field.SubstrateOptics.y, wet);
                vec3 color = base * shading * 0.82;
                if (Push.Mode == PBR_VIEW)
                    color = steel ? shade_steel(base, normal, view, light, material.a) :
                                   shade_dielectric(base, normal, view, light, roughness, Field.SubstrateOptics.z);
                if (phase_surface)
                {
                    float lower = 1.0 - smoothstep(0.000048, 0.00017, abs(Local.z - Field.PhaseLevels.x));
                    float upper = 1.0 - smoothstep(0.000048, 0.00017, abs(Local.z - Field.PhaseLevels.y));
                    color = mix(color, vec3(0.065, 0.49, 0.70), lower * 0.85);
                    color = mix(color, vec3(0.70, 0.83, 0.88), upper * 0.91);
                }
                Color = vec4(color, 1.0);
                return;
            }

            bool has_ice_layer = (Push.AppearanceFlags & ICE_LAYER_FIELD) != 0u;
            bool has_snow_layer = has_ice_layer && Field.IceProperties.z > 0.0;
            float grain = sin(Local.x * 2900.0) * sin(Local.y * 4200.0 + Local.z * 1250.0);
            float char_detail = mix(0.82 + 0.14 * grain, 0.62 + 1.5 * dot(material.rgb, vec3(0.2126, 0.7152, 0.0722)),
                                    clamp(Push.MaterialScale.a, 0.0, 1.0));
            vec2 combustion = vec2(0.0, DebugValue.z);
            vec4 display_state = vec4(0.0);
            if ((Push.AppearanceFlags & (COMBUSTION_FIELD | HEATER_LIGHTING | INCANDESCENT_SUBSTRATE)) != 0u ||
                (has_ice_layer && !has_snow_layer))
                display_state = sample_display_state(Local.xy);
            if ((Push.AppearanceFlags & COMBUSTION_FIELD) != 0u)
                combustion = display_state.xy;
            float char_amount = steel ? 0.0 :
                clamp(combustion.y + 0.12 * grain * combustion.y * (1.0 - combustion.y), 0.0, 1.0);
            if (phase_surface && Local.z < Field.PhaseLevels.x)
                base *= mix(0.72, 1.22, smoothstep(0.02, 0.12, abs(fract(Local.y / 0.006 + 0.5) - 0.5)));
            // 탄화층의 어두운 반사와 짙은 색을 같은 누적 탄화율로 변화시킨다.
            char_amount += Field.CharContrast * char_amount * (1.0 - char_amount);
            vec3 char_albedo = vec3(0.019, 0.012, 0.009) * char_detail * mix(1.0, 0.24, Field.CharContrast);
            base = mix(base, char_albedo, char_amount);

            float roughness = mix(steel ? material.a : dry_roughness, Field.SubstrateOptics.y, wet);
            roughness = mix(roughness, mix(0.9, 0.99, Field.CharContrast), char_amount);
            float substrate_f0 = Field.SubstrateOptics.z * (1.0 - 0.82 * Field.CharContrast * char_amount);
            vec3 half_vector = normalize(light + normalize(Push.CameraTime.xyz - World));
            float gloss = pow(max(dot(normal, half_vector), 0.0), mix(90.0, 8.0, roughness));
            vec3 firelight = vec3(0.0);
            if ((Push.AppearanceFlags & FIRE_LIGHTING) != 0u)
            {
                float drive = (combustion.x * 2.0 +
                               surface_combustion(Local.xy + vec2(0.010, 0.006)).x +
                               surface_combustion(Local.xy - vec2(0.010, 0.006)).x +
                               surface_combustion(Local.xy + vec2(-0.006, 0.010)).x +
                               surface_combustion(Local.xy + vec2(0.006, -0.010)).x) / 6.0;
                float flutter = 0.86 + 0.14 * sin(Push.CameraTime.w * 17.0 + Local.x * 119.0 - Local.y * 227.0);
                firelight = vec3(0.19, 0.044, 0.006) * flutter * smoothstep(0.018, 0.34, drive) * (0.70 + base);
            }
            vec3 color = base * shading + gloss * (0.023 + 0.06 * wet);
            if (Push.Mode == PBR_VIEW)
            {
                vec3 reflection_normal = normal;
                // 젖은 섬유 사이로 물이 차면 직조의 미세 기울기가 반사를 퍼뜨리는 정도가 줄어든다.
                if (has_absorbed_wetness)
                    reflection_normal = normalize(mix(normal, normalize(Normal), 0.60 * wet));
                color = steel ? shade_steel(material.rgb, normal, view, light, roughness) :
                                shade_dielectric(base, normal, reflection_normal, view, light, roughness, substrate_f0);
            }
            if (Push.Mode == PBR_VIEW && liquid_option_enabled(SMOOTH_LIQUID_BOUNDARY) &&
                (Push.AppearanceFlags & INCANDESCENT_LIQUID) != 0u)
            {
                // 얕은 발광층은 바닥에서 합성해야 벽돌 위의 얇은 막이 불투명 수면으로 보이지 않는다.
                float film_depth = molten_depth_at(Local.xy) > 0.0 ? depth : 0.0;
                if (film_depth > 0.0)
                {
                    float coverage = smoothstep(0.0, Field.LiquidOptics.y, film_depth);
                    color = mix(color, shade_molten_lava(Local.xy * 0.48, film_depth), coverage);
                }
            }
            if ((Push.AppearanceFlags & INCANDESCENT_SUBSTRATE) != 0u)
                color = shade_hot_substrate(color, display_state.z);
            float ice_coverage_mass = film_ice_kg_m2 + (has_ice_layer && !has_snow_layer ? display_state.w : 0.0);
            if (ice_coverage_mass > 0.0)
            {
                float coverage = smoothstep(0.006, 0.16, ice_coverage_mass);
                color = mix(color, shade_ice_coating(base, normal, view, light, ice_coverage_mass, wet, film_ice_kg_m2 > 0.0), coverage);
            }
            if ((Push.AppearanceFlags & HEATER_LIGHTING) != 0u)
            {
                // 외부 열원의 반사광으로 예열 위치를 표시한다.
                float preheat = smoothstep(295.0, 465.0, display_state.z);
                float ignition = smoothstep(475.0, 545.0, display_state.z);
                color += vec3(0.28, 0.067, 0.011) * preheat * (1.0 - ignition) * (0.45 + base) * (1.0 - char_amount);
            }
            if ((Push.AppearanceFlags & CELL_OUTLINE_OVERLAY) != 0u)
            {
                float boundary = grid_line(solver_cell_uv(Local.xy, CellId), 0.55);
                color = mix(color, vec3(0.99, 0.69, 0.24), boundary * 0.85);
            }
            Color = vec4(color + firelight, 1.0);
            return;
        }

        if (depth <= 0.000001)
            discard;
        // 얇은 수막의 색과 반사가 함께 사라지는 깊이 범위를 재질 외관에서 정한다.
        float coverage = liquid_option_enabled(SMOOTH_LIQUID_BOUNDARY) ? smoothstep(0.0, Field.LiquidOptics.y, depth) : 1.0;
        float flow_highlight = 0.0;
        if (overflow)
        {
            // 반사띠가 진행 방향을 따라 움직인다. 흐름이 멈추면 마지막 월류가 서서히 사라진다.
            float wave = 0.5 + 0.5 * cos(6.2831853 * (Local.y - 6.0 * Push.CameraTime.w));
            flow_highlight = smoothstep(0.28, 0.95, wave);
            float sides = 1.0 - smoothstep(0.72, 1.0, abs(Local.x));
            float ends = smoothstep(0.0, 0.11, Local.y) * (1.0 - smoothstep(0.80, 1.0, Local.y));
            coverage = DebugValue.x * sides * ends * mix(0.58, 0.85, flow_highlight);
        }
        if ((Push.AppearanceFlags & INCANDESCENT_LIQUID) != 0u)
        {
            if (Push.Mode == PBR_VIEW && liquid_option_enabled(SMOOTH_LIQUID_BOUNDARY) && !overflow &&
                (depth < Field.LiquidOptics.y || molten_depth_at(Local.xy) <= 0.0))
                discard; // 얕은 막은 불투명 바닥 패스에서 이미 합성했다.
            Color = vec4(shade_molten_lava(Local.xy * 0.48, depth), overflow ? coverage : 1.0);
            return;
        }
        if (Push.Mode == 0u)
        {
            Color = vec4(0.057, 0.34, 0.47, (overflow ? 0.80 : 0.42) * coverage);
            return;
        }
        float ripple = sin(Local.x * 980.0 - Push.CameraTime.w * 1.8) *
                       sin(Local.y * 1090.0 + Push.CameraTime.w * 2.2);
        float solid = 0.0;
        if (phase_surface)
        {
            SurfaceCellState state = display_cell(int(CellId));
            solid = state.substrate.y / max(1e-9, state.layer.x + state.substrate.y);
        }
        vec2 waves = vec2(0.024 * ripple, 0.031 * sin(Local.y * 810.0)) * (1.0 - solid);
        if (Push.Mode == PBR_VIEW)
        {
            // 얕은 물에는 파장이 긴 법선 변화만 남겨 스트립이 여러 줄로 갈라지는 현상을 줄인다.
            float broad_ripple = sin(dot(Local.xy, vec2(260.0, 350.0)) - 1.8 * Push.CameraTime.w);
            waves = vec2(0.003, 0.005) * broad_ripple * (1.0 - solid) * smoothstep(0.0, 0.0015, depth);
        }
        if (Push.Layer == 1u && !phase_surface && liquid_option_enabled(SLOSH_OPTICS) && slosh_amplitude() != 0.0)
        {
            // 넓은 반사 흐름은 기존 잔물결에 합성하고 물가의 아주 얕은 띠에서는 감쇠한다.
            float along = Local.x / Field.SampleLayout.x;
            float swing = slosh_amplitude() * smoothstep(0.0, 0.00045, depth);
            waves += swing * vec2(0.20 * cos(5.0 * along), 0.05 * sin(5.0 * along));
        }
        vec3 normal = normalize(Normal + vec3(waves, 0.0));
        vec3 view = normalize(Push.CameraTime.xyz - World);
        float grazing = 1.0 - clamp(dot(view, normal), 0.0, 1.0);
        float fresnel = 0.02 + 0.98 * pow(grazing, 5.0);
        vec3 reflection = reflect(-view, normal);
        float sheen = pow(max(dot(reflection, light), 0.0), 44.0);
        float tint = clamp(0.25 + 80.0 * depth, 0.26, 0.62);
        vec3 transmission = mix(vec3(0.038, 0.31, 0.49), Field.LiquidTint.rgb, Field.LiquidTint.a);
        #ifdef WATER_REFRACTION
        vec2 extent = vec2(textureSize(OpaqueColor, 0));
        vec2 view_size = vec2(Push.ViewSize & 65535u, Push.ViewSize >> 16u);
        vec2 origin = floor(gl_FragCoord.xy / view_size) * view_size;
        // Clamping keeps the refracted ray within its own comparison viewport.
        vec2 sample_pos = clamp(gl_FragCoord.xy + (normal.xy + 0.14 * view.xy) * min(65.0, depth * 12000.0),
                                origin + vec2(0.5), origin + view_size - vec2(0.5));
        if (texture(OpaqueDepth, sample_pos / extent).r <= gl_FragCoord.z)
            sample_pos = gl_FragCoord.xy;
        vec3 behind = texture(OpaqueColor, sample_pos / extent).rgb;
        vec3 background = liquid_option_enabled(SMOOTH_LIQUID_BOUNDARY) ? texture(OpaqueColor, gl_FragCoord.xy / extent).rgb : behind;
        if (Push.Mode == PBR_VIEW)
        {
            float ior = Field.LiquidOptics.z;
            float roughness = Field.LiquidOptics.w;
            float path = depth / max(0.20, -refract(-view, normal, 1.0 / ior).z);
            float color_path = path * Field.LiquidOptics.x;
            vec3 extinction = mix(vec3(83.0, 24.0, 9.0), vec3(6.0) + 200.0 * (1.0 - Field.LiquidTint.rgb), Field.LiquidTint.a);
            vec3 transmittance = exp(-extinction * color_path);
            vec3 scattering_tint = mix(vec3(0.023, 0.125, 0.21), Field.LiquidTint.rgb * 0.52, Field.LiquidTint.a);
            vec3 water_color = behind * transmittance + scattering_tint * (1.0 - exp(-160.0 * color_path));
            if (Field.BedMetrics.w > 0.0 && (Push.AppearanceFlags & LAVA_DEPOSIT) != 0u)
                water_color = behind * exp(-vec3(16.0, 7.0, 4.0) * path);
            vec3 reflection_color = studio_radiance(reflection, roughness);
            float f0 = dielectric_f0(ior);
            float sparkle = min(0.38, dielectric_specular(normal, view, light, roughness, f0));
            vec3 lit = mix(water_color, reflection_color, schlick_fresnel(dot(view, normal), f0)) + sparkle;
            if (solid > 0.0)
            {
                vec2 sample_at = Local.xy * 179.0;
                float grain = ice_grain(vec3(sample_at, 0.73));
                float fractures = ice_cracks(sample_at * 0.73 + vec2(0.23 * grain));
                float coverage_ice = smoothstep(0.12, 0.84, solid + 0.36 * (grain - 0.5));
                vec3 ice_ray = refract(-view, normal, 1.0 / Field.IceProperties.x);
                float ice_path = depth * solid / max(0.20, -ice_ray.z);
                vec3 bulk = ice_bulk_color(behind, ice_path);
                float frost = smoothstep(0.57, 0.85, grain);
                bulk = mix(bulk, vec3(0.40, 0.55, 0.61), 0.23 * frost + 0.08 * fractures);
                vec3 ice = ice_interface_color(bulk, normal, view, light, mix(Field.IceOptics.x, Field.IceOptics.y, frost));
                lit = mix(lit, ice, coverage_ice);
            }
            if (overflow)
                Color = vec4(mix(lit, reflection_color, 0.14 + 0.25 * flow_highlight), coverage);
            else
                Color = vec4(mix(background, lit, coverage), 1.0);
            return;
        }
        vec3 sky = water_preview_sky(reflection);
        vec3 extinction = mix(vec3(212.0, 58.0, 21.0), vec3(6.0) + 200.0 * (1.0 - Field.LiquidTint.rgb), Field.LiquidTint.a);
        vec3 filtered = behind * exp(-extinction * depth * Field.LiquidOptics.x);
        vec3 lit = mix(filtered + transmission * tint * 0.28, sky, fresnel) + 0.23 * sheen;
        Color = overflow ? vec4(mix(lit, sky, 0.24 * flow_highlight), coverage)
                         : vec4(mix(background, lit, coverage), 1.0);
        #else
        vec3 sky = water_preview_sky(reflection);
        Color = vec4(mix(transmission, sky, fresnel) + 0.27 * sheen, mix(tint, 0.85, fresnel) * coverage);
        #endif
    }

// #endregion
