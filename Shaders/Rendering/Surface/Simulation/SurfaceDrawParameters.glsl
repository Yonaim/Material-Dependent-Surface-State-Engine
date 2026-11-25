// Surface Simulation의 그래픽 패스가 공유하는 뷰, 레이어, 카메라 및 재질 입력이다.

// ============================================================
// 00. Draw Interface
// ============================================================
// #region 00_Draw_Interface
    // SurfaceSimulationDisplay.cpp의 FDrawPush와 레이아웃을 공유한다. Pose는 기울기의 sin과 cos이며 CameraTime.w는 장면 시간(s)이다.

    const uint PBR_SURFACE_MODE = 9u;
    const uint SNOW_LAYER = 4u;
    const uint LAVA_DEPOSIT_LAYER = 5u;
    const uint OVERFLOW_LAYER = 6u;
    const uint WATER_JOIN_LAYER = 7u;
    const uint SECTION_WINDOW_LAYER = 8u;
    const uint SECTION_WINDOW_BACK_LAYER = 9u;
    const uint VESSEL_CONTENTS_LAYER = 10u;
    const uint STEAM_VOLUME_LAYER = 2u;
    const uint LAVA_DEPOSIT = 1u << 5u;
    const uint OVERFLOW_FILM_ON_BED = 1u << 6u;
    const uint METAL_SUBSTRATE = 1u << 17u;
    const uint INCANDESCENT_LIQUID = 1u << 19u;
    const uint CHART_ALBEDO = 1u << 20u;
    const uint INCANDESCENT_SUBSTRATE = 1u << 21u;
    const uint SPECIMEN_MATERIALS = 1u << 23u;
    const float OVERFLOW_LIFETIME_S = 0.22;
    const uint SMOOTH_LIQUID_BOUNDARY = 1u;
    const uint SLOSH_OPTICS = 1u << 1u;
    const uint SLOSH_HEIGHT = 1u << 2u;

    layout(push_constant) uniform Draw
    {
        mat4 ViewProjection;
        vec4 CameraTime;
        vec4 MaterialScale;
        vec2 Pose;
        uint Layer;
        uint Mode;
        uint Palette;
        uint ViewSize;
        uint AppearanceFlags;
        uint LiquidOptions;
    } Push;

    bool liquid_option_enabled(uint option)
    {
        return (Push.LiquidOptions & option) != 0u;
    }

    float slosh_amplitude()
    {
        // Host와 같은 signed 16비트 값이다. 아래 비트는 뷰별 표시 옵션으로 사용한다.
        return float(int(Push.LiquidOptions) >> 16) / 32767.0;
    }

// #endregion
