#version 450

// 젖은 수면의 내부 연결 면과 실제 월류를 나타내는 물띠를 그린다.
#extension GL_GOOGLE_include_directive : require

// ============================================================
// 00. Vertex Interface
// ============================================================
// #region 00_Vertex_Interface
    // SurfaceFragmentMain.glsl에 수면과 같은 월드 위치, 방향, 광학 두께를 보낸다.

    #include "SurfaceDrawParameters.glsl"
    #include "SurfaceDisplayField.glsl"
    #include "LiquidSurfaceReconstruction.glsl"

    layout(location = 0) out vec3 World;
    layout(location = 1) out vec3 Normal;
    layout(location = 2) out vec3 Local;
    layout(location = 3) out float Depth;
    layout(location = 4) flat out vec4 DebugValue;
    layout(location = 5) flat out vec3 Activity;
    layout(location = 6) out float OverflowWetness;
    layout(location = 7) flat out uint CellId;
    layout(location = 8) out float ShoreAppearanceDepth;

// #endregion

// ============================================================
// 01. Overflow Ribbon
// ============================================================
// #region 01_Overflow_Ribbon
    // 접촉 지점에서 시작해 수위가 낮은 쪽으로 이어지는 표시용 물띠다.

    vec3 rotate_y(vec3 p)
    {
        float s = Push.Pose.x;
        float c = Push.Pose.y;
        return vec3(c * p.x + s * p.z, p.y, -s * p.x + c * p.z);
    }

    bool contains_sample(ivec2 at)
    {
        return all(greaterThanEqual(at, ivec2(0))) && all(lessThan(at, Field.Counts.xy));
    }

    int sample_index(ivec2 at)
    {
        return at.y * Field.Counts.x + at.x;
    }

    vec2 sample_center(ivec2 at)
    {
        return (vec2(at) + 0.5) * Field.SampleLayout.zw - 0.5 * Field.SampleLayout.xy;
    }

    int attachment_offset(ivec2 start, ivec2 direction, uint region, float level, float crossing_level)
    {
        int reach = 0;
        // 턱 위에 저장 영역 경계가 걸쳐도, 같은 영역의 물까지 최대 두 표본을 더 따라간다.
        for (int offset = 0; offset <= 2; ++offset)
        {
            ivec2 at = start + direction * offset;
            if (!contains_sample(at))
                break;
            int index = sample_index(at);
            if (field_sample_regions(int(index)) != region)
                break;
            float bed = rotate_y(vec3(sample_center(at), field_heights(int(index)))).z;
            if (offset > 0 && bed > crossing_level)
                break;
            reach = offset;
            if (level > bed + 0.00008)
                break;
        }
        return reach;
    }

    bool continues_side(ivec2 adjacent, int axis, int first, int second, bool receiver_is_first)
    {
        if (!contains_sample(adjacent))
            return false;
        int at = sample_index(adjacent);
        int contact = field_contact_indices(int(2 * at + axis));
        if (contact < 0)
            return false;
        int other = at + (axis == 0 ? 1 : Field.Counts.x);
        if (field_heights(int(at)) != field_heights(int(first)) || field_heights(int(other)) != field_heights(int(second)))
            return false;
        for (int direction = 0; direction < 2; ++direction)
        {
            OverflowTransferRecord flow = display_overflow(int(2 * contact + direction));
            if (flow.mass_kg > 0.0 && Push.CameraTime.w - flow.last_transfer_s < OVERFLOW_LIFETIME_S &&
                (field_sample_regions(int(at)) == flow.receiver) == receiver_is_first)
                return true;
        }
        return false;
    }

// #endregion

// ============================================================
// 02. Shared Water Edge
// ============================================================
// #region 02_Shared_Water_Edge
    float smooth_bed_corner(ivec2 corner)
    {
        ivec2 far_corner = Field.Counts.xy - 1;
        ivec2 low = clamp(corner - 1, ivec2(0), far_corner);
        ivec2 high = clamp(corner, ivec2(0), far_corner);
        return 0.25 * (field_heights(sample_index(low)) + field_heights(sample_index(ivec2(low.x, high.y))) +
                       field_heights(sample_index(ivec2(high.x, low.y))) + field_heights(sample_index(high)));
    }

    void draw_water_join()
    {
        if (Field.PhaseLevels.y > Field.PhaseLevels.x || (Push.AppearanceFlags & LAVA_DEPOSIT) != 0u)
            return;

        int contact = field_contact_slot(gl_InstanceIndex);
        int first = contact / 2;
        ivec2 sample_at = ivec2(first % Field.Counts.x, first / Field.Counts.x);
        ivec2 across = contact % 2 == 0 ? ivec2(1, 0) : ivec2(0, 1);
        ivec2 adjacent = sample_at + across;
        if (!contains_sample(adjacent))
            return;

        int second = sample_index(adjacent);
        int owner = int(field_sample_regions(first));
        int neighbor = int(field_sample_regions(second));
        if (owner == neighbor)
            return;

        vec2 a = display_region(owner);
        vec2 b = display_region(neighbor);
        float lower_level = min(a.y, b.y);
        float upper_level = max(a.y, b.y);
        if (min(a.x, b.x) <= 0.0 || upper_level <= lower_level)
            return;

        vec2 chart_center = 0.5 * Field.SampleLayout.xy;
        vec2 pitch = Field.SampleLayout.zw;
        vec2 middle = (vec2(sample_at) + 0.5 + 0.5 * vec2(across)) * pitch - chart_center;
        float base_crest = rotate_y(vec3(middle, max(field_base_height(first), field_base_height(second)))).z;
        float crest_level = max(base_crest, max(display_effective_floor(owner), display_effective_floor(neighbor)));
        if (lower_level <= crest_level)
            return;

        const int corners[6] = int[6](0, 1, 2, 2, 1, 3);
        int corner = corners[gl_VertexIndex];
        ivec2 edge = adjacent + (corner % 2) * across.yx;
        Local = vec3(vec2(edge) * pitch - chart_center, smooth_bed_corner(edge));
        World = rotate_y(Local);
        Depth = lower_level - World.z;
        if (smooth_single_water_surface())
        {
            // 상면과 같은 복원을 사용해 높이가 일치한 곳에서는 수직 연결 면이 접힌다.
            float depth_a = shore_appearance_depth(Local.xy, uint(owner), a.y, World.z, true, false);
            float depth_b = shore_appearance_depth(Local.xy, uint(neighbor), b.y, World.z, true, false);
            Depth = min(depth_a, depth_b);
            lower_level = World.z + Depth;
            upper_level = World.z + max(depth_a, depth_b);
        }
        // 수직 연결 면은 수평 투영에서 면적과 부피 기여가 0이다.
        World.z = corner < 2 ? lower_level : upper_level;
        World += rotate_y(vec3(display_gap_offset(specimen_at(sample_center(sample_at).x)), 0.0, 0.0));
        gl_Position = Push.ViewProjection * vec4(World, 1.0);
    }

// #endregion

// ============================================================
// 03. Draw Entry
// ============================================================
// #region 03_Draw_Entry
    void main()
    {
        DebugValue = vec4(0.0);
        Activity = vec3(0.0);
        OverflowWetness = 0.0;
        ShoreAppearanceDepth = 0.0;
        CellId = 0u;
        World = vec3(0.0);
        Normal = vec3(0.0, 0.0, 1.0);
        Local = vec3(0.0);
        Depth = 0.0;
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        if (Push.Layer == WATER_JOIN_LAYER)
        {
            draw_water_join();
            return;
        }

        OverflowTransferRecord flow = display_overflow(int(gl_InstanceIndex));
        float age = max(0.0, Push.CameraTime.w - flow.last_transfer_s);
        if (flow.mass_kg <= 0.0 || age >= OVERFLOW_LIFETIME_S)
            return;

        int first = int(flow.contact / 2u);
        int axis_index = int(flow.contact % 2u);
        bool axis_x = axis_index == 0;
        int second = first + (axis_x ? 1 : Field.Counts.x);
        bool receiver_is_first = field_sample_regions(int(first)) == flow.receiver;
        ivec2 first_sample = ivec2(first % Field.Counts.x, first / Field.Counts.x);
        ivec2 unit = axis_x ? ivec2(1, 0) : ivec2(0, 1);
        ivec2 lateral = unit.yx;
        ivec2 donor_start = receiver_is_first ? first_sample + unit : first_sample;
        ivec2 receiver_start = receiver_is_first ? first_sample : first_sample + unit;
        ivec2 direction = receiver_is_first ? -unit : unit;
        vec2 pitch = Field.SampleLayout.zw;
        vec2 edge = sample_center(first_sample) + 0.5 * vec2(unit) * pitch;
        vec2 travel = vec2(direction) * pitch;
        int first_region = int(field_sample_regions(first));
        int second_region = int(field_sample_regions(second));
        float base_crest = max(field_base_height(first), field_base_height(second));
        float crest = height_above_film_floor(base_crest, edge.x,
                                            max(display_effective_floor(first_region), display_effective_floor(second_region)));
        float crest_level = rotate_y(vec3(edge, crest)).z;
        uint donor_id = field_sample_regions(int(sample_index(donor_start)));
        vec2 donor = display_region(int(donor_id));
        vec2 receiver = display_region(int(flow.receiver));
        // 양쪽 수면이 턱을 덮으면 물띠가 수면에 겹쳐 줄무늬를 만든다.
        if (donor.x > 0.0 && receiver.x > 0.0 && min(donor.y, receiver.y) >= crest_level)
            return;
        float width = clamp(sqrt(flow.mass_kg / (1000.0 * pitch.x * pitch.y * flow.head_m)), 0.70, 0.99);
        float top = crest_level + flow.head_m;
        int approach = attachment_offset(donor_start, -direction, donor_id, donor.y, top);
        int run = attachment_offset(receiver_start, direction, flow.receiver, receiver.y, top);
        ivec2 landing_sample = receiver_start + direction * run;
        float start_bed = field_heights(int(sample_index(donor_start - direction * approach)));
        float bottom = field_heights(int(sample_index(landing_sample)));
        float lip_bed = run > 0 ? field_heights(int(sample_index(landing_sample - direction))) : crest;
        vec3 start = rotate_y(vec3(edge - (float(approach) + 0.74) * travel, start_bed));
        vec3 crossing = rotate_y(vec3(edge, crest));
        vec3 lip = rotate_y(vec3(edge + (float(run) + 0.04) * travel, max(lip_bed, bottom)));
        vec3 foot = rotate_y(vec3(edge + (float(run) + 0.16) * travel, bottom));
        vec3 landing = rotate_y(vec3(edge + (float(run) + 0.74) * travel, bottom));
        float crest_water = max(crest_level, min(top, donor.y));
        vec3 centers[5] = vec3[5](vec3(start.xy, max(start.z, min(top, donor.y))),
                                  vec3(crossing.xy, crest_water),
                                  vec3(lip.xy, max(lip.z, min(crest_water, receiver.y))),
                                  vec3(foot.xy, max(foot.z, receiver.y)),
                                  vec3(landing.xy, max(landing.z, receiver.y)));
        vec3 tangent = rotate_y(vec3(vec2(lateral) * pitch, 0.0));

        const int corners[6] = int[6](0, 1, 2, 2, 1, 3);
        int segment = gl_VertexIndex / 6;
        int corner = corners[gl_VertexIndex % 6];
        int row = segment + corner / 2;
        int side = corner % 2 == 0 ? -1 : 1;
        bool joined = continues_side(first_sample + side * lateral, axis_index, first, second, receiver_is_first);
        World = centers[row] + 0.5 * float(side) * (joined ? 1.0 : width) * tangent + vec3(0.0, 0.0, 0.00008);
        Normal = normalize(cross(centers[segment + 1] - centers[segment], tangent));
        if (Normal.z < 0.0)
            Normal = -Normal;
        // 표면 상태와 별개로 물띠 위 좌표와 마지막 흐름 이후의 표시 강도를 넘긴다.
        const float progress[5] = float[5](0.0, 0.34, 0.64, 0.83, 1.0);
        Local = vec3(joined ? 0.0 : float(side), progress[row], 0.0);
        Depth = clamp(flow.head_m, 0.00035, 0.0018);
        DebugValue.x = 1.0 - smoothstep(0.06, OVERFLOW_LIFETIME_S, age);
        World += rotate_y(vec3(display_gap_offset(specimen_at(sample_center(first_sample).x)), 0.0, 0.0));
        gl_Position = Push.ViewProjection * vec4(World, 1.0);
    }

// #endregion
