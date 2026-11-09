"""Preserve build stages and execute paired, serial optimization measurements.

Every run uses the stage's saved executable and SPIR-V, not whichever shaders were built last.
"""

from __future__ import annotations
import argparse, datetime as dt, hashlib, json, os, random, shutil, subprocess, sys, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LAB = ROOT / 'Build/OptimizationLab'
RESULT = ROOT / 'Performance-Data/20261025-optimization-round'
SHADERS = ROOT / 'Build/bin/Shaders/Release'


def save(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf8')


def snapshot(tag):
    target = LAB / tag
    target.mkdir(parents=True, exist_ok=False)
    shutil.copy2(ROOT / 'Build/bin/Release/MDSS.exe', target / 'MDSS.exe')
    shutil.copytree(SHADERS, target / 'Shaders')
    diff = subprocess.check_output(['git', 'diff', '--binary', 'HEAD'], cwd=ROOT)
    (target / 'changes.patch').write_bytes(diff)
    with zipfile.ZipFile(target / 'inputs.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
        for folder in ['Source', 'Shaders', 'Assets', 'CMake', 'Config', 'Tests', 'Scripts']:
            for path in (ROOT / folder).rglob('*'):
                if path.is_file():
                    archive.write(path, path.relative_to(ROOT))
        archive.write(ROOT / 'CMakeLists.txt', 'CMakeLists.txt')
    files = [p for p in target.rglob('*') if p.is_file()]
    save(
        target / 'snapshot.json',
        {
            'created_at': dt.datetime.now().astimezone().isoformat(),
            'build_configuration': 'Release',
            'vulkan_validation': False,
            'commit': subprocess.check_output(
                ['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True
            ).strip(),
            'files': {
                str(p.relative_to(target)): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in files
            },
        },
    )
    print(target, flush=True)


def plan(name, before, after, cases, settings_before=None, settings_after=None):
    runs = []
    for repeat in range(1, 4):
        ordered = list(cases)
        random.Random(42017 + repeat).shuffle(ordered)
        for scene, resolution in ordered:
            arms = [
                ('before', before, settings_before or {}),
                ('after', after, settings_after or {}),
            ]
            if repeat % 2 == 0:
                arms.reverse()
            for arm, tag, settings in arms:
                runs.append(
                    {
                        'id': f'{name}-{Path(scene).stem}-{resolution}-{arm}-{repeat}',
                        'experiment': name,
                        'arm': arm,
                        'repeat': repeat,
                        'scene': scene,
                        'resolution': resolution,
                        'binary': str(LAB / tag / 'MDSS.exe'),
                        'shader_archive': str(LAB / tag / 'Shaders'),
                        'settings': settings,
                        'warmup': 60,
                        'measure': 600,
                        'timeout': 1800,
                    }
                )
    path = LAB / (name + '-plan.json')
    save(path, {'name': name, 'runs': runs})
    return path


def run_plan(path):
    result = RESULT / path.stem.removesuffix('-plan')
    command = [
        sys.executable,
        str(ROOT / 'Scripts/Performance/run_suite.py'),
        '--plan',
        str(path),
        '--result',
        str(result),
    ]
    if (result / 'manifest.json').exists():
        command.append('--resume')
    return subprocess.call(command, cwd=ROOT)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=['snapshot', 'pair', 'run'])
    parser.add_argument('name')
    parser.add_argument('--before')
    parser.add_argument('--after')
    parser.add_argument('--wide', action='store_true')
    parser.add_argument('--before-settings', default='{}')
    parser.add_argument('--after-settings', default='{}')
    args = parser.parse_args()
    if args.command == 'snapshot':
        snapshot(args.name)
    elif args.command == 'run':
        raise SystemExit(run_plan(LAB / (args.name + '-plan.json')))
    else:
        cases = [
            (f'Assets/Scenes/{scene}.Scene', resolution)
            for scene in ['BrickCube', 'Mountain']
            for resolution in [256, 512]
        ]
        if args.wide:
            cases += [(str(LAB / 'BrickCube-Wide.Scene'), r) for r in [256, 512]]
        p = plan(
            args.name,
            args.before,
            args.after,
            cases,
            json.loads(args.before_settings),
            json.loads(args.after_settings),
        )
        print(p)
