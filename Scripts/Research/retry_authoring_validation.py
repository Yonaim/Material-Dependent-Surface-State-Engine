"""Retry unchanged binaries after the user's explicit report of a policy change."""

import copy, os, pathlib, shutil, subprocess, sys
from authoring_study import ROOT, STUDY, read, write, digest, stamp
import authoring_render_validation as render

OUT = STUDY / 'policy-change-retry'


def main():
    if OUT.exists():
        raise RuntimeError('Refusing to overwrite retry evidence')
    old_geometry = read(STUDY / 'geometry-validation/after.json')
    old_render = read(STUDY / 'render-validation/manifest.json')
    test = pathlib.Path(old_geometry['command'][0])
    if (
        digest(test) != old_geometry['executable_sha256']
        or digest(render.EXE) != old_render['executable_sha256']
    ):
        raise RuntimeError('Retry requires the same previously blocked binaries')
    OUT.mkdir(parents=True)
    authorization = dict(
        recorded_at=stamp(),
        source='Direct user message in current chat',
        instruction='스마트앱컨트롤 이제 해제했으니까 다시 시도해봐',
        scope='Retry existing shared-geometry test and six reference rendered validations',
        settings_changed_by_agent=False,
        previous_evidence_preserved=True,
        executable_sha256=[digest(test), digest(render.EXE)],
    )
    write(OUT / 'retry-authorization.json', authorization)
    archive = OUT / 'raw/geometry/inputs'
    files = {test, pathlib.Path(__file__).resolve(), ROOT / 'Tests/SharedSurfaceGeometryTests.cpp'}
    files.update(
        ROOT / p
        for p in old_geometry['source_hashes']
        if p.startswith('Source') or p.startswith('Tests')
    )
    hashes = {str(p.relative_to(ROOT)): digest(p) for p in files}
    for name, expected in hashes.items():
        dest = archive / name
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2('\\\\?\\' + str((ROOT / name).resolve()), '\\\\?\\' + str(dest.resolve()))
        if digest(dest) != expected:
            raise RuntimeError('Retry archive mismatch')
    record = dict(
        started_at=stamp(),
        command=[str(test)],
        executable_sha256=digest(test),
        source_hashes=hashes,
        retry_authorization='retry-authorization.json',
    )
    try:
        with (OUT / 'raw/geometry/test.log').open('w', encoding='utf-8') as log:
            result = subprocess.run(
                [str(test)], cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, timeout=60
            )
        record.update(
            status='passed' if result.returncode == 0 else 'failed', exit_code=result.returncode
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        record.update(status='execution_failed', error=str(exc))
    record['finished_at'] = stamp()
    write(OUT / 'geometry.json', record)
    print('SharedSurfaceGeometry:', record['status'], flush=True)
    if record['status'] != 'passed':
        raise SystemExit(1)

    run_render()


def run_render(correct_paths=False):
    if read(OUT / 'geometry.json')['status'] != 'passed':
        raise RuntimeError('Geometry retry did not pass')
    plan = copy.deepcopy(read(STUDY / 'render-validation/plan.json'))
    plan.update(
        created_at=stamp(),
        retry_of=str(STUDY / 'render-validation/plan.json'),
        original_plan_sha256=digest(STUDY / 'render-validation/plan.json'),
        retry_authorization=str(OUT / 'retry-authorization.json'),
        protocol_change='Output location only; same profiles, scenes, settings, frame counts and executable',
    )
    new_render = OUT / ('render-v2' if correct_paths else 'render')
    if new_render.exists():
        raise RuntimeError('Refusing to overwrite render retry')
    for item in plan['runs']:
        folder = new_render / 'raw' / item['id'] / 'input'
        shutil.copytree(pathlib.Path(item['scene']).parent, folder)
        if correct_paths:
            scene = read(folder / 'Task.Scene')
            scene['animation'] = os.path.relpath(scene['animation'], folder).replace('\\', '/')
            for obj in scene['objects']:
                obj['mesh'] = os.path.relpath(obj['mesh'], folder).replace('\\', '/')
            write(folder / 'Task.Scene', scene)
        item.update(
            scene=str(folder / 'Task.Scene'),
            settings=str(folder / 'settings.json'),
            output=str(folder.parent / 'capture.jsonl'),
        )
    if correct_paths:
        plan['protocol_change'] = (
            'Rebase mesh and animation paths from absolute to required relative paths. Same resolved assets, profiles, settings, frame counts and executable; previous loader failure preserved.'
        )
    write(new_render / 'plan.json', plan)
    render.OUT = new_render
    render.run()


if __name__ == '__main__':
    if sys.argv[1:] == ['render']:
        run_render()
    elif sys.argv[1:] == ['render-v2']:
        run_render(correct_paths=True)
    else:
        main()
