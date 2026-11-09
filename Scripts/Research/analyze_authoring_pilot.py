"""Score the frozen task's numeric predicates; visual/human quality is separate."""

# ============================================================
# 00. Imports and Module Configuration
# ============================================================
# region 00_Imports_and_Module_Configuration

import argparse
import pathlib
import sys

from authoring_study import ROOT, STUDY, read, write, stamp

sys.path.insert(0, str(ROOT / 'Build/ResearchPython'))
import numpy as np

OUT = STUDY / 'goal-v1/pilot'
OLD = STUDY / 'layout-lifetime-fix/render/raw/absorbedwetness'


# endregion

# ============================================================
# 01. Processing Helpers
# ============================================================
# region 01_Processing_Helpers


def load(kind):
    plan = read(OUT / (kind + '-plan.json'))
    record = read(OUT / (kind + '-manifest.json'))
    prefix = pathlib.Path(plan['output'])
    log = prefix.with_suffix('.log').read_text(encoding='utf-8')
    errors = [x for x in log.splitlines() if '[ERROR]' in x or 'Validation Error' in x]
    skips = [x for x in log.splitlines() if 'recreat' in x.lower() and 'swapchain' in x.lower()]
    final = read(pathlib.Path(str(prefix) + '.validation.json'))
    actual = read(pathlib.Path(str(prefix) + '.settings.json'))
    snaps = {
        s: read(pathlib.Path(str(prefix) + f'.step-{s}.json')) for s in plan['required_checkpoints']
    }
    assert all(x['simulation_step_serial'] == s for s, x in snaps.items())
    assert all(
        len(x['instances']) == 1 and x['instances'][0]['id'] == 'b_heated' for x in snaps.values()
    )
    assert all(abs(x['demo_time_seconds'] - s / 60) < 0.0084 for s, x in snaps.items() if s)
    assert final['simulation_step_serial'] == plan['final_requested_serial'] + 2
    channels = snaps[0]['channels']
    index = snaps[0]['instances'][0]['instance']

    def buffer(s, name, dtype='<f4'):
        return np.fromfile(str(prefix) + f'.step-{s}.instance-{index}-{name}.bin', dtype=dtype)

    states = {s: buffer(s, 'state').reshape(-1, len(channels)) for s in snaps}
    assert not np.any(states[0]), 'Initial State must still be empty before the first solver'
    initial_input = buffer(0, 'initial-input').reshape(-1, len(channels))
    areas = buffer(0, 'area').astype('float64')
    normals = buffer(0, 'normal').reshape(-1, 4)
    positions = buffer(0, 'position').reshape(-1, 4)
    surfaces = buffer(0, 'surface', '<u4')
    profiles = buffer(0, 'profile', '<u4')
    valid = (surfaces != 0xFFFFFFFF) & (profiles != 0xFFFFFFFF) & (areas > 0)
    top = (
        valid & (normals[:, 2] > 0.99) & (abs(positions[:, 2] - np.max(positions[valid, 2])) < 1e-5)
    )
    assert len(np.unique(surfaces[top])) == 1 and areas[top].sum() > 0
    # Production buffer is three vec4 SoA planes, not the logical 48-byte AoS struct.
    parameters = np.fromfile(str(prefix) + '.step-0.profile-parameters.bin', dtype='<f4').reshape(
        3, -1, len(channels), 4
    )
    capacity = np.ones_like(initial_input, dtype='float64')
    capacity[valid] = parameters[0, profiles[valid], :, 0] * areas[valid, None] * 65536
    assert np.all(capacity[valid] > 0)
    initial = states[0].astype('float64') + initial_input
    cov = {s: np.clip(q / capacity, 0, 1) for s, q in states.items()}
    cov[0] = np.clip(initial / capacity, 0, 1)
    state_bad = {
        s: dict(nonfinite=int((~np.isfinite(q)).sum()), negative=int((q < 0).sum()))
        for s, q in states.items()
    }
    dynamic_bad = {}
    for s in snaps:
        if s:
            geom = buffer(s, 'dynamic-geometry').reshape(-1, 2, 4)
            dynamic_bad[s] = int((~np.isfinite(geom[valid])).sum())
    flags = sum(
        x['reaction_budget_flags_at_checkpoint'] + x['reaction_arithmetic_flags_at_checkpoint']
        for s, snap in snaps.items()
        if s
        for x in snap['instances']
    )
    fixed = dict(
        geometry_feedback=True,
        raw_flux_cache=True,
        fp16_raw_flux=False,
        fp16_weights=False,
        sparse_solver=False,
        sparse_height=False,
        sparse_geometry=False,
        active_channel_mask=False,
        per_workgroup_channel_mask=False,
        surface_mesh_resolution=128,
        overlay_mesh_resolution=128,
    )
    settings_ok = all(actual.get(k) == v for k, v in fixed.items())
    image_paths = [pathlib.Path(str(prefix) + f'.step-{s}.ppm') for s in snaps if s]
    capture_ok = all(p.is_file() and p.stat().st_size > 1000 for p in image_paths)
    gate0 = (
        record['status'] == 'completed'
        and not errors
        and not skips
        and settings_ok
        and capture_ok
        and not flags
        and all(not x['nonfinite'] and not x['negative'] for x in state_bad.values())
        and not any(dynamic_bad.values())
        and all(not x['nonfinite'] and not x['negative'] for x in final['instances'])
    )
    audit = dict(
        created_at=stamp(),
        runtime_error_lines=errors,
        swapchain_skip_lines=skips,
        state_checks=state_bad,
        dynamic_nonfinite=dynamic_bad,
        checkpoint_reaction_flags=flags,
        final_serial=final['simulation_step_serial'],
        settings_match=settings_ok,
        images_complete=capture_ok,
        initial_input_sums=dict(zip(channels, initial.sum(axis=0).tolist())),
        target_instance=index,
        top_face_ids=np.unique(surfaces[top]).tolist(),
        top_reference_area=float(areas[top].sum()),
        top_texels=int(top.sum()),
        g0_passed=bool(gate0),
        initial_scope='Initial rasterized InputDelta plus zero CurrentState before first step, not a post-solver snapshot',
        flag_scope='Checkpoints and final only; not a full per-step budget audit',
        actual_checkpoint_times={
            s: dict(demo=x['demo_time_seconds'], simulation=x['simulated_seconds'])
            for s, x in snaps.items()
        },
    )
    return plan, prefix, channels, states, initial, cov, areas, top, audit


def analyze(kind):
    plan, prefix, names, states, initial, cov, area, top, audit = load(kind)
    if kind == 'compatibility':
        arrays = []
        for old in sorted(OLD.glob('capture.jsonl.state-*.f32')):
            new = pathlib.Path(str(prefix) + old.name[len('capture.jsonl') :])
            a = np.fromfile(old, dtype='<u4')
            b = np.fromfile(new, dtype='<u4')
            arrays.append(dict(file=old.name, bitwise_equal=bool(np.array_equal(a, b))))
        previous = read(STUDY / 'layout-lifetime-fix/render/manifest.json')['source_hashes']
        current = read(OUT / (kind + '-manifest.json'))['source_sha256']
        shader_changes = [
            p
            for p, h in previous.items()
            if (p.startswith('Shaders') or p.endswith('.spv')) and current.get(p) != h
        ]
        source_changes = [
            p for p, h in previous.items() if p.startswith('Source') and current.get(p) != h
        ]
        passed = (
            audit['g0_passed']
            and len(arrays) == 3
            and all(x['bitwise_equal'] for x in arrays)
            and not shader_changes
        )
        write(
            OUT / 'compatibility-analysis.json',
            dict(
                passed=bool(passed),
                audit=audit,
                state_arrays=arrays,
                shader_changes=shader_changes,
                source_changes=source_changes,
                limitation='Three final arrays against prior clean capture; consistency, not physical ground truth or complete repository tests',
            ),
        )
        print('compatibility passed:', passed, 'arrays:', len(arrays))
        return
    ice = names.index('ice')
    water = names.index('waterfilm')
    wet = names.index('absorbedwetness')
    initial_ice = cov[0][:, ice] >= 0.10
    outside = top & ~initial_ice
    ratio = float(states[360][:, ice].sum(dtype='float64') / initial[:, ice].sum())
    union = np.zeros(len(area), dtype=bool)
    for s, c in cov.items():
        if 0 < s <= 480:
            union |= c[:, water] >= 0.10
    transport_area = float(area[outside & union].sum() / area[top].sum())
    visited = np.zeros(len(area), dtype=bool)
    for s, c in cov.items():
        if 0 < s < 720:
            visited |= c[:, water] >= 0.10
    common = outside & visited
    per_time = []
    for s in [720, 840, 960]:
        c = cov[s]
        mask = outside & visited & (c[:, water] <= 0.05) & (c[:, ice] <= 0.05) & (c[:, wet] >= 0.25)
        common &= mask
        per_time.append(
            dict(
                step=s,
                seconds=s / 60,
                trace_area_fraction=float(area[mask].sum() / area[top].sum()),
            )
        )
    trace_area = float(area[common].sum() / area[top].sum())
    metrics = []
    for s, q in states.items():
        if not s:
            q = initial
        metrics.append(
            dict(
                step=s,
                seconds=s / 60,
                state_sums=dict(zip(names, q.sum(axis=0, dtype='float64').tolist())),
                outside_water_area_fraction=float(
                    area[outside & (cov[s][:, water] >= 0.1)].sum() / area[top].sum()
                ),
                outside_wet_area_fraction=float(
                    area[outside & (cov[s][:, wet] >= 0.25)].sum() / area[top].sum()
                ),
            )
        )
    result = dict(
        created_at=stamp(),
        task_id='AUTH-GOAL-01-v1',
        candidate=kind,
        audit=audit,
        numeric_gates=dict(
            G0=audit['g0_passed'],
            G1_quantity=0.05 <= ratio <= 0.80,
            G2=transport_area >= 0.01,
            G3_quantity=trace_area >= 0.01,
        ),
        ice_fraction_at_6s=ratio,
        transport_area_fraction_by_8s=transport_area,
        persistent_trace_area_fraction=trace_area,
        trace_by_time=per_time,
        checkpoints=metrics,
        G1_visual='pending_AI_review',
        G3_visual='pending_AI_review',
        G4='not_run',
        overall='not_complete_visual_and_reload_unresolved',
        human_sessions=0,
        interpretation='Task-design predicates only; no productivity, physical accuracy, comparative or novelty conclusion',
    )
    np.savez_compressed(
        prefix.parent / 'masks.npz',
        top=top,
        initial_ice=initial_ice,
        persistent_trace=common,
        area=area,
    )
    write(OUT / (kind + '-analysis.json'), result)
    print(result['numeric_gates'])
    print('Ice fraction', ratio, 'transport', transport_area, 'trace', trace_area)


# endregion

# ============================================================
# 02. Script Execution
# ============================================================
# region 02_Script_Execution

if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('kind', choices=['compatibility', 'candidate-01', 'candidate-01-retry'])
    analyze(ap.parse_args().kind)

# endregion
