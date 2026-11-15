"""저작 가능성 검토의 Profile 사례를 probe로 실행하고 결과를 기록한다.

Prepare isolated authoring tasks and verify capability boundaries; never fabricate human data.
"""

# ============================================================
# 00. Imports and Module Configuration
# ============================================================
# region 00_Imports_and_Module_Configuration
# 실행에 필요한 모듈과 입력·결과 경로를 한곳에서 구성한다.

import argparse, copy, datetime as dt, hashlib, json, os, pathlib, shutil, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
STUDY = ROOT / 'Performance-Data/20261027-authoring-readiness'
PACK = STUDY / 'raw/task-pack'
EXE = ROOT / 'Build/Release/MDSS_AuthoringProbe.exe'
# endregion

# ============================================================
# 01. Processing Helpers
# ============================================================
# region 01_Processing_Helpers
# 저작 가능성 검토의 Profile 사례를 probe로 실행하고 결과를 기록한다.


def stamp():
    return dt.datetime.now(dt.timezone(dt.timedelta(hours=9))).isoformat()


def read(p):
    return json.loads(p.read_text(encoding='utf-8'))


def write(p, x):
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(x, indent=2, ensure_ascii=False), encoding='utf-8')


def digest(p):
    path = str(p.resolve())
    if os.name == 'nt' and not path.startswith('\\\\?\\'):
        path = '\\\\?\\' + path
    with open(path, 'rb') as f:
        return hashlib.sha256(f.read()).hexdigest()


def prepare():
    if (STUDY / 'plan.json').exists():
        return read(STUDY / 'plan.json')
    base = read(ROOT / 'Assets/SurfaceProfiles/IceReactionLab.SRProfile')
    spec = read(ROOT / 'Scripts/Research/Plans/modular-baseline-v1.json')
    t2 = next(t for t in spec['tasks'] if t['id'] == 'T2')['profile_changes']
    wet = copy.deepcopy(t2['add_states']['wetness'])
    rule = copy.deepcopy(t2['append_reactions'][0])
    profiles = {
        'base': copy.deepcopy(base),
        'wetness': copy.deepcopy(base),
        'absorbedwetness': copy.deepcopy(base),
        'residue': copy.deepcopy(base),
    }
    for key in ['wetness', 'absorbedwetness', 'residue']:
        profiles[key]['states'][key] = copy.deepcopy(wet)
        r = copy.deepcopy(rule)
        r['produces'] = {key: 1}
        profiles[key]['reactions'].append(r)
    profiles['threshold'] = copy.deepcopy(base)
    next(r for r in profiles['threshold']['reactions'] if r['id'] == 'ice_melting')['modifiers'][0][
        'threshold'
    ] = 0.45
    cases = []
    for name, p in profiles.items():
        path = PACK / 'reference' / f'{name}.SRProfile'
        write(path, p)
        state = name if name in ['wetness', 'absorbedwetness', 'residue'] else 'waterfilm'
        cases.append(
            dict(
                id=name,
                profile=str(path),
                state=state,
                expected_builtin_absorbed_wetness_binding=name == 'absorbedwetness',
            )
        )
    write(STUDY / 'probe-cases.json', cases)

    # Clone the existing rendered scene into an isolated folder; no source assets or live settings change.
    source = ROOT / 'Assets/Scenes/IceReactionLab.Scene'
    scene = read(source)
    scene['animation'] = str((source.parent / scene['animation']).resolve())
    tasks = []
    briefs = [
        (
            'P1',
            '조정값 저장과 재로드',
            '같은 장면의 차가운 판·가열 판·냉각 판을 유지하며 융해 시작 조건을 조정한다. 저장한 파일만으로 앱 재시작 후 같은 설정이 복원돼야 한다. 화면만 바꾸거나 runtime override만 남긴 제출은 완료가 아니다.',
            'Exact target clip/threshold window must be independently chosen before timed collection; current fixture demonstrates persistence workflow only.',
        ),
        (
            'P2',
            '흐른 물의 흔적 추가',
            '새로 입력되거나 이웃에서 들어온 물 중 일부가 반사성 물막과 다른 젖은 자국으로 남게 한다. 정지해 있던 물 전체를 매 Step 다시 전환하지 않고, Waterfilm과 잔류량의 수지를 유지한다. 재시작해도 규칙과 외관 연결이 남아야 한다.',
            'Require functional inflow contract, visible residue and restart persistence; time-to-quality gate pending independent render inspection.',
        ),
        (
            'P3',
            '조합과 변환 재사용',
            'P2의 규칙을 재사용해 같은 판의 기울기가 다른 배치에 적용한다. 특수 Shader나 반응 분기를 추가했다면 모두 기록한다. 원래 장면도 다시 확인한다. 이 과제는 새 Mesh 일반화 시험이 아니다.',
            'Reuse plus transform regression only; scene and tasks known to implementer, not held-out or blind.',
        ),
    ]
    for id, title, brief, limit in briefs:
        folder = PACK / id / 'start'
        folder.mkdir(parents=True, exist_ok=True)
        write(folder / 'Editable.SRProfile', base)
        distribution = read(ROOT / 'Assets/SurfaceProfileMaps/IceReactionLab.SurfaceProfileMap')
        distribution['profiles'] = ['Editable.SRProfile']
        write(folder / 'Editable.SurfaceProfileMap', distribution)
        s = copy.deepcopy(scene)
        for obj in s['objects']:
            obj['mesh'] = str((source.parent / obj['mesh']).resolve())
            if 'surfaceProfileMap' in obj:
                obj['surfaceProfileMap'] = 'Editable.SurfaceProfileMap'
            if id == 'P3' and 'surfaceProfileMap' in obj:
                obj['transform']['rotationDegrees'][0] = 20
        s['animation'] = os.path.relpath(s['animation'], folder).replace('\\', '/')
        for obj in s['objects']:
            obj['mesh'] = os.path.relpath(obj['mesh'], folder).replace('\\', '/')
        write(folder / 'Task.Scene', s)
        (folder.parent / 'brief.md').write_text(
            f'# {id} — {title}\n\n{brief}\n\n이 과제 팩은 단일 시스템 pilot용입니다. 아직 사람의 수행 시간·성공률·외관 품질은 측정하지 않았습니다.\n',
            encoding='utf-8',
        )
        tasks.append(
            dict(
                id=id,
                title=title,
                brief=brief,
                scene=str((folder / 'Task.Scene').relative_to(ROOT)),
                start_profile=str((folder / 'Editable.SRProfile').relative_to(ROOT)),
                quality_gate_status='not_validated_by_independent_rendered_evaluation',
                limitation=limit,
            )
        )
    plan = dict(
        schema_version=1,
        created_at=stamp(),
        status='capability_probe_prepared_before_execution',
        scope='Authoring readiness and a single-system task pack; not a comparative human study or performance experiment',
        source_commit=subprocess.check_output(
            ['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True
        ).strip(),
        tasks=tasks,
        probe_cases=cases,
        comparative_go_conditions=[
            'Distinct model/authoring intervention defined',
            'Both backends usable through matched complete workflows',
            'Independent visible outcome/functional gates validated',
            'Real participants and actual event logs available',
        ],
        no_go='Same declarations and same services under U/M selector alone are not an identifiable authoring intervention; no human productivity inference',
        prior_knowledge='PERF-041/042/043 and task outcomes known; no blind or held-out claims',
    )
    write(STUDY / 'plan.json', plan)
    write(
        STUDY / 'session-template.json',
        dict(
            status='unexecuted_template',
            participant_id=None,
            backend=None,
            task_id=None,
            started_at=None,
            finished_at=None,
            active_seconds=None,
            wall_seconds=None,
            time_limit_seconds=None,
            completion=None,
            quality_gate=None,
            ai_policy=None,
            ai_assistance_used=None,
            prior_experience=None,
            interventions=[],
            artifacts=[],
            notes=[],
        ),
    )
    return plan


def probe():
    plan = prepare()
    out = STUDY / 'raw/probe'
    if (out / 'results.json').exists():
        raise RuntimeError('Refusing to overwrite probe evidence')
    out.mkdir(parents=True, exist_ok=True)
    required = [
        EXE,
        ROOT / 'Scripts/Research/AuthoringProbe.cpp',
        ROOT / 'Scripts/Research/authoring_study.py',
        ROOT / 'CMakeLists.txt',
        ROOT / 'Source/Rendering/Surface/DemoSurfaceEffects.h',
        ROOT / 'Source/DebugUI/Panels/DebugUIContactProfile.cpp',
        ROOT / 'Shaders/Rendering/Surface/Lighting/SurfaceLit.glsl',
        ROOT / 'Shaders/Rendering/Surface/Lighting/Effects/AbsorbedWetness.glsl',
        ROOT / 'Scripts/Research/ModularBaseline.h',
        ROOT / 'Scripts/Research/Plans/modular-baseline-v1.json',
    ]
    for base in ['Source/AssetManager', 'Source/SurfaceState/Types']:
        required.extend(p for p in (ROOT / base).rglob('*') if p.is_file())
    required.extend(p for p in PACK.rglob('*') if p.is_file())
    required.extend([STUDY / 'plan.json', STUDY / 'probe-cases.json'])
    hashes = {str(p.relative_to(ROOT)): digest(p) for p in set(required)}
    archive = out / 'inputs'
    for name, h in hashes.items():
        target = archive / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2('\\\\?\\' + str((ROOT / name).resolve()), '\\\\?\\' + str(target.resolve()))
        if digest(target) != h:
            raise RuntimeError('Archive mismatch')
    manifest = dict(
        started_at=stamp(), executable_sha256=digest(EXE), source_hashes=hashes, scope=plan['scope']
    )
    with (out / 'probe.log').open('w', encoding='utf-8') as log:
        r = subprocess.run(
            [str(EXE), str(STUDY / 'probe-cases.json'), str(out / 'results.json')],
            cwd=ROOT,
            stdout=log,
            stderr=subprocess.STDOUT,
        )
    manifest.update(
        finished_at=stamp(),
        exit_code=r.returncode,
        status='completed' if r.returncode == 0 else 'failed',
    )
    write(STUDY / 'manifest.json', manifest)
    if r.returncode:
        raise RuntimeError((out / 'probe.log').read_text(encoding='utf-8'))
    results = read(out / 'results.json')
    scene_errors = []
    for task in plan['tasks']:
        path = ROOT / task['scene']
        s = read(path)
        if not (path.parent / s['animation']).exists():
            scene_errors.append(str(path) + ' animation')
        for obj in s['objects']:
            if not (path.parent / obj['mesh']).exists():
                scene_errors.append(str(path) + ' mesh')
            if 'surfaceProfileMap' in obj:
                d = path.parent / obj['surfaceProfileMap']
                if not d.exists() or not all((d.parent / p).exists() for p in read(d)['profiles']):
                    scene_errors.append(str(path) + ' profile path')
    output = dict(
        created_at=stamp(),
        status='not_ready_for_comparative_authoring_study',
        profile_probe_cases=len(results),
        profile_and_binding_checks_passed=all(
            x['profile_loaded']
            and x['registered']
            and x['builtin_absorbed_wetness_binding']
            == x['expected_builtin_absorbed_wetness_binding']
            for x in results
        ),
        scene_reference_errors=scene_errors,
        results=results,
        prepared_tasks=len(plan['tasks']),
        human_sessions=0,
        human_completion_times=None,
        comparative_quality_results=None,
        human_study_not_run_reason='M is a headless coordinator with the same profile declaration workflow; distinct model intervention and matched UI not available. Rendered acceptance predicates and real participant data are absent.',
        rendered_scene_execution='not_run',
        rendered_quality='not_verified',
        single_system_pilot='Isolated editable scene/profile task pack prepared; real author must perform, restart and submit artifacts before any completion result',
        finding='Runtime scalar registration is broader than builtin demo appearance; wetness and absorbedwetness are distinct names, not aliases',
    )
    write(STUDY / 'readiness.json', output)
    if scene_errors:
        raise RuntimeError('Broken scene references')
    print(
        json.dumps(
            dict(
                status=output['status'],
                cases=len(results),
                binding_checks='passed',
                scene_reference_errors=scene_errors,
                human_sessions=0,
            ),
            indent=2,
        )
    )


# endregion

# ============================================================
# 02. Script Execution
# ============================================================
# region 02_Script_Execution
# 저작 가능성 검토의 Profile 사례를 probe로 실행하고 결과를 기록한다.

if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('stage', choices=['prepare', 'probe'])
    a = ap.parse_args()
    if a.stage == 'prepare':
        print('Prepared', len(prepare()['tasks']), 'authoring task fixtures')
    else:
        probe()

# endregion
