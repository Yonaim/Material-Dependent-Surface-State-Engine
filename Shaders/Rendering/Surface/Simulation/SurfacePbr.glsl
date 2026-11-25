// Surface Simulation의 유전체, 얼음과 등고선 표시에서 공유하는 광학 함수를 제공한다.

// ============================================================
// 00. Dielectric Lighting
// ============================================================
// #region 00_Dielectric_Lighting
    // 유전체 반사율과 거칠기에 따른 직접광 및 환경광을 평가한다.

    const float PI = 3.14159265;
    const uint PBR_VIEW = PBR_SURFACE_MODE;
    const uint ELEVATION_VIEW = 10u;
    const uint NO_STUDIO_STRIP = 1u << 7u;
    const uint ENVIRONMENT_REFLECTIONS = 1u << 22u;

    float pow5(float x)
    {
        float x2 = x * x;
        return x2 * x2 * x;
    }

    float dielectric_f0(float ior)
    {
        float r = (ior - 1.0) / (ior + 1.0);
        return r * r;
    }

    float schlick_fresnel(float cosine, float f0)
    {
        return mix(f0, 1.0, pow5(1.0 - clamp(cosine, 0.0, 1.0)));
    }

    float dielectric_specular(vec3 n, vec3 v, vec3 l, float roughness, float f0)
    {
        float nl = clamp(dot(n, l), 0.0, 1.0);
        float nv = clamp(dot(n, v), 0.001, 1.0);
        vec3 h = normalize(l + v);
        float nh = clamp(dot(n, h), 0.0, 1.0);
        float a = max(roughness * roughness, 0.02);
        float a2 = a * a;
        float f = nh * nh * (a2 - 1.0) + 1.0;
        float distribution = a2 / (PI * f * f);
        float visibility = 0.5 / max(1e-4, nv * sqrt(nl * nl * (1.0 - a2) + a2) +
                                    nl * sqrt(nv * nv * (1.0 - a2) + a2));
        return nl * distribution * visibility * schlick_fresnel(dot(v, h), f0);
    }

    vec3 environment_sample(vec2 uv, float pole)
    {
        return textureLod(EnvironmentTexture, vec2(uv.x, clamp(uv.y, pole, 1.0 - pole)), 0.0).rgb;
    }

    vec3 environment_radiance(vec3 direction, float roughness)
    {
        float azimuth = dot(direction.xy, direction.xy) > 1e-8 ? atan(direction.y, direction.x) : 0.0;
        float polar = acos(clamp(direction.z, -1.0, 1.0));
        vec2 uv = vec2(0.5 + (azimuth + Field.EnvironmentOptics.y) / (2.0 * PI), polar / PI);
        float pole = 0.5 / float(textureSize(EnvironmentTexture, 0).y);
        vec3 radiance = environment_sample(uv, pole);
        if (roughness > 0.12)
        {
            // 위도에 따라 압축되는 U 좌표를 보정해 거친 재질의 반사를 넓힌다.
            float horizontal = max(sin(polar), 0.35);
            vec2 spread = (0.067 * roughness * roughness) * vec2(1.0 / horizontal, 1.8);
            vec3 neighbours = environment_sample(uv + vec2(spread.x, 0.0), pole) +
                              environment_sample(uv - vec2(spread.x, 0.0), pole) +
                              environment_sample(uv + vec2(0.0, spread.y), pole) +
                              environment_sample(uv - vec2(0.0, spread.y), pole);
            radiance = mix(radiance, 0.25 * neighbours, 0.65 * smoothstep(0.12, 0.8, roughness));
        }
        radiance *= Field.EnvironmentOptics.x;
        // 기존 SDR 표시 경로에 더할 때 HDR 광원이 채널별로 바로 잘리는 것을 줄인다.
        return radiance / (1.0 + 0.22 * max(radiance.r, max(radiance.g, radiance.b)));
    }

    vec3 water_preview_sky(vec3 direction)
    {
        if ((Push.AppearanceFlags & ENVIRONMENT_REFLECTIONS) != 0u)
            return environment_radiance(direction, Field.LiquidOptics.w);
        return mix(vec3(0.20, 0.44, 0.62), vec3(0.73, 0.88, 0.98), max(0.0, direction.z));
    }

    vec3 studio_base_radiance(vec3 direction, float roughness)
    {
        float up = clamp(0.50 + 0.55 * direction.z, 0.0, 1.0);
        vec3 world = mix(vec3(0.030, 0.046, 0.073), vec3(0.54, 0.69, 0.85), up);
        // 넓은 조명과 거칠기 로브의 각분산을 합쳐 젖은 표면에는 더 집중된 반사를 만든다.
        const vec3 softbox_direction = normalize(vec3(-0.39, 0.74, 0.55));
        const float softbox_variance = 0.038;
        float variance = softbox_variance + 0.14 * roughness * roughness;
        float softbox_energy = softbox_variance / variance *
                               exp((dot(direction, softbox_direction) - 1.0) / variance);
        return world + 2.65 * softbox_energy * vec3(1.0, 0.98, 0.94);
    }

    vec3 studio_radiance(vec3 direction, float roughness)
    {
        vec3 radiance = (Push.AppearanceFlags & ENVIRONMENT_REFLECTIONS) != 0u
                        ? environment_radiance(direction, roughness)
                        : studio_base_radiance(direction, roughness);
        if ((Push.AppearanceFlags & NO_STUDIO_STRIP) != 0u)
            return radiance;

        // 월드 X를 조명 면에 투영해 긴 변을 만든다. 거칠기는 띠를 퍼뜨리고 최대 휘도를 낮춘다.
        const vec3 center = normalize(vec3(-0.32, 0.694, 0.646));
        const vec2 half_size = vec2(0.38, 0.014);
        vec3 axis_long = normalize(vec3(1.0, 0.0, 0.0) - center.x * center);
        vec3 axis_short = cross(center, axis_long);
        float front = dot(direction, center);
        if (front <= 0.0)
            return radiance;
        vec2 offset = vec2(dot(direction, axis_long), dot(direction, axis_short)) / front;
        float blur = 0.008 + 0.30 * roughness * roughness;
        vec2 profile = 1.0 - smoothstep(half_size - 0.5 * blur, half_size + blur, abs(offset));
        float peak = half_size.y / (half_size.y + blur);
        return radiance + 6.0 * peak * profile.x * profile.y * vec3(1.0, 0.98, 0.94);
    }

// #endregion

// ============================================================
// 01. Ice Lighting
// ============================================================
// #region 01_Ice_Lighting
    // 얼음에서 꺾인 광선이 빠져나가는 면과 배경을 찾아 내부 산란과 함께 표시한다.

    const float ICE_BACKDROP_GAP = 0.003;

    vec3 ice_backdrop(vec2 p, float softness)
    {
        vec2 rule = abs(fract(p / 0.016 + 0.5) - 0.5);
        float grid = 1.0 - smoothstep(0.012, 0.060 + 0.12 * softness, min(rule.x, rule.y));
        float reach = smoothstep(0.14, 0.35, length(p));
        vec3 slate = mix(vec3(0.130, 0.164, 0.184), vec3(0.024, 0.036, 0.055), reach);
        return slate * (1.0 - 0.22 * grid * (1.0 - reach) * (1.0 - 0.7 * softness));
    }

    struct IceExit
    {
        float distance;
        vec3 normal;
    };

    IceExit ice_exit(vec3 start, vec3 ray)
    {
        vec2 half_size = 0.5 * Field.SampleLayout.xy;
        vec2 edge = half_size * sign(ray.xy);
        vec3 hit = vec3(
            abs(ray.x) > 1e-5 ? (edge.x - start.x) / ray.x : 1.0,
            abs(ray.y) > 1e-5 ? (edge.y - start.y) / ray.y : 1.0,
            ray.z < -1e-5 ? (Field.BedMetrics.z - start.z) / ray.z : 1.0);
        float limit = max(0.0, min(hit.x, min(hit.y, hit.z)));
        vec3 normal = hit.x <= min(hit.y, hit.z) ? vec3(sign(ray.x), 0.0, 0.0) :
                      hit.y <= hit.z ? vec3(0.0, sign(ray.y), 0.0) : vec3(0.0, 0.0, -1.0);

        // The sides and bottom are planar. March only the potentially non-planar upper surface.
        float near = 0.0;
        for (int i = 1; i <= 112 && near < limit; ++i)
        {
            float far = min(limit, float(i) * 0.0025);
            vec3 p = start + ray * far;
            if (p.z > bed_height(p.xy) + 0.00035)
            {
                for (int j = 0; j < 5; ++j)
                {
                    float mid = 0.5 * (near + far);
                    p = start + ray * mid;
                    if (p.z > bed_height(p.xy) + 0.00035)
                        far = mid;
                    else
                        near = mid;
                }
                p = start + ray * far;
                vec2 dx = vec2(0.5 * Field.SampleLayout.z, 0.0);
                vec2 dy = vec2(0.0, 0.5 * Field.SampleLayout.w);
                vec2 slope = vec2(bed_height(p.xy + dx) - bed_height(p.xy - dx),
                                  bed_height(p.xy + dy) - bed_height(p.xy - dy)) / Field.SampleLayout.zw;
                return IceExit(far, normalize(vec3(-slope, 1.0)));
            }
            near = far;
        }
        return IceExit(limit, normal);
    }

    vec3 ice_environment_direction(vec3 local_ray)
    {
        if ((Push.AppearanceFlags & ENVIRONMENT_REFLECTIONS) == 0u)
            return local_ray;
        float s = Push.Pose.x, c = Push.Pose.y;
        return vec3(c * local_ray.x + s * local_ray.z, local_ray.y, -s * local_ray.x + c * local_ray.z);
    }

    vec3 ice_background(vec3 point, vec3 ray, float softness)
    {
        if (ray.z < -1e-4)
        {
            float distance = (Field.BedMetrics.z - ICE_BACKDROP_GAP - point.z) / ray.z;
            vec2 surface = point.xy + ray.xy * distance;
            if (distance > 0.0 && all(lessThanEqual(abs(surface), Field.SampleLayout.xy * vec2(2.0, 2.5))))
                return ice_backdrop(surface, softness);
        }
        return mix(vec3(0.020, 0.031, 0.046), 0.43 * studio_radiance(ice_environment_direction(ray), 0.6),
                   smoothstep(-0.08, 0.62, ray.z));
    }

    vec2 ice_hash(vec2 point)
    {
        vec3 h = fract(vec3(point.xyx) * vec3(0.1031, 0.1030, 0.0973));
        h += dot(h, h.yzx + 19.19);
        return fract((h.xx + h.yz) * h.zy);
    }

    float ice_grain(vec3 point)
    {
        vec3 cube = floor(point);
        vec3 f = fract(point);
        f = f * f * (3.0 - 2.0 * f);
        vec2 shift = cube.z * vec2(37.0, 19.0);
        vec2 base = cube.xy + shift;
        vec2 top = base + vec2(37.0, 19.0);
        float a = mix(ice_hash(base).x, ice_hash(base + vec2(1.0, 0.0)).x, f.x);
        float b = mix(ice_hash(base + vec2(0.0, 1.0)).x, ice_hash(base + vec2(1.0)).x, f.x);
        float c = mix(ice_hash(top).x, ice_hash(top + vec2(1.0, 0.0)).x, f.x);
        float d = mix(ice_hash(top + vec2(0.0, 1.0)).x, ice_hash(top + vec2(1.0)).x, f.x);
        return mix(mix(a, b, f.y), mix(c, d, f.y), f.z);
    }

    float ice_cracks(vec2 point)
    {
        vec2 tile = floor(point);
        vec2 local_point = fract(point);
        float first = 8.0;
        float second = 8.0;
        for (int y = -1; y <= 1; ++y)
            for (int x = -1; x <= 1; ++x)
            {
                vec2 cell = vec2(x, y);
                vec2 delta = cell + ice_hash(tile + cell) - local_point;
                float distance = dot(delta, delta);
                second = min(second, max(first, distance));
                first = min(first, distance);
            }
        return 1.0 - smoothstep(0.008, 0.045, second - first);
    }

    // 얼음층과 얼음 바닥이 공유하는 투과, 체적 산란, 표면 반사다.
    vec3 ice_bulk_color(vec3 behind, float path)
    {
        vec3 transmittance = exp(-vec3(2.3, 0.82, 0.31) * path);
        vec3 scatter = vec3(0.021, 0.050, 0.068) * (1.0 - exp(-8.0 * path));
        return behind * transmittance + scatter;
    }

    vec3 ice_interface_color(vec3 bulk, vec3 n, vec3 view, vec3 light, float roughness)
    {
        float f0 = dielectric_f0(Field.IceProperties.x);
        float fresnel = schlick_fresnel(dot(n, view), f0);
        vec3 reflected = studio_radiance(reflect(-view, n), roughness);
        return mix(bulk, reflected, fresnel) +
               vec3(0.95, 0.98, 1.0) * min(0.40, dielectric_specular(n, view, light, roughness, f0));
    }

    vec3 shade_ice(vec3 local_p, vec3 n, vec3 local_n, vec3 view, vec3 light, float wet)
    {
        float ior = Field.IceProperties.x;
        float slopes = sqrt(clamp(1.0 - local_n.z * local_n.z, 0.0, 1.0));
        float softness = 0.7 * slopes;
        vec3 ray = local_normal(refract(-view, n, 1.0 / ior));
        vec3 start = local_p - local_n * 0.00055;
        IceExit exit_face = ice_exit(start, ray);
        float path = exit_face.distance;
        vec3 inclusion = start + ray * min(0.022, path * 0.42);
        vec3 exit_point = start + path * ray;
        vec3 outgoing = refract(ray, -exit_face.normal, ior);
        float critical = sqrt(1.0 - 1.0 / (ior * ior));
        float transmission = smoothstep(critical, critical + 0.065, dot(ray, exit_face.normal));
        vec3 transmitted = ice_background(exit_point, outgoing, softness);
        if (transmission < 0.999)
        {
            ray = reflect(ray, exit_face.normal);
            start = exit_point - exit_face.normal * 0.00055;
            exit_face = ice_exit(start, ray);
            exit_point = start + ray * exit_face.distance;
            outgoing = refract(ray, -exit_face.normal, ior);
            float escape = smoothstep(critical, critical + 0.065, dot(ray, exit_face.normal));
            vec3 reflected = 0.45 * studio_radiance(ice_environment_direction(reflect(ray, exit_face.normal)), 0.75);
            vec3 bounced = mix(reflected, ice_background(exit_point, outgoing, min(1.0, softness + 0.34)), escape);
            transmitted = mix(bounced, transmitted, transmission);
            path += exit_face.distance * (1.0 - transmission);
        }

        float broad_grain = ice_grain(inclusion * 69.0);
        float fine_grain = ice_grain(inclusion * 415.0);
        float frost = smoothstep(0.57, 0.85, 0.57 * broad_grain + 0.27 * fine_grain + 0.16 * slopes) *
                      (1.0 - wet * 0.65);
        vec2 fracture_point = vec2(inclusion.x, inclusion.y + 0.4 * inclusion.z) * vec2(84.0, 65.0);
        fracture_point += vec2(0.26 * broad_grain, 0.17 * fine_grain);
        float cracks = ice_cracks(fracture_point) * smoothstep(0.50, 0.85, broad_grain);
        float roughness = mix(clamp(Field.IceOptics.x + 0.11 * slopes, 0.0, 1.0), Field.IceOptics.y, frost);
        vec3 bulk = ice_bulk_color(transmitted, path);
        bulk = mix(bulk, vec3(0.40, 0.55, 0.61), 0.23 * frost + 0.08 * cracks);
        return ice_interface_color(bulk, n, view, light, roughness);
    }

// #endregion

// ============================================================
// 02. Material and Elevation Shading
// ============================================================
// #region 02_Material_and_Elevation_Shading
    // 바닥 재질의 유전체 반사와 높이 표시의 등고선 강도를 계산한다.

    vec3 shade_dielectric(vec3 base, vec3 n, vec3 reflection_normal, vec3 view, vec3 light, float roughness, float f0)
    {
        float nl = max(0.0, dot(n, light));
        float fresnel = schlick_fresnel(dot(reflection_normal, view), f0);
        vec3 diffuse = base * (vec3(0.31, 0.34, 0.38) + nl * vec3(0.89, 0.83, 0.73));
        return diffuse * (1.0 - fresnel) + studio_radiance(reflect(-view, reflection_normal), roughness) * fresnel +
               vec3(0.96, 0.91, 0.83) * min(0.50, dielectric_specular(reflection_normal, view, light, roughness, f0));
    }

    vec3 shade_dielectric(vec3 base, vec3 n, vec3 view, vec3 light, float roughness, float f0)
    {
        return shade_dielectric(base, n, n, view, light, roughness, f0);
    }

    vec3 shade_steel(vec3 base, vec3 n, vec3 view, vec3 light, float roughness)
    {
        vec3 reflectance = mix(base, vec3(1.0), pow5(1.0 - clamp(dot(n, view), 0.0, 1.0)));
        // 백색 스트립 조명이 은회색 바탕의 방향성 반사를 드러낸다.
        vec3 reflection = reflect(-view, n);
        float ambient = dot(studio_radiance(reflection, roughness), vec3(0.2126, 0.7152, 0.0722));
        float across_strip = abs(dot(reflection, normalize(vec3(0.10, -0.65, 0.75))));
        float panel = 1.0 - smoothstep(0.05, 0.38, across_strip);
        float highlight = min(0.38, dielectric_specular(n, view, light, roughness, dielectric_f0(2.2)));
        return reflectance * (0.88 * ambient + 0.23 * panel + 0.35 * highlight);
    }

    float contour(float height, float spacing)
    {
        float bands = height / spacing;
        float distance = abs(fract(bands + 0.5) - 0.5);
        float width = clamp(fwidth(bands), 0.015, 0.30);
        return 1.0 - smoothstep(width * 0.46, width * 1.30, distance);
    }

// #endregion
