"""Archive baseline/fix and validate descriptor rebinding without changing numeric settings."""

import copy, os, pathlib, shutil, subprocess, sys
from authoring_study import ROOT, STUDY, read, write, digest, stamp
import authoring_render_validation as render

OUT = STUDY / 'layout-lifetime-fix'


def archive(folder, extra):
    files = set(extra)
    for directory in [
        'Source',
        'Shaders',
        'Build/bin/Shaders/Release',
        'Assets/Meshes/MarbleCube',
        'Assets/Meshes/ReactionLab',
    ]:
        files.update(p for p in (ROOT / directory).rglob('*') if p.is_file())
    files.update(
        [
            ROOT / 'CMakeLists.txt',
            ROOT / 'Tests/SceneResourceTests.cpp',
            pathlib.Path(__file__).resolve(),
        ]
    )
    hashes = {str(p.relative_to(ROOT)): digest(p) for p in files}
    for name, expected in hashes.items():
        dest = folder / name
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2('\\\\?\\' + str((ROOT / name).resolve()), '\\\\?\\' + str(dest.resolve()))
        if digest(dest) != expected:
            raise RuntimeError('Archive mismatch')
    return hashes


def test(stage):
    folder = OUT / 'raw' / stage
    if folder.exists():
        raise RuntimeError('Preserving existing test evidence')
    exe = ROOT / 'Build/Release/MDSS_SceneResourceTests.exe'
    hashes = archive(folder / 'inputs', [exe])
    record = dict(
        started_at=stamp(),
        stage=stage,
        command=[str(exe), '--scratch-layout-only'],
        executable_sha256=digest(exe),
        source_hashes=hashes,
        scope='12 scratch format switches, stable layout/sets, State preservation, GPU solver/render and Vulkan validation',
    )
    with (folder / 'test.log').open('w', encoding='utf-8') as log:
        p = subprocess.run(
            record['command'], cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, timeout=120
        )
    lines = (folder / 'test.log').read_text(encoding='utf-8')
    record.update(
        finished_at=stamp(),
        exit_code=p.returncode,
        status='passed' if p.returncode == 0 else 'failed',
        vulkan_error_lines=sum(
            '[ERROR] [Vulkan Validation]' in line for line in lines.splitlines()
        ),
    )
    write(OUT / (stage + '.json'), record)
    print(
        stage,
        record['status'],
        'exit',
        p.returncode,
        'Vulkan error lines',
        record['vulkan_error_lines'],
    )
    if stage != 'before' and p.returncode:
        raise SystemExit(1)


def rendered():
    if read(OUT / 'after.json')['status'] != 'passed':
        raise RuntimeError('Targeted regression must pass before scene reruns')
    dest = OUT / 'render'
    if dest.exists():
        raise RuntimeError('Preserving existing render evidence')
    original = read(STUDY / 'policy-change-retry/capture-selection.json')
    plan = copy.deepcopy(read(STUDY / 'policy-change-retry/render-v2/plan.json'))
    plan.update(
        created_at=stamp(),
        runs=[],
        protocol_change='Descriptor lifetime correctness fix only; same scenes, profiles, animation, numeric shaders, fixed step and FP32 Dense settings',
        baseline_outputs='policy-change-retry/capture-selection.json',
        scope='Post-fix rendered correctness validation, not performance or human authoring study',
    )
    for old in original['selected_runs']:
        case = copy.deepcopy(old)
        folder = dest / 'raw' / case['id'] / 'input'
        source = pathlib.Path(old['scene']).parent
        shutil.copytree(source, folder)
        scene = read(folder / 'Task.Scene')
        scene['animation'] = os.path.relpath(
            (source / scene['animation']).resolve(), folder
        ).replace('\\', '/')
        for obj in scene['objects']:
            obj['mesh'] = os.path.relpath((source / obj['mesh']).resolve(), folder).replace(
                '\\', '/'
            )
        write(folder / 'Task.Scene', scene)
        case.update(
            scene=str(folder / 'Task.Scene'),
            settings=str(folder / 'settings.json'),
            output=str(folder.parent / 'capture.jsonl'),
        )
        plan['runs'].append(case)
    write(dest / 'plan.json', plan)
    render.OUT = dest
    render.run()


def gpu():
    folder = OUT / 'raw/gpu'
    if folder.exists():
        raise RuntimeError('Preserving GPU regression evidence')
    exe = ROOT / 'Build/Release/MDSS_SurfaceGPUResourceTests.exe'
    hashes = archive(folder / 'inputs', [exe, ROOT / 'Tests/SurfaceGPUResourceTests.cpp'])
    record = dict(
        started_at=stamp(),
        command=[str(exe)],
        source_hashes=hashes,
        executable_sha256=digest(exe),
        scope='Existing complete SurfaceGPUResource test executable, not complete repository CTest suite',
    )
    with (folder / 'test.log').open('w', encoding='utf-8') as log:
        p = subprocess.run([str(exe)], cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, timeout=180)
    record.update(
        finished_at=stamp(),
        exit_code=p.returncode,
        status='passed' if p.returncode == 0 else 'failed',
    )
    write(OUT / 'gpu.json', record)
    print('SurfaceGPUResource', record['status'], 'exit', p.returncode)
    if p.returncode:
        raise SystemExit(1)


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit('Usage: layout_lifetime_validation.py before|after|render|gpu')
    if sys.argv[1] in ['before', 'after']:
        test(sys.argv[1])
    elif sys.argv[1] == 'render':
        rendered()
    elif sys.argv[1] == 'gpu':
        gpu()
    else:
        raise SystemExit('Unknown stage')
