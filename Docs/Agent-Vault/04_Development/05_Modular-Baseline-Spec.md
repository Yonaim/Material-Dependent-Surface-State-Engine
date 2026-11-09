# 모듈식 Baseline과 비교 과제 — v1

> **한 줄 요약:** 같은 국소 수송·반응·공유 형상을 합리적인 모듈식 조합으로 구현했을 때도 MDSSP의 결합·확장 이점이 유지되는지 평가한다. Baseline이 같은 구조로 자연스럽게 일반화되는 결과도 수용한다.

- 작성일: 2026-10-27. 상태: **설계 v1 고정 · Baseline 구현/실행 전**.
- 범위: 사용자가 요청한 Baseline 명세와 비교 과제 확정. 이 문서 작성으로 새 Solver나 실험 결과가 생긴 것은 아니다.
- 상위 명세: [[04_Development/04_System-Contribution-Protocol|시스템 기여 프로토콜]] §5–8. 이 문서가 후속 H3 비교의 구체적인 실행 계약이다.
- 기계 판독 명세: 저장소 루트 `Scripts/Research/Plans/modular-baseline-v1.json`. 현재 실행기가 직접 읽는 입력 파일은 아니며 구현 시 request로 변환해야 한다.
- 근거: `880af1ebcffe601f0bac27413cad94055ce92812` + 현 작업 트리. JSON의 `source_snapshot`에 확인한 파일 해시를 기록한다.
- 이미 알려진 결과: PERF-041의 판 A–F, Bunny, Mud 결과를 본 뒤 작성했다. 이 사례들을 새로운 blind/held-out 평가로 부르지 않는다. 새 T2/T3 역시 설계자가 내용을 아는 **사전 지정 확장 과제**이며 통계적 일반화 검증이 아니다.

## 1. 이번 비교가 답할 질문

> 동일한 현상학적 계산을 제공하는 일반 모듈식 조합과 비교해, MDSSP의 공통 계산 계약이 새로운 State·Reaction·Geometry 조합에 필요한 의미 있는 전용 결합 구현을 줄이는가?

일차 귀무가설은 **같은 공통 서비스를 갖춘 모듈식 시스템도 같은 결과와 비슷한 확장 부담을 제공한다**이다. MDSSP가 더 좋다고 가정하지 않는다. 이 비교는 Günther 등 특정 선행 시스템의 재현이 아니며 H4 신규성 판정을 대신하지 않는다.

코드 검토상 수송은 공유 `G_n`을 고정하면 채널별로 분리할 수 있다. Reaction은 그 뒤 공통으로 적용하고 Geometry는 최종 State에서 갱신한다. 따라서 모듈식 구현과 같은 이산 연산을 구성할 수 있다는 **강한 귀무 설명**이 이미 있다. 구현 실험은 이 동일성을 부정하려는 장치가 아니라, 실제 확장 부담과 추가로 필요한 계약이 있는지 확인하는 장치다.

## 2. 비교 대상과 실제 구조 차이

| 대상 | 모델 정의와 책임 | 역할 |
|---|---|---|
| U: MDSSP | 공통 State Registry/Profile, 공통 채널 수송 모델, 공통 Reaction과 형상 합성 | 현재 구현 |
| M: 모듈식 결합 | 현상 모듈이 자신의 State·수송 provider·형상 기여를 등록하고, 공통 조정자가 snapshot·반응·형상 갱신을 연결 | **주 Baseline** |
| M-separate | M의 transport를 별도 dispatch로 실행한 단순 schedule | 구현 검증/실행 스케줄 진단용. 이것만으로 성능 우위 주장 불가 |
| 독립 효과의 단순 중첩 | 공유 형상/반응 없는 별도 효과 | 주 비교에서 제외. PERF-041 A/B가 일부 역할을 이미 담당 |

M은 Ice/Waterfilm/Heat에 대해 각각 등록 가능한 모듈 단위를 가지되, 세 모듈이 **같은 LocalFlux provider/factory를 재사용해도 된다**. Heat에 불필요한 중력 계산이나 Ice에 불필요한 수송을 강제하지 않는다. Geometry와 Reaction을 각 모듈에 중복 구현하지 않는다.

### 공통으로 허용하는 기반

- 같은 OBJ/UV graph·seam 처리·고정 macro 면적·Profile loader·파라미터·입력장·초기화·관측 코드.
- 같은 연속/유입 Reaction evaluator, 공급량 경쟁, 내부 substep 및 diagnostics.
- 같은 Geometry 합성 함수·Normal/Concavity/Transfer Weight 계산과 GPU 자원 공유.
- 상태 ID/공통 배열/같은 메모리 배치, 동일 직렬화 형식. 소유권이 모듈에 있다는 이유로 복사·readback을 강제하지 않는다.
- 공통 수송 수식의 함수·템플릿 재사용, 데이터 기반 파라미터와 규칙, 범용 모듈 factory.
- kernel batching/fusion·공통 읽기 재사용·캐시·불필요한 계산 생략. 모듈 수만큼 dispatch하도록 제한하지 않는다.

반대로 모듈이라는 이름만 붙인 채 `TSurfaceStateSolver::RecordStep(s)`를 그대로 호출하는 구현은 **별도 계산 구조 비교가 아니라 wrapper 대조**로 분류한다. 그러나 일반적인 helper/factory와 batch 구성을 적용한 결과 U와 같아졌다면 Baseline을 탈락시키지 않는다. **그 수렴 자체가 H3를 지지하지 않는 증거다.** 같은 구조를 억지로 다르게 만들기 위한 제약은 추가하지 않는다.

### M의 최소 인터페이스 — 설계이며 미구현

```text
ModuleDescriptor:
  state key / supported profiles / parameter view
  transport provider and channels it writes
  height contribution descriptor
  required shared fields and input/actual-incoming outputs

Coordinator:
  bind immutable State_n and Geometry_n views
  execute registered transport providers -> TransportState, PositiveInput, Incoming
  evaluate all reactions together -> State_n+1
  compose contributions and rebuild geometry -> Geometry_n+1
  publish/swap all fields together
```

기여 등록 1개와 Profile entry 1개는 모두 선언 비용으로 센다. M에만 별도 수동 연결을 강요하지 않는다. 인터페이스가 누락되어 새 범용 기능을 추가해야 했다면 그 변경을 기반/한계비용에 기록하고, 이후 과제에서 재사용을 허용한다.

## 3. 반드시 같은 계산을 수행할 계약

texel i, 채널 k에 대해 `S_n`, `G_n`, 사건 입력 `I_n`을 사용한다. 수송 원시 요청 `q_ij^k`와 Decay `d_i^k`는 현행 함수 그대로다.

```text
a_i^k = sum(q_ij^k)>0 ? min(1, max(S_n[i,k]-d_i^k,0)/sum(q_ij^k)) : 1
J_i^k = sum_j(a_j^k * q_ji^k)
T_i^k = max(0, S_n[i,k] + I_n[i,k] + J_i^k - a_i^k*sum_j(q_ij^k) - d_i^k)
S_n+1 = Reaction(T, max(I_n,0), J, dt)
G_n+1 = Geometry(S_n+1, profiles)
```

- Pass 1의 saturation은 **Decay 전** `S_n / Capacity`이며 outgoing 가용량에 Decay를 반영한다. 같은 input을 먼저 넣어 수송시키지 않는다.
- 모든 모듈은 `S_n`, `G_n`을 읽는다. 먼저 계산한 모듈의 새 State/Geometry를 다음 모듈에 노출하지 않는다. 받은 양과 생성물은 다음 전역 Step부터 이동한다.
- 모든 수송 결과와 실제 Incoming이 준비된 후 공통 Reaction을 한 번 호출한다. 모듈별 반응을 순서대로 적용해 공급량 경쟁을 바꾸지 않는다.
- `inflowFraction`은 positive input/실제 incoming에 한 번 적용하고 continuous 적분보다 먼저 처리한다. 매 substep 중복 적용하지 않는다.
- Geometry는 모든 최종 State 기여를 합성한다. 채널 수나 등록 순서가 합성 대상의 누락·중복을 만들면 안 된다.
- E에서는 source/target 위치·Normal·Concavity·가중치 모두 G0. 표시용 최신 Height가 Solver에 누출되면 안 된다.
- State 총량·면적·Capacity와 축적 clamp 계약은 현행 그대로다. 다른 물리 Solver와의 정확도 경쟁은 이번 질문에서 제외한다.

M의 실행 schedule은 위 의존성을 지키는 한 자유다. M-separate와 batch/fused M의 수량 동등성을 확인한 뒤 유효한 M 중 비용이 낮은 구성을 주 비교로 쓴다. 코드 공유로 결과가 일치한다면 그 사실과 공유 범위를 공개한다. 이를 독립적인 수학적 검증이라고 부르지 않는다.

## 4. 비교 과제와 범위

기본 판·입력·Profile·14초·128/60 Hz는 PERF-041 plan과 동일하다. 공통 seed/bootstrap 후 실제 GPU S0를 기준으로 양쪽을 초기화한다. 기본 수치 경로는 **Dense Solver + 매 Step 전체 Geometry + FP32 Raw Flux/Weights**, Adaptive·채널 생략·FP16·Sparse OFF다. 이전 Sparse 실행 시간을 직접 성능 기준으로 재사용하지 않는다.

| 과제 | 출발점과 변경 | 고정할 내용 | 주 관측 |
|---|---|---|---|
| T0 기존 모델 재현 | Ice/Waterfilm/Heat; B,D,E | 현재 Profile·입력·형상·dt | 계산/초기화 동등성, 기반 구현량 |
| T1 State 추가 | T0-D에 기존 Mud 추가 | PERF-041 Mud의 동일 3개 Cavity 계수 0·초기 분포, 기존 3 State/Reaction 유지 | 새 State·형상 기여 등록의 한계비용 |
| T2 유입 Reaction 추가 | T0-D에 `wetness`와 물막 유입의 10% 전환 추가 | 아래 수치, 기존 열/융해 규칙 유지 | 신규 유입 provenance 전달, 규칙/State 추가 비용 |
| T3 조합·변환 | T1+T2를 함께 적용한 판, X 회전 20도; D,E | 같은 계수·입력 local 좌표·scale, 초기 wetness 0 | 조합 시 예외 처리·재조정, 공유 형상 계약 |

최소 진단 행렬은 **7 task/variant × 2 구현 = 14 trajectory**다. 최종 M이 M-separate와 다른 경우 M-separate T0-D 1회 및 ledger OFF의 U/M T0-D 1초 대조 2회를 추가한다. 등록 순서는 각 방식에서 정순/역순 1초 대조를 T3-D에 적용하고 채널 key로 정렬해 비교한다. 특정 모듈 실행 순서에 의존하는 문제를 찾기 위한 검사이며 별도 품질 사례 수로 세지 않는다.

### T2의 새 설정 — 지원된 기능을 사용하는 제안, 아직 실행하지 않음

`wetness`: Capacity 1, inputFactor 1, saturationSpread/gravityFlow/decay/accumulation/cavity 및 두 retention 계수 0, thicknessPerAmount .025. 초기/외부 입력 0. 저장 상한 없는 State다.

```json
{
  "id": "surface_absorption",
  "mode": "inflowFraction",
  "inflowState": "waterfilm",
  "inflowSources": ["positiveInput", "incoming"],
  "fraction": 0.1,
  "consumes": {"waterfilm": 1},
  "produces": {"wetness": 1}
}
```

이것은 기존 Reaction 기능으로 정의하는 현상학적 흡수 과제다. 새 물리 모델이나 시각적 젖음 구현이 아니다. 동작/설정은 양쪽에 동일하게 적용한다. `Ice + Waterfilm + Wetness`는 해당 전환에서 보존되고 T3에서는 Mud의 별도 Decay를 장부에 반영한다. 아직 T2/T3를 지원하는 연구용 request 변환 코드·설정 에셋은 작성하지 않았다.

Bunny는 주 비교에서 제외한다. 이유는 이미 알려진 seam 누락이며, 새 Baseline 결과를 보고 제외한 것이 아니다. 별도 매핑 검증 뒤 보조 사례로 추가하려면 v2로 기록한다. T3는 다른 Mesh 일반화가 아니라 같은 graph의 구성·transform 재사용 검사다.

## 5. 동등성 gate — 성능/부담 우열 해석의 선행 조건

다음은 새 **동일 모델 구현 비교**의 공학적 허용 범위다. 물리적 정답·지각 동등성·SIGGRAPH 합격선이 아니다. 같은 정밀도/graph이므로 bit equality를 먼저 보고한다. 실패하면 아래 오차도 기록한다.

- 정규화 `Z_heat = 초기 Heat + 전체 positive Heat input`. 비열 State 공통 `Z_material = 초기 Ice/Waterfilm/Mud/Wetness 합 + 해당 State들의 전체 positive input`. 생산량을 분모로 더하지 않는다.
- snapshot `t=0,1,2,4,8,14초`의 채널별 `Σ|U−M| / Z`가 **1e-5 이하**. 0 분모는 1e-20으로 보호하고 원 절대량을 함께 보고한다.
- snapshot의 최대 기준 면적당 State 차이는 `1e-4 + 1e-5 × max(|density_U|, |density_M|)` 이하. `density=S/AreaScale`, 양의 면적 texel만 검사한다. 국소 큰 차이를 전체 L1로 숨기지 않는다.
- snapshot의 월드 위치 최대 차이 `1e-5 × L` 이하(`L=max(초기 월드 bbox 대각 길이,1 world unit)`), Normal 벡터 차이 1e-4 이하, 간선 Weight 차이 2e-5 이하. E는 자기 G0의 cache와 비트 동일해야 한다.
- 최종 누적 경계 순수송·Decay·Rule extent의 U/M 차이를 위 Z로 정규화해 1e-5 이하. 표본 시점뿐 아니라 840 Step 전체 지역/채널 수지·오류 장부를 검사한다.
- 음수/NaN/Inf·반응 예산/산술 오류·동일 Step/입력 중복·E 고정 위반 0. 각 실행의 관련 물질 장부 상대 잔차 1e-5 이하.
- 시작 State·입력 일정·geometry hash는 같은 task/variant의 U/M에서 일치해야 한다. T0의 B와 D/E 형상은 의도적으로 다르다.

하나라도 실패하면 **현재 비교 무효 또는 품질 미달**로 기록한다. 수식을 고치거나 threshold를 느슨하게 한 결과를 같은 v1 결과로 덮어쓰지 않는다. 구현 버그 수정 후 같은 명세로 재실행할 수 있고, 모델/판정 변경이면 v2 및 이전 실패를 남긴다. Baseline 실패를 MDSSP의 구조적 우수성으로 세지 않는다.

## 6. 확장 부담의 측정과 공정성

양쪽 모두 동일하게 제공받는 기반을 `shared`로 공개한다. U에 이미 존재하는 엔진 개발 비용을 0으로 간주하면서 M의 전체 신규 엔진 비용과 비교하지 않는다. 아래를 구분한다.

| 장부 | 기록할 것 | 주 해석 |
|---|---|---|
| 공통 제공 기반 | loader, graph, GPU 자원, 수식 helper, Reaction/Geometry 서비스, 계측 | 차별화 증거에서 제외 |
| T0 기반 구축 | U 실험 adapter와 M 모듈/조정자·packing·schedule 구축 | 일회 비용. 기존 U의 역사적 총 개발 시간은 측정 불가 |
| T1/T2/T3 한계비용 | 직전 tag에서 필요한 의미 있는 수정, 선언·adapter·공통 기반 확장·전용 결합·회귀 수정 | 일차 비교 |
| 최종 전체 시스템 | 모든 과제 후 범용화된 구현과 재적용한 이전 과제 | 첫 미숙한 M 구현만 비교하는 것을 방지 |

한계비용은 T1과 T2가 각각 T0에서 분기하고, T3는 두 변경의 합성을 기준으로 한다. T2의 기반을 T1 비용에 감추거나, T3에 T1/T2 구현량을 다시 더하지 않는다. 모든 과제 전후 diff·실패·재조정·재검증을 보존한다.

각 변경은 다음으로 분류하되 줄 수의 단일 점수로 합치지 않는다.

1. 선언: Profile/Reaction/module registration/입력 설정. 형식이 다르다는 이유만으로 M을 불리하게 세지 않는다.
2. 범용 기반 확장: 새 인터페이스·필드·factory 등. 한 번 도입한 helper는 이후 과제에 재사용한다.
3. 과제 전용 결합: 특정 State 쌍/조합을 위한 branch, adapter, 순서 조건, 예외. 실제 계산 의미를 설명한다.
4. 핵심 수식/공급량 경쟁/형상 계산 변경: 변경 이유와 새 회귀 위험을 기록한다.
5. 오류 수정과 생성 boilerplate: 별도 기록. 타이핑 실수·파일 수·자동 생성 줄 수를 계산 모델의 우수성으로 해석하지 않는다.

작업 시간·AI 호출/실패 횟수는 탐색 로그로만 쓴다. 작성자가 U에 더 익숙하고 양쪽 내용을 알고 있으므로 인간 생산성·학습 용이성의 주 증거로 쓰지 않는다. UI·완성된 외관·게임 엔진 전체 통합 비용은 이번 범위에서 제외한다.

## 7. 비용 비교 — 품질 동등성 후에만 실행

- 주 성능 조건: T0-D와 T3-D, 128 및 256, 60 Hz, 14초. 같은 Dense FP32·매 Step Geometry 기준. M-separate가 아니라 허용한 batching/재사용을 적용한 유효 M을 U와 비교한다.
- 조건당 독립 실행 U 5회·M 5회. 짝 순서는 `U/M, M/U, U/M, M/U, U/M`으로 고정한다. GPU 동시 실행 금지.
- 각 실행은 pipeline 준비 후 같은 조건의 1초 scratch 실행으로 준비하고, 초기 State·G0·입력 큐·시간·마스크/누적 장부를 모두 초기화한다. 준비 결과를 본 측정에 이어 쓰지 않는다.
- 측정은 840 Step 전부. GPU timestamp가 Transport/Reaction/Height/Geometry/Weights와 필요한 GPU packing을 포함한다. CPU coordinator 기록 시간·setup/compile·할당량은 별도 보고한다.
- 전체 Step별 readback/ledger/그림 저장은 비용 실행에서 OFF. 두 방식에 같은 설정을 사용한다. 최종 State는 측정 구간 밖에서 받고 진단 결과와 gate를 비교한다.
- 반복별 Simulation GPU 중앙값·p95와 반복 간 범위, CPU 제출/record 비용, 실제 할당 buffer byte 및 peak 임시 공간을 보고한다. GPU별 VRAM 상주량과 혼동하지 않는다.
- M의 자원 변환·packing은 필요한 만큼 비용에 포함한다. 양쪽에 불필요한 변환을 강제하지 않는다. 빠른 경로가 품질 gate에 실패하면 속도 우열에서 제외한다.

이것은 headless simulation 비용 비교다. Interactive 앱 frame budget·렌더링·제작 생산성의 검증으로 대체하지 않는다. 새 실행의 성능 기록은 기존 측정 규칙에 따라 baseline 폴더와 새 PERF ID를 부여하며, 이번 문서 작성만으로 측정 기록을 만들지 않는다.

## 8. Go / No-Go와 중단 기준

| 관찰 | 사전 판정 |
|---|---|
| M의 동등한 연산 분해가 정의되지 않거나 gate 실패 | 구현/비교 준비 미완료. H3의 승리로 취급하지 않음 |
| M의 일반 factory·공통 Geometry/Reaction으로 T1/T2/T3가 선언만으로 구성됨 | **이 과제에서 H3 지지 안 됨**. 더 불리한 M으로 교체하지 않음 |
| 차이가 파일 배치·registration 문법·dispatch 개수뿐 | **구조적 Contribution 근거로 사용하지 않음** |
| M-separate만 느리고 합리적인 batch M이 차이를 제거 | 스케줄 구현 차이. 공통 모델의 연구 우위 주장 중단 |
| T1과 T2라는 서로 다른 변경 종류에서 U는 선언으로 끝나고, 충분히 재사용한 M에도 계산 의미를 가진 전용 결합/핵심 변경이 남으며 T3에서 재현됨 | H3를 더 연구할 **잠정 Go**. 왜 M에 그 변경이 불가피한지 구체적 계약으로 설명해야 함 |
| M의 범용 인터페이스를 보완하자 위 차이가 사라짐 | 최종 구현 기준 H3 지지 안 됨. 초기 불완전 M만 결과로 내세우지 않음 |
| 차이는 실행 시간뿐 | 성능 결과로 보고. 그 자체를 새 시스템 모델/신규성으로 올리지 않음 |
| U/M가 같은 모델·확장 부담·실용 비용을 제공 | **현재 시스템 우위 주장 중단/축소**. H1 기전 검증과 엔진의 가치는 별개 |

서로 다른 두 확장 종류와 합성 과제는 내부의 최소 탐색 기준이며 통계적 일반성·논문 채택 기준이 아니다. M에 새 수송 모델을 허용하고 U에는 허용하지 않는 식의 비대칭도 만들지 않는다. 이번 범위를 벗어난 모델 요구가 발견되면 별도 질문으로 기록한다.

중단 예산: T0는 **동등성 구현 1개와 합리적인 batch/재사용 보완 1회**를 우선한다. 이 보완은 동일 수식·scope 안에서 불필요한 복사/dispatch/특례를 제거하는 단위이며 모든 수정/실패를 남긴다. T1–T3 결과를 보고 M의 허용 기능을 줄이거나 유리한 새 장면으로 교체하지 않는다. 귀무 설명이 설계 대응표만으로 성립하면 대규모 성능 실행 전에 그 사실을 보고하고 최소 재현 구현의 필요성을 재판정한다.

## 9. 코드 대응표와 구현 착수 조건

| 계산 계약 | 현행 근거 | M에서 대응할 것 |
|---|---|---|
| 채널별 수송·limiter | `SurfaceSolverPass1.comp::main`, `Transport.glsl::rawFlux` | 모듈 provider의 source 요청/alpha |
| Incoming/Input/Decay | `SurfaceSolverPass2.comp::main` | 실제 Incoming·positive input·중간 State를 공통 Reaction에 노출 |
| 같은 snapshot의 공급 경쟁 | `SurfaceReaction.comp::reactionRequest/main` | 같은 공통 evaluator 재사용 가능 |
| State→Height | `StateMath.glsl::accumulationHeight` | 등록한 기여 전체 합성, 같은 clamp와 환산 |
| 순서/형상 갱신 | `SurfaceStateSolver.cpp::RecordSteps` | 같은 의존성을 가진 모듈 조정자. 그대로 호출하면 wrapper로 표기 |
| 입력/초기화·계측 | `Scripts/Research/CouplingStudy.cpp::main` | 공통 입력 및 검증 부분 재사용, 구현 선택 지점 분리 |
| 유입 전환 지원 | `SurfaceReactions.cpp::ValidateSurfaceReactions`, `SurfaceGPUResourceTests.cpp` 유입 테스트 | T2는 새 알고리즘이 아닌 지원된 기능의 새 설정 |

현재 구현 상태: M 인터페이스·request 변환·T2/T3 설정 에셋·비교 실행기는 **미구현**이다. 이 명세와 JSON은 설계 산출물이다. 이후 실행 전 실제 source/config 해시, 적용한 M schedule, 공유 코드 범위와 최적화 차이를 manifest에 고정한다. 변경은 사유와 시점을 남기며 새 결과를 보며 v1을 소급 수정하지 않는다.
