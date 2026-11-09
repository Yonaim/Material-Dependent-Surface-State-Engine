"""Fill the shared experiment log from machine-written observations, without performance claims."""

from meso_study import ROOT, EXP, STUDY, read, write, stamp

audit = read(EXP / 'study-audit.json')
manifest = read(EXP / 'manifest.json')
if audit['errors'] or audit['missing']:
    raise SystemExit('Audit has unresolved study failures')
reconstruction = {r['id']: r for r in read(EXP / 'reconstruction.json')['results']}
pairs = read(EXP / 'comparisons.json')['comparisons']


def pair(a, b):
    return next(r for r in pairs if r['a'] == a and r['b'] == b)


def water(r):
    return r['state_l1_pct_supply'][r['channels'].index('waterfilm')]


start = min(r['started_at'] for r in manifest['runs'])
end = max(r['finished_at'] for r in manifest['runs'])
prefix = '../../../Performance-Data/20261027-meso-study/PERF-043-meso/'
result = f'''### 실행 결과

- 실행 일시: `{start}` ~ `{end}` (KST). **35회 완료: CPU 12조건 + GPU 본 비교 19조건 + 대조 4조건, GPU 평가 5,580 Step**. 준비/bootstrap은 Step 수에서 제외했다. 실패한 연구 실행은 없다.
- 원래 32조건은 유지했다. CPU 결과 이후·원래 GPU 실행 도중·수송 비교 수치 분석 전에 [동일 높이 후처리 대조 명세]({prefix}postprocess-control-plan.json) 1조건을 추가했다. 제품의 같은 normal/concavity 후처리를 재사용하는 adapter가 동일 높이를 넣었을 때 결과를 바꾸지 않는지 검사하는 대조다. 완전한 사전등록으로 부르지 않는다.
- 환경: Windows / Release / AMD Radeon RX 6700 XT, driver raw version `8388961`. 전체 35조건의 실행 파일 SHA-256은 동일한 `{audit['unique_executable_hashes'][0]}`이다. native production Solver·Meso 적분 식은 변경하지 않았다.
- [manifest]({prefix}manifest.json), [복원 결과]({prefix}reconstruction.json), [수송 비교]({prefix}comparisons.json), [감사]({prefix}study-audit.json).
- 연구 실행 감사 오류 **0**, 누락 **0**. NaN/음수/Reaction budget·arithmetic/동결 위반 **0**. 최대 material 상대 수지 잔차 **{audit['max_relative_material_budget']:.8e}**, 독립 최종 채널 수지 상대 잔차 **{audit['max_independent_channel_relative_budget']:.8e}**로 `1e-5` gate를 만족했다. LS 방정식 잔차도 `1e-8` 이하이다.
- [legacy flat 대조]({prefix}legacy-control.json)의 State·형상·Weights·Concavity·Flux·Alpha 10개 배열이 PERF-042의 1초 실행과 비트 일치했다. [동일 높이 후처리 대조]({prefix}postprocess-control.json)는 0/1/2/4초 22개 배열이 비트 일치했다. 모든 D/E 초기 G0와 같은 해상도 S0도 일치했다.
- **보관 누락과 재현 보완:** 최초 33회는 exe·GLSL·연구용 생성 CPU 코드를 보관했으나 실제 production SPIR-V 폴더를 잘못 검색해 실행 전 바이너리 보관이 누락됐다. 이를 사후에 실행 전 해시가 있었던 것으로 채우지 않았다. [보완 명세]({prefix}shader-capture-replay-plan.json)에 따라 경로 수정 후 brick production/LS 2조건을 재실행했고, 실제 production SPIR-V를 실행 전에 보관했다. [재현 검사]({prefix}shader-capture-replay.json)의 **44개 배열이 원래 두 실행과 비트 일치**했다. 나머지 최초 실행의 개별 shader 보관 누락은 제한으로 남는다. 최종 소스·분석 보관본은 [보관 색인]({prefix}analysis-archive.json)을 따른다.
- **전체 테스트가 통과한 것은 아니다.** `MDSS_NormalMapTransfer`는 통과했지만 `MDSS_SharedSurfaceGeometry`는 `Tests/SharedSurfaceGeometryTests.cpp:233`의 invalid-texel render triangle 개수 검사 1개가 실패했다. 관련 제품 코드·테스트는 이번 변경에서 수정하지 않았으며, 새 연구 경로는 모든 texel이 유효한 평면이다. [실제 테스트 로그]({prefix}raw/unit-tests.log)를 보존했다. render mesh 실패 원인 수정은 이번 연구 결과에 포함하지 않는다.

### 복원 결과

아래 오차는 component 평균을 0으로 맞춘 뒤의 높이 상대 L2/RMSE이다. 알려진 합성 높이만 ground truth가 있다. 실물 asset에는 LS 대비 차이와 edge 제약 잔차를 사용한다.

| 입력·해상도 | production 높이 오차 / 알려진 높이 | 표준 LS 높이 오차 / 알려진 높이 | production 높이 차이 / LS 높이 |
|---|---:|---:|---:|
'''
for key, label in [
    ('cpu-analytic-64', 'float analytic 64'),
    ('cpu-analytic-128', 'float analytic 128'),
    ('cpu-analytic-256', 'float analytic 256'),
    ('cpu-png-128', 'RGBA8 synthetic 128'),
    ('cpu-brick-128', '실제 brick 128'),
    ('cpu-brick-256', '실제 brick 256'),
]:
    r = reconstruction[key]
    p = (
        f"{100*r['production_relative_rmse_vs_truth']:.6f}%"
        if 'production_relative_rmse_vs_truth' in r
        else '정답 높이 미상'
    )
    q = (
        f"{100*r['reference_relative_rmse_vs_truth']:.6f}%"
        if 'reference_relative_rmse_vs_truth' in r
        else '정답 높이 미상'
    )
    result += f"| {label} | {p} | {q} | {100*r['height_relative_rmse_vs_ls']:.6f}% |\n"
r = reconstruction['cpu-brick-128']
neutral = reconstruction['cpu-flat8-128']
result += f'''
- 위 여섯 production 조건 모두 128회에 도달했다. float analytic의 오차는 해상도에 따라 단조 감소하지 않는다. 같은 이산 slope 제약을 더 정확히 푸는 LS에서는 크게 줄었다. **기존 반복 예산·수렴 처리의 한계이며 새로운 복원 알고리즘 필요성의 증거는 아니다.** 128 analytic의 높이 RMSE는 LS 대비 `{reconstruction['cpu-analytic-128']['height_rmse_vs_ls']:.8g}` mesh-local length이다. 이 장면의 Z scale은 `.22`이므로 normal 방향 월드 차이는 그 0.22배 수준이다. 상대 높이 오차만으로 큰 수송 오류를 가정하지 않는다.
- brick 128의 edge 상대 RMS 잔차는 production **{100*r['production_edge_relative']:.6f}%**, 정확한 동일 제약 LS도 **{100*r['reference_edge_relative']:.6f}%**다. 독립 계산한 production edge 잔차는 C++ 보고값과 `1e-6` 이내로 일치했다. 따라서 반복 오차와 **선택한 graph에서 단일 높이로 동시에 만족할 수 없는 샘플 기울기의 불일치**를 분리해야 한다. 이 값은 물리적 높이 오차 70%라는 뜻이 아니다. 샘플링·양자화·법선 처리·이산 제약 자체가 모두 포함되며, 원본 asset만의 문제로 단정하지 않는다.
- RGBA8 중립값 `(128,128,255)`는 production decode에서 정확한 `(0,0,1)`이 아니다. flat8의 복원 범위는 `{neutral['production_range'][0]:.8g}` ~ `{neutral['production_range'][1]:.8g}` local length였다. 예상 가능한 8-bit decode bias의 진단이며 별도 신규성으로 취급하지 않는다.

### 후속 수송 결과

아래 값은 4초 시점 `sum(abs(W_a-W_b)) / (초기 Ice+Waterfilm + 누적 positive material Input) × 100`이다. 속도 개선율이나 물리적 정확도는 아니다.

| 비교 | 물막 L1 / 총 공급량 |
|---|---:|
'''
table = [
    ('float analytic: production ↔ LS', 'gpu-analytic-128-D', 'gpu-analytic-ls-128-D'),
    ('RGBA8 synthetic: production ↔ LS', 'gpu-png-128-D', 'gpu-png-ls-128-D'),
    ('brick 128: production ↔ LS', 'gpu-brick-128-D', 'gpu-brick-ls-128-D'),
    ('brick 256: production ↔ LS', 'gpu-brick-256-D', 'gpu-brick-ls-256-D'),
    (
        'brick 128 + Ice cavity: production ↔ LS',
        'gpu-brick-cavity-128-D',
        'gpu-brick-cavity-ls-128-D',
    ),
    ('flat ↔ brick Meso', 'gpu-flat-128-D', 'gpu-brick-128-D'),
    ('brick: 동적 D ↔ 초기 G0 동결 E', 'gpu-brick-128-D', 'gpu-brick-128-E'),
    ('brick cavity: D ↔ E', 'gpu-brick-cavity-128-D', 'gpu-brick-cavity-128-E'),
    ('brick: slope 강도 1 ↔ 0.5', 'gpu-brick-128-D', 'gpu-brick-128-x0.5-D'),
    ('brick: slope 강도 1 ↔ 2', 'gpu-brick-128-D', 'gpu-brick-128-x2-D'),
    ('brick D: 60 ↔ 120 Hz', 'gpu-brick-128-D', 'gpu-brick-128-120-D'),
    ('brick D: 64 ↔ 128, 면적 합 restriction', 'gpu-brick-64-D', 'gpu-brick-128-D'),
    ('brick D: 128 ↔ 256, 면적 합 restriction', 'gpu-brick-128-D', 'gpu-brick-256-D'),
]
for label, a, b in table:
    result += f'| {label} | {water(pair(a,b)):.8f}% |\n'
ref = pair('gpu-brick-128-D', 'gpu-brick-ls-128-D')
result += f'''
- brick 128의 production/LS 차이는 동일 장면의 60/120 Hz 민감도보다 작다. 최종 LS 물막 자체를 분모로 바꿔도 차이는 **{ref['state_l1_pct_final_reference'][ref['channels'].index('waterfilm')]:.8f}%**이다. 모든 장면·시간·입력에 대해 안전하다는 오차 상한은 아니다.
- Meso 유무·강도·지속적 형상 갱신은 이 장면에서 State 분포를 바꾼다. 그러나 해상도 변화의 관측 차이도 크다. 초기 Ice 분포만의 64/128·128/256 면적 합 차이는 각각 **{pair('gpu-brick-64-D','gpu-brick-128-D')['initial_l1_pct_supply'][1]:.8f}%**, **{pair('gpu-brick-128-D','gpu-brick-256-D')['initial_l1_pct_supply'][1]:.8f}%**다. graph 해상도·Normal Map 재샘플링·복원·초기장/입력 샘플링을 합친 민감도이며 순수한 수송 이산화 오차나 수렴 증명이 아니다. 실제 brick의 LS 높이 RMS도 128과 256에서 달라져 동일 연속 형상의 해상도 정밀화로 간주할 수 없다.
- D/E는 같은 G0에서 시작하고 동결 위반은 없었다. 다만 E의 dt/해상도 정밀화는 이 명세에 없으므로 D/E contrast의 수렴을 입증했다고 부르지 않는다. 이전 PERF-041의 14초 판 기전 증거와 별개인 4초 탐색 결과다.

### 해석·판정과 범위

**현상학적 Meso와 State-Driven Geometry Coupling이 계산 결과에 참여하지만, 새 복원·수송 알고리즘이 필요한 중요한 실패는 이번 조건에서 확인하지 못했다.** 알려진 높이에서 확인한 복원 오차는 같은 제약의 표준 LS로 줄일 수 있었다. 그 오차가 후속 물막에 미치는 차이는 작았다. 입력 기울기의 불일치 자체도 Normal integration의 알려진 역문제 유형이며, 이 측정만으로 새 연구 문제나 해결책을 주장하지 않는다.

- 연구 판단: **Meso 알고리즘 Contribution 가설은 현 근거로 지지하지 않음.** 시스템 통합에 가깝다는 H4 판정을 바꿀 증거가 아니다. 사용한 데이터·조건에 대한 판단이지 일반적인 연구 불가능성 선언이 아니다.
- 기술 개선 후보: 반복 중단/실제 잔차 보고와 Normal Map convention·양자화·입력 scale 계약 정리. 제품 Solver나 기본 파라미터를 이번에 바꾸거나 참조 LS를 채택하지 않았다.
- 한계: 평면 한 장·실제 map 한 개·4초 transient·한 GPU·기존 입력장. UV seam/재분할, 다른 mesh, 큰 변형, 다양한 반응/자극, 렌더링 품질과 제작자 작업량은 미평가다. 실제 asset 높이의 물리적 정답도 없다.
- 다음 연구의 우선순위는 신규 수치 알고리즘 추가보다 **실제 제작 과제에서 이 공통 모델이 줄이는 작업·실패가 무엇인지 검증하는 것**이다. 그 이점이 표준 rule/모듈 재사용으로 같아지면 새 시스템 기여로 밀지 않는다.
- 이번에 관측한 Geometry render triangle 테스트 실패는 별도 추적해야 한다. 이번 모든 유효 texel graph의 연구 감사 통과를 전체 제품 정확성 확인으로 확대하지 않는다.

그림: [복원 높이 비교]({prefix}figures/reconstruction.png), [후속 물막 분포]({prefix}figures/transport.png), [관측 민감도]({prefix}figures/sensitivities.png). 도표는 원자료에서 자동 생성했으며 실제 렌더러 화면이 아니다.

'''
path = ROOT / 'Docs/MDSSP-Agent-Vault/04_Experiments/02_Performance-Measurements.md'
text = path.read_text(encoding='utf-8')
a = text.index('## PERF-043')
b = text.index('## PERF-042', a)
section = text[a:b]
section = section.replace(
    '현재 실행 전 명세이며 결과는 미측정이다.',
    '35회·5,580 GPU Step을 완료했고, 새 Meso 알고리즘의 필요성은 확인하지 못했다.',
)
section = section.replace('33회·5,100 GPU Step을 완료했고', '35회·5,580 GPU Step을 완료했고')
section = section.replace(
    '**계획 동결 · 빌드 중**', '**실행·분석 완료 · 새 알고리즘 기여 가설 지지 안 됨**'
)
section = (
    section[: section.index('### 결과·해석')] + result
    if '### 결과·해석' in section
    else section[: section.index('### 실행 결과')] + result
)
path.write_text(text[:a] + section + text[b:], encoding='utf-8')
study = read(STUDY / 'study.json')
study.update(
    status='completed',
    updated_at=stamp(),
    started_at=start,
    finished_at=end,
    successful_runs=(
        len(runs)
        if (runs := {r['id']: r for r in manifest['runs'] if r['status'] == 'completed'})
        else 0
    ),
    evaluation_gpu_steps=audit['evaluation_steps'],
    research_audit_errors=0,
    unit_test_failures=1,
    conclusion='Standard same-constraint LS reduces reconstruction error; downstream state differences small in tested cases; no demonstrated need for new meso transport algorithm',
)
study['experiments'][0]['status'] = 'completed'
write(STUDY / 'study.json', study)
print('PERF-043 report populated from raw results')
