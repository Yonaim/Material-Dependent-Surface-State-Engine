/**
 * @file WaterFilm.glsl
 * @brief 얇은 물막에 젖은 바탕색과 매끈한 표면 반응을 적용한다.
 */
float VisibleWaterCoverage(float Coverage, vec2 UV)
{
    if (Material.WaterAppearance.x < 0.5)
        return Coverage;
    // Leather beads water: the simulated state still moves continuously, while
    // its material appearance resolves into small, separate drops.
    vec2 Grid = UV * Material.WaterAppearance.w;
    vec2 Cell = floor(Grid);
    vec2 Local = fract(Grid);
    float H1 = fract(sin(dot(Cell, vec2(127.1, 311.7))) * 43758.5453);
    float H2 = fract(sin(dot(Cell, vec2(269.5, 183.3))) * 43758.5453);
    vec2 Center = vec2(0.28) + 0.44 * vec2(H1, H2);
    float Radius = length(Local - Center);
    float Antialias = max(fwidth(Radius), 0.012);
    float Drop = 1.0 - smoothstep(0.22 - Antialias, 0.27 + Antialias, Radius);
    return Coverage * Drop;
}

void ApplyWaterFilm(float Coverage, inout vec3 Albedo, inout float Roughness, float FilmRoughness)
{
    float Blend = smoothstep(0.02, 0.85, clamp(Coverage, 0.0, 1.0));
    Albedo *= mix(1.0, 1.0 - Material.WaterAppearance.z, Blend);
    Roughness = mix(Roughness, FilmRoughness, Blend);
}
