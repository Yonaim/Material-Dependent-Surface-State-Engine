"""모듈 기준선 실험의 비교 결과와 제한을 문서에 반영한다.

Publish measured tables into the existing experiment section after all required gates and a decision.
"""

import json
from run_modular_baseline import EXP, ROOT, write, stamp

comparison = json.loads((EXP / "comparisons.json").read_text(encoding="utf-8"))
audit = json.loads((EXP / "study-audit.json").read_text(encoding="utf-8"))
decision = json.loads((EXP / "decision.json").read_text(encoding="utf-8"))
manifest = json.loads((EXP / "manifest.json").read_text(encoding="utf-8"))
if (
    comparison["remaining_diagnostics"]
    or comparison["failed_comparisons"]
    or audit["audit_failures"]
):
    raise SystemExit("Cannot finalize an incomplete or failed comparison")
latest = {r["id"]: r for r in manifest["runs"] if r["status"] == "completed"}
main = [r for r in latest.values() if r["stage"] == "diagnostic"]
rows = []
for task, variants in [("T0", "BDE"), ("T1", "D"), ("T2", "D"), ("T3", "DE")]:
    for v in variants:
        key = f"diagnostic-{task}-{v}-U-128"
        r = latest[key]
        s = r["summary"]
        c = next(
            x
            for x in comparison["comparisons"]
            if x["a"] == key and x["b"] == key.replace("-U-", "-M-")
        )
        maximum = max(max(check["state_l1_relative"]) for check in c["snapshots"])
        rows.append(
            f"| {task}-{v} | {len(s['channels'])} / {len(s['cumulative_extents'])} | {'통과' if c['passed'] else '실패'} | {'비트 일치' if c['state_geometry_weights_bit_identical'] else '차이 있음'} | {maximum:.6g} |"
        )
max_budget = max(
    max(x["max_relative_per_channel"])
    for c in comparison["comparisons"]
    for x in c.get("cumulative_channel_budgets", [])
)
first = manifest["runs"][0]["started_at"]
last = manifest["runs"][-1]["finished_at"]
core_exes = {r["executable_sha256"] for r in main}
extension = json.loads((EXP / "extension-audit.json").read_text(encoding="utf-8"))
extension.update(
    status="completed_declaration_only_execution_audit",
    verified_at=stamp(),
    main_diagnostic_executable_hashes=sorted(core_exes),
    fourteen_main_runs_single_executable=len(core_exes) == 1,
    decision=decision["status"],
    limitation="No retrospective reconstruction of marginal authoring time; the generic infrastructure was written with task definitions known",
)
write(EXP / "extension-audit.json", extension)

text = f'''### 현재 결과와 판단

**완료: 이 과제에서 MDSSP의 구조적 확장 부담 우위는 지지되지 않았다.** 동일 수치 모델과 공통 서비스를 제공받은 M도 T1/T2/T3를 설정 선언과 일반 모듈 factory로 처리했다. 이는 공통 모델 자체의 선행연구 대비 신규성 판정과 구분한다.

- 실행: `{first}` ~ `{last}`. 성공 **{audit['completed_attempts']}회**, 평가 Step **{audit['main_steps']:,}개**(진단 {audit['diagnostic_steps']:,}, 나머지는 timing 경로 1초 검사). 준비 실행·dt=0 bootstrap은 이 Step 수에 더하지 않았다.
- [감사](../../../Performance-Data/20261026-coupling-study/baseline/study-audit.json): 오류 **{audit['audit_failures']}개**, 쌍별 gate 실패 **{comparison['failed_comparisons']}개**. 최대 누적 채널 장부 상대 잔차 **{max_budget:.6g}**. 전체 실행 상태는 manifest에 보존한다.
- 14개 본 진단은 동일 실행 파일 `{next(iter(core_exes))[:12]}`와 동일 모듈 핵심 코드로 수행했다. 수치 provider·기존 Solver·Reaction·Profile은 v1 확인 시점에서 바뀌지 않았고 공통 연구용 adapter/계측만 확장했다.

| 과제/조건 | State / Rule 수 | 동등성 gate | State·형상·Weights·Flux 등 저장 배열 | 최대 State L1 / 공급량 |
|---|---:|---|---|---:|
{chr(10).join(rows)}

같은 task/variant에서 0/1/2/4/8/14초의 snapshot과 전체 Step의 State 총량·하류량·Input·Decay·실제 경계 순수송·Rule extent를 비교했다. 숫자가 같다는 것은 **공유한 수치 모델의 동등한 구현**이라는 뜻이며 정확한 물리·독립 수치 검증·새 표현력의 증거가 아니다.

T0의 M-separate 14초 대조, ledger OFF, 등록 역순, 실제 모듈별 dispatch 순서의 정순/역순 대조도 gate를 통과했다. 두 timing 경로 1초 검사는 준비 후 S0/G0/Weights/Concavity 복원과 진단 기준 최종 State의 비트 일치를 확인했다. 첫 smoke binary 3회에는 Step별 timing export가 없으며 audit의 미지원 항목으로 남겼다. 정식 반복 성능 비교와 혼용하지 않는다.

### 확장 부담과 구조적 판정

| 변경 | U에서 필요한 변경 | 최종 M에서 필요한 변경 | 해석 |
|---|---|---|---|
| T1 Mud 추가 | State/초기 분포 선언 | 같은 선언 + 일반 factory의 자동 모듈 등록 | 전용 수송·형상 결합 코드 없음 |
| T2 Wetness/유입 전환 | State/Reaction 선언 | 같은 선언, 기존 Incoming/positive input 출력 사용 | 전용 Reaction/공급량 경쟁 코드 없음 |
| T3 합성·회전 | 두 선언의 합성 및 transform | 같은 합성·transform | 새로운 조합별 branch·계수 재조정 없음 |

[설정/소스 동일성 감사](../../../Performance-Data/20261026-coupling-study/baseline/extension-audit.json)에 recipe와 고정 실행 파일을 기록했다. 이것은 같은 범용 기반이 각 과제를 데이터 변경만으로 실행한다는 **능력 검사**다. 미래 과제를 알고 작성한 adapter/factory의 코드 줄 수나 AI 작업 시간을 인간 제작 생산성의 측정으로 제시하지 않는다. 일회 M 기반 작성 비용과 이미 존재하던 U의 전체 개발 비용도 직접 비교하지 않았다.

최종 M의 fused 수송은 U와 같은 provider shader를 사용하고 M의 별도 조정자가 실행한다. 따라서 차이가 소유권·Vulkan 실행 조정으로 수렴했다. 이 구성으로 **‘단일 공통 Solver라서 일반 모듈식 설계보다 결합이 본질적으로 쉽다’는 우위는 확인하지 못했다.** 이 결과를 피하려고 M의 factory·공통 Geometry/Reaction·batching을 금지하면 비교군을 인위적으로 약화시키게 된다.

실제 구현은 LocalFlux provider 하나를 사용하는 최소 모듈 조정자다. 모듈의 channel은 공유 Profile table의 파라미터/형상 기여 view를 가리키고, `height_contributor`는 그 view에서 유도한 진단 metadata이며 별도 형상 스위치가 아니다. 단일 Profile·같은 판 graph·최대 5 State를 평가했다. 연구용 channel selector의 구현 상한은 7이며, 이 prototype을 임의 provider·임의 State 수·다중 재질 엔진 전체의 대체물로 제시하지 않는다. **현재 비교는 공통 모델 자체를 양쪽에 제공한 구조 대조**여서, 모델 설계의 신규성과 실제 선행 엔진의 제작 부담을 직접 식별하지 못한다.

![5개 State 합성의 동일 결과](../../../Performance-Data/20261026-coupling-study/baseline/figures/T3-distributions.png)

### 성능 실행의 중단과 남은 주장

[최종 결정](../../../Performance-Data/20261026-coupling-study/baseline/decision.json)에 따라 v1 §8의 구조적 중단 조건을 적용했다. **256 진단 기준 4개와 정식 비용 40개 조건은 미실행**이다. 원 계획과 조건 목록을 삭제하지 않고 deferred로 남겼다. source 고정·수치 gate·동일 설정의 범용 모듈 구성이 성립했으므로, 이번 구조적 확장 부담 질문에서 대규모 성능 반복을 추가해도 그 차이를 식별하지 못한다는 판단이다.

**성능/메모리 우위는 미판정이다.** 진단 timestamp 및 1초 timing 경로 검사를 정식 반복 성능 증거로 승격하지 않는다. M의 barrier/dispatch 구현 차이에 따른 속도만으로 공통 모델의 새 Contribution을 주장하지 않는다. setup/compile 시간·driver pipeline 메모리도 미계측이다.

H1의 State-Driven Geometry Coupling 기전 증거는 유지된다. H3 중 모듈 조정 방식에 의한 확장 부담 우위는 이 범위에서 지지되지 않으며, H4 모델의 신규성은 여전히 별도 질문이다. 실제 선행 프레임워크의 계산 모델과 비교해 어떤 제한을 해결하는지 정의하기 전에는 이번 결과를 SIGGRAPH 시스템 기여의 증거로 쓰지 않는다.

'''
report = ROOT / "Docs/Agent-Vault/05_Experiments/02_Performance-Measurements.md"
content = report.read_text(encoding="utf-8")
start = content.index("### 현재 결과와 판단", content.index("## PERF-042"))
end = content.index("### 중단 기준과 해석의 경계", start)
content = content[:start] + text + content[end:]
content = content.replace(
    "현재 실행 중이며 구조적/성능 우위는 미판정이다.",
    "동일 모델의 모듈식 구성도 확장 과제를 선언만으로 처리했다. 구조적 우위는 지지되지 않았고 정식 성능 비교는 중단했다.",
)
content = content.replace(
    "상태: **실행 중**.", "상태: **진단/확장 비교 완료 · 사전 조건에 따른 구조적 중단**.", 1
)
report.write_text(content, encoding="utf-8")
study_path = ROOT / "Performance-Data/20261026-coupling-study/study.json"
study = json.loads(study_path.read_text(encoding="utf-8"))
study["baseline"].update(
    status="completed_structural_stop",
    completed_at=stamp(),
    completed_attempts=audit["completed_attempts"],
    audited_steps=audit["main_steps"],
    decision="No structural extension-burden advantage demonstrated in v1 tasks; repeated cost study not run",
)
write(study_path, study)
print(report)
