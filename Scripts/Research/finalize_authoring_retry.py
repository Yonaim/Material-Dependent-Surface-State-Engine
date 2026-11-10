"""저작 재검증 결과를 원래 판정과 구분해 문서에 반영한다.

Write the retry interpretation from actual archived data, preserving earlier failures.
"""

import pathlib, re, shutil
from authoring_study import ROOT, STUDY, read, write, digest, stamp

OUT = STUDY / 'policy-change-retry'
EXP = OUT / 'render-v2'


def main():
    analysis = read(EXP / 'analysis.json')
    manifest = read(EXP / 'manifest.json')
    geometry = read(OUT / 'geometry.json')
    non_runtime_errors = [e for e in analysis['audit_errors'] if not e.endswith(': runtime error')]
    if non_runtime_errors or manifest['status'] != 'completed' or geometry['status'] != 'passed':
        raise RuntimeError('Unresolved evidence/analysis failure; cannot finalize')
    runtime_failed = any(r['runtime_error_count'] for r in analysis['runs'])
    # Preserve active-block history and retire only the two binaries actually retried.
    registry = ROOT / 'Build/OptimizationLab/blocked-executables.json'
    if not (OUT / 'prior-blocked-registry.json').exists():
        write(OUT / 'prior-blocked-registry.json', read(registry))
    blocked = read(registry)
    resolved = [geometry['executable_sha256'], manifest['executable_sha256']]
    for key in resolved:
        blocked.pop(key, None)
    write(registry, blocked)
    write(
        OUT / 'policy-resolution.json',
        dict(
            recorded_at=stamp(),
            basis='User reported Smart App Control disabled; same previously blocked binaries now launch. CPU test passed; GPU validation failure is tracked separately.',
            authorization='retry-authorization.json',
            resolved_executable_hashes=resolved,
            agent_modified_windows_policy=False,
            prior_failure_records_preserved=True,
            removed_from_active_block_registry_after_confirmed_native_launch=True,
        ),
    )
    selection = analysis.get('capture_selection')
    replay = (
        read(pathlib.Path(selection['replay_manifest']))
        if selection and selection['replay_manifest']
        else None
    )
    finish = max(
        manifest['finished_at'], replay['finished_at'] if replay else manifest['finished_at']
    )
    current = dict(
        updated_at=stamp(),
        status=(
            'geometry_passed_render_outputs_observed_vulkan_validation_failed'
            if runtime_failed
            else 'geometry_passed_reference_renders_completed_human_study_not_ready'
        ),
        geometry_test='passed_same_previously_blocked_executable',
        render_conditions_observed=len(analysis['runs']),
        reference_render_validations_passed=0 if runtime_failed else len(analysis['runs']),
        audit_errors=analysis['audit_errors'],
        human_sessions=0,
        human_productivity='not_measured',
        render_acceptance=analysis['acceptance'],
        runtime_error_counts={r['id']: r['runtime_error_count'] for r in analysis['runs']},
        independent_quality_gate='not_validated',
        material_ledger='not_collected_in_render_capture',
        initial_fixture_problem='Absolute paths rejected by SceneLoader; relative-path v2 pack preserves same resolved assets',
        earlier_failures=[
            'Original Application Control blocks',
            'One long-path archive preflight failure without app launch',
            'One actual SceneLoader absolute-path failure',
        ],
        capture_replays=selection['replayed_conditions'] if selection else [],
        sources=dict(
            geometry='policy-change-retry/geometry.json',
            render_manifest='policy-change-retry/render-v2/manifest.json',
            analysis='policy-change-retry/render-v2/analysis.json',
            tasks='policy-change-retry/task-pack-v2.json',
        ),
    )
    write(STUDY / 'readiness-current.json', current)
    study = read(STUDY / 'study.json')
    study.update(updated_at=stamp(), status=current['status'])
    entries = [
        dict(id='policy-change-geometry-retry', status='passed', path='policy-change-retry/'),
        dict(
            id='relative-path-reference-renders',
            status='observed_6_cases_validation_failed' if runtime_failed else 'completed_6_cases',
            path='policy-change-retry/render-v2/',
        ),
    ]
    study['experiments'] = [
        x for x in study['experiments'] if x['id'] not in {e['id'] for e in entries}
    ] + entries
    write(STUDY / 'study.json', study)
    prefix = '../../../Performance-Data/20261027-authoring-readiness/policy-change-retry/'
    text = '''\n## 7. Smart App Control 해제 후 실제 재실행 — 2026-10-27\n\n'''
    text += f'''사용자가 Smart App Control을 해제했다고 알리고 재시도를 명시적으로 요청했다. 이전과 **같은 exe hash**의 Geometry 테스트와 본 앱을 실행했다. 에이전트가 Windows 정책을 변경한 것은 아니다. 이전 차단 기록·명세는 보존하고 [사용자 재시도 지시]({prefix}retry-authorization.json), [Geometry 결과]({prefix}geometry.json), [새 manifest]({prefix}render-v2/manifest.json)를 따로 기록했다. Geometry 테스트는 통과했다. 제품 Geometry/Solver를 고치지 않고 테스트의 ID 해석만 바로잡은 결과다.\n\n'''
    text += '''과제 팩에는 별도의 준비 오류가 있었다. 최초 절대 경로 Scene이 실제 `SceneLoader::ResolveScenePath`의 상대 경로 요구를 위반해 앱 기동 후 실패했다. 앞선 파일 존재 검사는 이 계약을 검사하지 못했다. 오류를 기존 시스템의 한계로 돌리지 않는다. **동일한 mesh/animation을 가리키는 상대 경로로 과제 파일만 수정**하고 새 v2 명세·폴더에서 재실행했다. 모든 조건에서 원래 의도한 자산과 같은지 검증했다. 입력 보관 중의 긴 경로 처리 실패도 별도 보존했다. 원래 실패를 지우거나 완전한 최초 사전등록이라고 부르지 않는다.\n\n'''
    if runtime_failed:
        text += '''**앱 검증 판정은 실패다.** 아래 6조건은 화면·State 파일을 관측한 것이며 정상 GPU 검증을 통과한 reference가 아니다. 모든 조건의 로그에서 `VUID-vkCmdBindPipeline-pipeline-parameter` 오류가 발생했다. 첫 메시지는 pipeline layout이 이미 삭제된 descriptor set layout을 참조한다는 것이다. 중복 메시지 제한 때문에 로그의 오류 줄 수를 실제 오류 발생 총수로 해석하지 않는다. 비음수 State와 반복 일치는 이 GPU 자원 수명 위반을 상쇄하지 않는다. 아래 수치/화면은 **잠정 관측**으로만 보존하며 기여·외관 기능의 확정 검증 근거로 쓰지 않는다.\n\n소스 대응은 `TSurfaceStateSystem::PrepareScratchStorage` → `TSurfaceGPUResourceManager::ConfigureScratchStorage`에서 descriptor를 교체·삭제하는 경로다. `TSurfaceStateDescriptorResources` 소멸자는 이전 layout을 삭제하지만, 기존 Solver의 pipeline layout은 처음 받은 layout을 계속 참조한다. 이번 설정은 기본 FP16 RawFlux에서 FP32로 바꿔 scratch 재구성을 유발한다. 이 연결은 코드와 로그에 근거한 원인 진단이며, 이번 재시도에서 GPU 제품 코드 수정이나 수정 후 검증을 수행한 것은 아니다.\n\n'''
    text += f'''실행 구간은 `{manifest['started_at']}` ~ `{finish}` (KST). 앱 exe SHA-256 `{manifest['executable_sha256']}`, Release / AMD Radeon RX 6700 XT / Vulkan validation ON. simulation·surface/overlay mesh 128, Coupling ON, Dense Solver·FP32 RawFlux/Weights, 원래 데모 입력·카메라, fixed dt 1/60이다. 실제 옵션은 각 `.settings.json`에 저장했다. 채택한 실행당 362 Solver Step이며 이미지는 frame 359, 최종 State는 drain 후 Step 362이므로 같은 시각이라고 부르지 않는다. 전체 14초 중 약 6초만 관측했다.\n\n'''
    if selection and selection['replayed_conditions']:
        text += f"최초 v2 실행 중 `{', '.join(selection['replayed_conditions'])}` 조건은 종료 코드 0에도 필수 화면/프레임 산출물을 충족하지 못했다. swapchain 재구성과 실제 제출 프레임 부족을 [원래 gate 감사]({prefix}initial-capture-gates.json)에 보존하고 해당 조건만 새 프로세스로 재실행했다. 수치 결과의 유불리가 아닌 기존 산출물/프레임 기준으로 [선택]({prefix}capture-selection.json)했으며, 미완료 실행을 성공 수에 합산하지 않는다.\n\n"
    text += '| 조건 | 최종 Ice 합 | 최종 Waterfilm 합 | 최종 잔류량 합 | Step |\n|---|---:|---:|---:|---:|\n'
    for run in analysis['runs']:
        total = run['totals']
        residue = total.get('absorbedwetness', total.get('wetness', 0.0))
        text += f"| {run['id']} | {total['ice']:.6f} | {total['waterfilm']:.6f} | {residue:.6f} | {run['simulation_step_serial']} |\n"
    text += '\n합은 세 판 전체의 texel 총량이며 면적을 다시 곱하지 않았다. C 판은 별도 물 입력을 받으므로 판끼리 또는 조건 사이의 최종 합만으로 보존성을 판단하지 않는다.\n\n'
    text += '| 대조 | 이름 대응 후 최종 State 비트 일치 | 달라진 화면 pixel 수 | RGB 절대차 평균(0–255) |\n|---|---|---:|---:|\n'
    for pair in analysis['comparisons']:
        text += f"| {pair['a']} / {pair['b']} | {pair['all_named_states_bit_equal']} | {pair['changed_pixels']} | {pair['mean_absolute_rgb_8bit']:.8f} |\n"
    text += '\n`wetness`와 `absorbedwetness` 비교는 두 잔류 채널을 이름 대응시켰다. 화면 pixel 차이는 외관/배치 차이의 서술적 지표이며 품질·사용성 점수가 아니다. 최종 State와 이미지의 시점도 다르다.\n\n'
    text += f'''- **실제 실행:** Geometry 테스트 통과 1건, 화면·State 관측 {len(analysis['runs'])}조건. 최종 저장 State의 음수/NaN/Inf와 exporter의 최종 Reaction budget/arithmetic flag는 0. GPU 로그 gate 실패 {sum(r['runtime_error_count']>0 for r in analysis['runs'])}조건, 그 외 원자료·설정·배열 합·자산 재경로·보관 hash 감사 오류 {len(non_runtime_errors)}. 중간 모든 Step의 Reaction flag/물질 장부를 수집한 검증은 아니다.\n- **해석:** 이름 대응한 최종 wetness/absorbedwetness 배열 및 새 프로세스 반복의 State는 비트 일치했다. 화면의 젖은 외관 차이는 작고 반복 화면도 완전히 같지는 않았다. 잠정 관측이며 GPU 검증 오류가 해결되기 전 기능 정확성·사용성의 확정 증거로 쓰지 않는다. 독립 평가자·목표 외관 gate·인간 제작 시간도 없다.\n- **계속 미측정:** 사람의 작업 완료/시간/실패율, 같은 품질에 도달하는 제작 부담의 상대적 우위, 새 Mesh 일반화, 전체 시간 수지, 상호작용 UI 조정/저장 과정, 정식 GPU 성능. 주어진 reference 파일의 새 프로세스 재실행을 사람의 저장 작업 검증으로 대체하지 않는다. 현재 U/M 인간 비교 No-Go 판단과 시스템 기여 미확보 판정은 유지한다.\n- 수정된 [과제 팩 v2 색인]({prefix}task-pack-v2.json), [잠정 수치·화면 분석과 오류]({prefix}render-v2/analysis.json), [현재 상태](../../../Performance-Data/20261027-authoring-readiness/readiness-current.json). 원래 차단 상태 JSON은 당시 관측으로 보존한다.\n\n![GPU validation 오류가 있는 6조건의 잠정 관측]({prefix}render-v2/figures/reference-sheet.png)\n'''
    report = ROOT / 'Docs/Agent-Vault/04_Development/07_Authoring-Study-Protocol.md'
    old = report.read_text(encoding='utf-8')
    old = re.sub(
        r'^> .*$',
        '> **현재 판정:** 실행 차단은 해소됐고 Geometry 테스트는 통과했다. 화면·State 6조건을 확보했으나 모두 Vulkan 자원 수명 오류가 있어 앱 검증은 실패다(§7). 최초 경로 오류와 화면 누락도 보존했다. 인간 제작 시간 비교는 여전히 No-Go다.',
        old,
        count=1,
        flags=re.MULTILINE,
    )
    marker = '\n## 7. Smart App Control 해제 후 실제 재실행'
    if marker in old:
        old = old[: old.index(marker)]
    report.write_text(old + text, encoding='utf-8')
    archive = OUT / 'raw/final-analysis'
    files = [
        pathlib.Path(__file__).resolve(),
        ROOT / 'Scripts/Research/analyze_authoring_retry.py',
        ROOT / 'Scripts/Research/retry_authoring_validation.py',
        ROOT / 'Scripts/Research/replay_missing_authoring_captures.py',
        ROOT / 'Scripts/Research/authoring_render_validation.py',
        ROOT / 'Scripts/Research/authoring_study.py',
        report,
        STUDY / 'readiness-current.json',
        EXP / 'analysis.json',
        OUT / 'policy-resolution.json',
    ]
    hashes = {}
    for path in files:
        rel = path.relative_to(ROOT)
        target = archive / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2('\\\\?\\' + str(path.resolve()), '\\\\?\\' + str(target.resolve()))
        hashes[str(rel)] = digest(path)
        if digest(target) != hashes[str(rel)]:
            raise RuntimeError('Final archive mismatch')
    write(
        OUT / 'final-analysis-archive.json',
        dict(
            created_at=stamp(),
            scope='Post-execution analysis, separate from pre-run inputs',
            archive=str(archive.relative_to(ROOT)),
            sha256=hashes,
        ),
    )
    print(
        'Retry finalized: geometry passed; render outputs observed, Vulkan validation failed; no human productivity claim'
    )


if __name__ == '__main__':
    main()
