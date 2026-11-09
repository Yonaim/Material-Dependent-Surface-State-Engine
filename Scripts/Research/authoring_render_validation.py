"""Render isolated reference fixtures; this is not a timed human authoring study."""

# ============================================================
# 00. Imports and Module Configuration
# ============================================================
# region 00_Imports_and_Module_Configuration

import copy, os, pathlib, shutil, subprocess, sys
from authoring_study import ROOT, STUDY, PACK, read, write, stamp, digest

EXE = ROOT / 'Build/bin/Release/MDSS.exe'
OUT = STUDY / 'render-validation'


# endregion

# ============================================================
# 01. Processing Helpers
# ============================================================
# region 01_Processing_Helpers


def prepare():
    if (OUT / 'plan.json').exists():
        return read(OUT / 'plan.json')
    blocked = ROOT / 'Build/OptimizationLab/blocked-executables.json'
    if blocked.exists() and digest(EXE) in read(blocked):
        raise RuntimeError('Executable previously blocked; no workaround attempted')
    settings = dict(
        capture_image=True,
        presentation_mode=True,
        replay_demo=True,
        render_mesh_resolution=128,
        geometry_feedback=True,
        raw_flux_cache=True,
        fp16_raw_flux=False,
        fp16_weights=False,
        sparse_solver=False,
        sparse_height=False,
        sparse_geometry=False,
        active_channel_mask=False,
        per_workgroup_channel_mask=False,
        adaptive_geometry=False,
    )
    runs = []
    for name, task, profile in [
        ('base', 'P1', 'base'),
        ('threshold', 'P1', 'threshold'),
        ('wetness', 'P2', 'wetness'),
        ('absorbedwetness', 'P2', 'absorbedwetness'),
        ('reuse20', 'P3', 'absorbedwetness'),
        ('absorbedwetness-repeat', 'P2', 'absorbedwetness'),
    ]:
        folder = OUT / 'raw' / name / 'input'
        folder.mkdir(parents=True, exist_ok=False)
        scene = read(PACK / task / 'start/Task.Scene')
        distribution = read(PACK / task / 'start/Editable.SurfaceProfileMap')
        write(folder / 'Task.Scene', scene)
        write(folder / 'Editable.SurfaceProfileMap', distribution)
        write(folder / 'Editable.SRProfile', read(PACK / 'reference' / f'{profile}.SRProfile'))
        write(folder / 'settings.json', settings)
        names = sorted(read(folder / 'Editable.SRProfile')['states'])
        runs.append(
            dict(
                id=name,
                task=task,
                reference_profile=profile,
                channels=names,
                scene=str(folder / 'Task.Scene'),
                settings=str(folder / 'settings.json'),
                warmup_frames=359,
                measurement_frames=1,
                output=str(folder.parent / 'capture.jsonl'),
            )
        )
    plan = dict(
        created_at=stamp(),
        scope='Rendered feasibility of reference submissions, not human task completion or productivity',
        known_results='Five profile/binding probes and prior PERF studies known; no blind claim',
        runs=runs,
        fixed_dt_seconds=1 / 60,
        screenshot_frame_index=359,
        state_capture_scope='After two timestamp-drain frames, not exactly the screenshot step',
        comparisons=[
            'base vs threshold: persisted profile variation',
            'wetness vs absorbedwetness: builtin appearance binding, with named-state comparison',
            'absorbedwetness vs repeat: fresh-process repeatability',
            'reuse20: same profile, rotated same mesh, no held-out claim',
        ],
        gates=[
            'Exit zero and no error log',
            'One frame sample and valid PPM',
            'Nonnegative finite final state, no reported reaction budget/arithmetic failures',
            'Record actual settings and simulation serials; do not infer numerical budget from final-state sums',
        ],
        quality_gate='Independent artist target and judged quality remain undefined; visual feasibility only',
    )
    write(OUT / 'plan.json', plan)
    return plan


def run():
    plan = prepare()
    if (OUT / 'manifest.json').exists():
        raise RuntimeError('Refusing to overwrite execution record')
    files = {
        EXE,
        pathlib.Path(__file__).resolve(),
        ROOT / 'Scripts/Research/authoring_study.py',
        ROOT / 'CMakeLists.txt',
        OUT / 'plan.json',
    }
    for directory in [
        'Source',
        'Shaders',
        'Build/bin/Shaders/Release',
        'Assets/Meshes/MarbleCube',
        'Assets/Meshes/ReactionLab',
    ]:
        files.update(p for p in (ROOT / directory).rglob('*') if p.is_file())
    files.update(p for p in (ROOT / 'Config').glob('*') if p.is_file())
    files.add(ROOT / 'Assets/Animations/IceReactionLab.DemoAnim')
    files.update(p for p in (OUT / 'raw').rglob('*') if p.is_file())
    hashes = {str(p.relative_to(ROOT)): digest(p) for p in sorted(files)}
    archive = OUT / 'raw/inputs'
    for name, expected in hashes.items():
        target = archive / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2('\\\\?\\' + str((ROOT / name).resolve()), '\\\\?\\' + str(target.resolve()))
        if digest(target) != expected:
            raise RuntimeError('Input archive mismatch')
    manifest = dict(
        started_at=stamp(),
        executable_sha256=digest(EXE),
        source_hashes=hashes,
        shader_files=sum(p.endswith('.spv') for p in hashes),
        runs=[],
        scope=plan['scope'],
    )
    write(OUT / 'manifest.json', manifest)
    layout = ROOT / 'Config/EditorLayout.ini'
    original_layout = layout.read_bytes() if layout.exists() else None
    try:
        for item in plan['runs']:
            output = pathlib.Path(item['output'])
            command = [
                str(EXE),
                '--benchmark-scene',
                item['scene'],
                '--benchmark-resolution',
                '128',
                '--benchmark-warmup-frames',
                str(item['warmup_frames']),
                '--benchmark-measure-frames',
                '1',
                '--benchmark-output',
                str(output),
            ]
            record = dict(id=item['id'], started_at=stamp(), command=command, status='running')
            try:
                with output.with_suffix('.log').open('w', encoding='utf-8') as log:
                    result = subprocess.run(
                        command,
                        cwd=ROOT,
                        env=dict(os.environ, MDSS_PERF_SETTINGS=item['settings']),
                        stdout=log,
                        stderr=subprocess.STDOUT,
                        timeout=240,
                    )
                record.update(
                    exit_code=result.returncode,
                    status='completed' if result.returncode == 0 else 'failed',
                )
                if result.returncode == 0:
                    required = [
                        pathlib.Path(str(output) + suffix)
                        for suffix in ['.ppm', '.settings.json', '.validation.json']
                    ]
                    missing = [str(p) for p in required if not p.exists()]
                    lines = (
                        output.read_text(encoding='utf-8').splitlines() if output.exists() else []
                    )
                    if missing or len(lines) != item['measurement_frames']:
                        record.update(
                            status='capture_incomplete',
                            missing_outputs=missing,
                            frame_samples=len(lines),
                        )
            except (OSError, subprocess.TimeoutExpired) as exc:
                record.update(status='failed', error=str(exc))
                if isinstance(exc, OSError) and getattr(exc, 'winerror', None) == 4551:
                    record['status'] = 'blocked_before_execution'
                    registry = ROOT / 'Build/OptimizationLab/blocked-executables.json'
                    blocked = read(registry) if registry.exists() else {}
                    blocked[digest(EXE)] = dict(
                        path=str(EXE),
                        winerror=4551,
                        status='blocked_before_execution',
                        recorded_at=stamp(),
                        action='Do not relaunch, rebuild, rename or repackage as a workaround',
                        evidence=str(OUT / 'manifest.json'),
                    )
                    write(registry, blocked)
            record['finished_at'] = stamp()
            manifest['runs'].append(record)
            write(OUT / 'manifest.json', manifest)
            print(item['id'], record['status'], flush=True)
            if record['status'] != 'completed':
                break
    finally:
        if original_layout is not None:
            layout.write_bytes(original_layout)
        elif layout.exists():
            layout.unlink()
        manifest['finished_at'] = stamp()
        manifest['status'] = (
            'completed'
            if len(manifest['runs']) == len(plan['runs'])
            and all(r['status'] == 'completed' for r in manifest['runs'])
            else 'incomplete_or_failed'
        )
        manifest['editor_layout_restored'] = (
            layout.read_bytes() if layout.exists() else None
        ) == original_layout
        write(OUT / 'manifest.json', manifest)
    if manifest['status'] != 'completed':
        raise SystemExit(1)


# endregion

# ============================================================
# 02. Script Execution
# ============================================================
# region 02_Script_Execution

if __name__ == '__main__':
    if len(sys.argv) != 2 or sys.argv[1] not in ['prepare', 'run']:
        raise SystemExit('Usage: authoring_render_validation.py prepare|run')
    if sys.argv[1] == 'prepare':
        print('Prepared', len(prepare()['runs']), 'reference render validations')
    else:
        run()

# endregion
