"""Prepare a falsifiable single-system authoring task; do not run or score it."""

# ============================================================
# 00. Imports and Module Configuration
# ============================================================
# region 00_Imports_and_Module_Configuration

import copy
import os
import pathlib
import subprocess

from authoring_study import ROOT, STUDY, digest, read, stamp, write

OUT = STUDY / 'goal-v1'
SOURCE_SCENE = ROOT / 'Assets/Scenes/IceReactionLab.Scene'
SOURCE_ANIMATION = ROOT / 'Assets/Animations/IceReactionLab.DemoAnim'
SOURCE_PROFILE = ROOT / 'Assets/SurfaceProfiles/IceReactionLab.SRProfile'
SOURCE_MAP = ROOT / 'Assets/SurfaceProfileMaps/IceReactionLab.SurfaceProfileMap'


# endregion

# ============================================================
# 01. Processing Helpers
# ============================================================
# region 01_Processing_Helpers


def relative(path, directory):
    return os.path.relpath(path, directory).replace('\\', '/')


def prepare():
    if OUT.exists():
        raise RuntimeError('Refusing to overwrite an existing task specification or submission')
    scene = read(SOURCE_SCENE)
    animation = read(SOURCE_ANIMATION)
    # This is a new, disclosed task input, not a re-analysis of the six old renders.
    animation['durationSeconds'] = 16
    animation['tracks'] = []  # Fixed scene camera; no moving view to hide the trace.
    animation['contactEvents'] = [e for e in animation['contactEvents'] if e['time'] <= 4]
    distribution = read(SOURCE_MAP)
    distribution['profiles'] = ['Editable.SRProfile']
    artifacts = []
    for name in ['start', 'work']:
        folder = OUT / 'raw' / name
        s = copy.deepcopy(scene)
        s['animation'] = 'Task.DemoAnim'
        for obj in s['objects']:
            obj['mesh'] = relative((SOURCE_SCENE.parent / obj['mesh']).resolve(), folder)
            if 'surfaceProfileMap' in obj:
                obj['surfaceProfileMap'] = 'Editable.SurfaceProfileMap'
        for filename, data in [
            ('Task.Scene', s),
            ('Task.DemoAnim', animation),
            ('Editable.SurfaceProfileMap', distribution),
            ('Editable.SRProfile', read(SOURCE_PROFILE)),
        ]:
            p = folder / filename
            write(p, data)
            artifacts.append(p)
    checks = []
    for name in ['start', 'work']:
        folder = OUT / 'raw' / name
        s = read(folder / 'Task.Scene')
        refs = [s['animation']]
        for obj in s['objects']:
            refs.append(obj['mesh'])
            if 'surfaceProfileMap' in obj:
                refs.append(obj['surfaceProfileMap'])
        refs.extend(read(folder / 'Editable.SurfaceProfileMap')['profiles'])
        checks.append(
            dict(
                folder=name,
                relative_paths_only=all(not pathlib.Path(x).is_absolute() for x in refs),
                references_exist=all((folder / x).is_file() for x in refs),
            )
        )
    assert all(x['relative_paths_only'] and x['references_exist'] for x in checks)
    assert not animation['tracks'] and max(e['time'] for e in animation['contactEvents']) == 4
    assert not any(
        e['target'] == 'b_heated' and e['state'] != 'heat' for e in animation['contactEvents']
    )
    by_target = {}
    for e in animation['contactEvents']:
        key = e['target'] + '/' + e['state']
        by_target[key] = by_target.get(key, 0) + 1
    plan = dict(
        schema_version=1,
        task_id='AUTH-GOAL-01-v1',
        created_at=stamp(),
        status='specification_prepared_execution_not_run',
        scope='AI-assisted single-system feasibility pilot; no human productivity or baseline comparison',
        source_commit=subprocess.check_output(
            ['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True
        ).strip(),
        source_version='Commit plus dirty working tree; source hashes below identify preparation inputs only',
        prior_knowledge='PERF-041/042/043 and six approximately-six-second reference renders are known. '
        'This goal is informed by that knowledge, not independently or blindly selected.',
        report='Docs/MDSSP-Agent-Vault/03_Development/06_Authoring-Study-Protocol.md#9',
        primary_object='b_heated',
        contextual_objects=['a_finite', 'c_cooled'],
        duration_seconds=16,
        last_external_input_seconds=4,
        requested_checkpoints_seconds=[0, 1, 2, 4, 6, 8, 10, 12, 14, 16],
        initial_snapshot='After initialContacts, before the first solver step; not frame zero after an update',
        screenshot_state_alignment='Record actual simulation serial and demo time. Old benchmark drain-end '
        'State is not interchangeable with a screenshot-time State.',
        input_event_counts=by_target,
        fixed_configuration=dict(
            simulation_resolution=128,
            render_mesh_resolution=128,
            fixed_dt_seconds=1 / 60,
            geometry_feedback=True,
            raw_flux_cache=True,
            fp16_raw_flux=False,
            fp16_weights=False,
            sparse_solver=False,
            sparse_height=False,
            sparse_geometry=False,
            camera='Unchanged Scene camera; animation camera tracks removed',
            appearance='Existing built-in lit effects, fixed lighting and display scale',
        ),
        editable=[
            'work/Editable.SRProfile numerical response parameters',
            'Existing ice_melting and water_cooling rate/modifier parameters; keep both rules',
            'Add absorbedwetness and one existing inflowFraction waterfilm-to-absorbedwetness rule',
        ],
        locked=[
            'Engine/solver/shaders',
            'Mesh/material/textures',
            'Scene/contacts/animation/camera',
            'Direct wetness paint, scripted State removal, new phenomenon-specific code',
        ],
        gates=[
            dict(
                id='G0',
                criterion='Runtime/Vulkan, finite/nonnegative State and input/settings provenance',
                evidence_status='not_run_for_this_new_input',
            ),
            dict(
                id='G1',
                criterion='At t=6, B total Ice / B initial Ice is in [0.05, 0.80]; '
                'Ice is visually recognizable at t=1',
                evidence_status='not_measured',
            ),
            dict(
                id='G2',
                criterion='By t=8, waterfilm coverage >=0.10 outside B initial Ice coverage '
                '>=0.10 footprint occupies >=1% of B reference upper-face area',
                evidence_status='requires_time_matched_state_and_geometry_samples',
            ),
            dict(
                id='G3',
                criterion='At t=12, 14 and 16, the same texels outside the B initial Ice '
                'coverage >=0.10 footprint occupy >=1% of B reference '
                'upper-face area, have waterfilm <=0.05, Ice <=0.05, '
                'absorbedwetness >=0.25, and had waterfilm >=0.10 at an earlier '
                'recorded checkpoint. Their lit trace is visible at normal viewing scale.',
                evidence_status='requires_temporal_samples_and_visual_review',
            ),
            dict(
                id='G4',
                criterion='Fresh-process reload preserves final profile/rules and passes G0-G3 '
                'again; report actual differences, do not demand bitwise pixels',
                evidence_status='not_run',
            ),
        ],
        metric_definition='coverage = clamp(amount / (profileCapacity * WorldTexelArea * 256^2),0,1). '
        'Use fixed macro reference-area weights and top-face identity from geometry, '
        'not render vertex IDs, pixel counts or presumed surface ID.',
        threshold_status='Task design choices, not physical constants, literature standards or measured results; '
        'freeze for this pilot and version any revisions, retaining failures.',
        visual_gate='An assessor marks the normal-size fixed-camera lit trace without heatmap/difference '
        'amplification; location must overlap the numerical G3 region. AI self-review is provisional '
        'and cannot substitute for an independent human quality assessment.',
        limitations=[
            'No independently approved artistic reference',
            'Per-checkpoint same-step State/geometry captures and top-face area masks not yet prepared',
            'A checkpoint visit mask is a lower bound on visited area, not a per-step provenance proof',
            'No per-step full material ledger for this new rendered task',
            'No human session, comparison intervention or new-mesh generalization',
        ],
        stopping_rule='At most six recorded profile candidates, then one fresh-process replay of a successful '
        'submission. Stop on the first complete provisional pass; retain failed candidates. '
        'If none pass, report goal unmet within the declared budget, not impossible in principle.',
        model_claim='Successful output establishes feasibility of this task only, not novelty or relative ease.',
        preparation_checks=checks,
        input_sha256={
            str(p.relative_to(ROOT)): digest(p)
            for p in [
                SOURCE_SCENE,
                SOURCE_ANIMATION,
                SOURCE_PROFILE,
                SOURCE_MAP,
                pathlib.Path(__file__).resolve(),
            ]
        },
        prepared_artifact_sha256={str(p.relative_to(ROOT)): digest(p) for p in artifacts},
        human_sessions=0,
        application_runs=0,
        measured_results=None,
    )
    write(OUT / 'plan.json', plan)
    write(
        OUT / 'preparation.json',
        dict(
            created_at=stamp(),
            status='static_input_checks_passed',
            preparation_checks=checks,
            prepared_files=len(artifacts),
            application_runs=0,
            product_source_changed=False,
            note='JSON/reference checks only, not actual SceneLoader/GPU validation',
        ),
    )
    print('Task specification and 8 isolated input files prepared; application runs: 0')


# endregion

# ============================================================
# 02. Script Execution
# ============================================================
# region 02_Script_Execution

if __name__ == '__main__':
    prepare()

# endregion
