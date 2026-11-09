"""Audit recorded capability evidence and preserve blocked/unexecuted outcomes."""
from authoring_study import ROOT, STUDY, PACK, read, write, digest, stamp
import pathlib, shutil

probe = read(STUDY/'readiness.json')
manifest = read(STUDY/'manifest.json')
renders = read(STUDY/'render-validation/manifest.json')
render_plan = read(STUDY/'render-validation/plan.json')
errors = []
archives = []
for name, entry, folder in [('profile_probe',manifest,STUDY/'raw/probe/inputs'),
        ('render_attempt',renders,STUDY/'render-validation/raw/inputs')]:
    missing, mismatch = [], []
    for source, expected in entry['source_hashes'].items():
        archived = folder/source
        if not archived.exists():
            missing.append(source)
        elif digest(archived)!=expected:
            mismatch.append(source)
    archives.append(dict(archive=name, files=len(entry['source_hashes']), missing=missing, mismatch=mismatch))
    errors.extend(missing+mismatch)

blocked_runs = [r for r in renders['runs'] if '4551' in r.get('error','')]
if len(blocked_runs)!=1 or len(renders['runs'])!=1:
    errors.append('Unexpected render execution history; manual review required')
if probe['profile_probe_cases']!=5 or not probe['profile_and_binding_checks_passed']:
    errors.append('Profile probe incomplete')
images = list((STUDY/'render-validation/raw').glob('*/capture.jsonl.ppm'))
states = list((STUDY/'render-validation/raw').glob('*/capture.jsonl.state-*.f32'))
samples = list((STUDY/'render-validation/raw').glob('*/capture.jsonl'))
if images or states or samples:
    errors.append('Unexpected render outputs; audit scope must be revised')
if blocked_runs:
    registry = ROOT/'Build/OptimizationLab/blocked-executables.json'
    blocked = read(registry) if registry.exists() else {}
    blocked.setdefault(renders['executable_sha256'],dict(path=str(ROOT/'Build/bin/Release/MDSS.exe'),
        winerror=4551,status='blocked_before_execution',recorded_at=stamp(),
        action='Do not relaunch, rebuild, rename or repackage as a workaround',
        evidence=str(STUDY/'render-validation/manifest.json')))
    write(registry,blocked)

write(STUDY/'readiness-followup.json',dict(created_at=stamp(),
    status='capability_probe_passed_rendering_blocked_comparative_human_study_not_ready',
    historical_record='Initial readiness.json and attempted render manifest are preserved unchanged',
    profile_cases_passed=5, prepared_single_system_tasks=3,
    reference_render_conditions_planned=len(render_plan['runs']),
    reference_render_launch_attempts=len(renders['runs']),
    reference_render_completed=0, reference_render_unattempted=len(render_plan['runs'])-len(renders['runs']),
    render_block=dict(winerror=4551, executable_sha256=renders['executable_sha256'],
        policy='Windows Application Control', before_execution=True,
        blocked_hash_recorded=True, workaround_attempted=False),
    rendered_images=len(images), state_arrays=len(states), frame_sample_files=len(samples),
    human_sessions=0, human_completion_times=None, human_quality_results=None,
    pilot_visual_quality_gate='not_validated',
    comparative_no_go='Same authoring declaration workflow and headless M; no identifiable model intervention or matched complete workflow',
    source_archive_audit=archives, audit_errors=errors,
    editor_layout_restored=renders['editor_layout_restored'],
    unexecuted=['Human pilot','Comparative authoring study','Rendered acceptance','Scene restart persistence',
        'Rendered scene reaction/material ledger','GPU performance comparison'],
    source_test_limit='SharedSurfaceGeometry invalid-texel render triangle expectation failure remains unresolved'))

write(STUDY/'study.json',dict(schema_version=1,study_id='20261027-authoring-readiness',
    question='Can a fair authoring study currently identify the contribution of the common coupled model?',
    status='readiness_audited_comparative_no_go_rendering_blocked',
    created_at=read(STUDY/'plan.json')['created_at'], updated_at=stamp(),
    record_kind='validation and study design, no performance experiment ID',
    experiments=[dict(id='profile-appearance-probe',status='completed_5_cases',path='raw/probe/'),
        dict(id='reference-render-validation',status='blocked_before_execution_1_of_6_attempted',path='render-validation/')],
    report='../../Docs/MDSSP-Agent-Vault/03_Development/06_Authoring-Study-Protocol.md'))

(PACK/'README.md').write_text('''# 단일 시스템 제작 pilot 팩

이 팩은 기능·저장·외관 연결을 점검하기 위한 준비물입니다. 사람의 작업 시간·성공률·품질 결과가 아닙니다. 현재 앱 실행은 Windows Application Control(4551)에 차단돼 화면을 확인하지 못했습니다. 차단을 우회하는 실행 지침은 제공하지 않습니다.

1. P1/start, P2/start, P3/start를 각각 별도 제출 폴더로 복사합니다. 원본 시작 파일은 보존합니다.
2. 각 brief.md를 읽습니다. 같은 폴더의 Task.Scene이 Editable.SurfaceProfileMap과 Editable.SRProfile을 참조합니다.
3. P1은 ice_melting의 heat threshold 0.25→0.45 저장·새 프로세스 재로드를 확인하는 제한된 기능 과제입니다.
4. P2는 새 Input/Incoming의 0.1을 별도 잔류 State로 전환합니다. 기존 젖은 외관을 사용하려면 정확한 이름 absorbedwetness를 씁니다. reference/absorbedwetness.SRProfile은 진행자의 기능 대조용이며, 탐색·창작 시간 연구 참가자에게 제공할 정답이 아닙니다.
5. P3는 P2의 최종 Profile을 20° 배치에 재사용하고 원래 8° 배치도 재확인합니다. 새 Mesh 일반화 시험이 아닙니다.
6. 수정 파일, 새 프로세스 재로드 증거, 실제 로그/State/영상과 실패를 제출합니다. session-template.json의 null은 실행 후 실측으로만 채웁니다.

완료에는 Profile 파싱만으로 부족합니다. 렌더링 가시성, input/reaction/decay/경계 장부, 새 프로세스 설정 복원이 필요하며 이 팩에서 아직 검증하지 못했습니다. 독립적인 외관 목표·품질 기준도 미확정입니다. UI의 Apply Override는 원본 파일을 저장하지 않습니다. Reaction 변경은 파일 편집·scene reload가 필요합니다.

asset 경로는 이 checkout의 절대 경로입니다. 다른 위치에서는 생성기로 다시 준비해야 합니다. U/M backend만 바꾼 인간 시간 비교는 같은 선언 workflow와 M의 UI 부재 때문에 현재 시행하지 않습니다.

상세 명세: Docs/MDSSP-Agent-Vault/03_Development/06_Authoring-Study-Protocol.md
''',encoding='utf-8')

# Post-execution documentation/archive is explicitly distinct from preregistered inputs.
followup_files = [ROOT/'Scripts/Research/authoring_render_validation.py',pathlib.Path(__file__).resolve(),
    ROOT/'Docs/MDSSP-Agent-Vault/03_Development/06_Authoring-Study-Protocol.md', PACK/'README.md',
    STUDY/'readiness-followup.json',STUDY/'study.json',STUDY/'session-template.json']
archive = STUDY/'raw/followup'
hashes = {}
for source in followup_files:
    rel = source.relative_to(ROOT)
    dest = archive/rel
    dest.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2('\\\\?\\'+str(source.resolve()),'\\\\?\\'+str(dest.resolve()))
    hashes[str(rel)] = digest(source)
    if digest(dest)!=hashes[str(rel)]:
        raise RuntimeError('Followup archive mismatch')
write(STUDY/'followup-archive.json',dict(created_at=stamp(),scope='Post-execution interpretation and runner error-status fix, not original pre-run inputs',
    archive=str(archive.relative_to(ROOT)),sha256=hashes))
print('Archive audit:',len(archives),'archives;',sum(a['files'] for a in archives),'files;',len(errors),'errors')
if errors:
    raise SystemExit(1)
