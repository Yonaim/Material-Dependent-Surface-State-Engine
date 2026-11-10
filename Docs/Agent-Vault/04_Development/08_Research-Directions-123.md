# 후속 연구 방향 1·2·3 — 문제 정식화와 비판적 검토

> **보존한 최초 통합 검토:** 사용자 요청으로 연구 기록을 [[06_Research/00_Overview|05_Research]]의 주제별 문서로 분리했다. 이 파일의 기존 내용은 당시 분석으로 유지하고, 이후 연구는 새 폴더를 정본으로 갱신한다.

작성: **2026-10-27, Asia/Seoul**. 사용자 요청: 이전 제안의 4번(축적 순서/층 표현)을 제외하고 1·2·3번을 진지하게 검토한다.

상태: **문헌·코드·수식 분석 완료, 신규 알고리즘·실험은 미구현/미실행**. 로컬 앱·GPU·빌드·벤치마크 실행 중지 요청을 유지한다. 이 작업은 가벼운 코드/문서 읽기, 온라인 문헌 조사와 연구 명세 작성이다. 현재 기여 판정을 상향하는 실측 결과가 아니다.

## 1. 판단과 우선순위

| 방향 | 이번에 좁힌 질문 | 이미 알려진 부분 | 남은 연구 후보 | 권고 |
|---|---|---|---|---|
| 1 목표 진행·후속 반응 역설계 | 겉보기 목표와 지정된 후속 입력 반응을 함께 만족하는 허용 입력을 찾는가? | 역설계, 시공간 목표, adjoint/자동미분, 다중 목적 최적화 | 관측만으로 구별 안 되는 해를 후속 반응 관점에서 구분하고 안정적으로 제어하는 특화 방법 | **단독 Contribution 주장은 약함. 2의 과제·평가 체계로 결합** |
| 2 다른 현상을 보존하는 국소 수정 | 지정 효과를 바꾸면서 보호한 State/진행/후속 반응의 변화는 제한하는가? | 국소 시뮬레이션 편집, 제약 최적화, 시공간 subspace | 허용 조작이 제한된 다중 State–Reaction–Geometry 연산에서 유효한 수정 방향과 보상 범위를 찾는 방법 | **프로젝트 목적에 맞는 주력 탐색. 일반 최적화 대조부터** |
| 3 혼합·수송 정보를 유지하는 coarse 표현 | 평균량만으로 사라지는 반응/경로 정보를 작은 추가 상태로 유지하는가? | 비선형 평균화의 closure 문제, covariance 보정, subgrid 지형 | 변하는 표면 형상과 반응이 함께 있는 제한된 모델군의 compact closure·갱신법 | **구체적 수학 단서가 가장 강함. 알고리즘 대안으로 별도 탐색** |

세 방향 모두 아직 신규 해법을 발견한 것은 아니다. 1+2의 목적 적합성과 3의 문제 명확성을 구분한다. 세 개를 한 논문에 모두 넣는 계획은 권하지 않는다. 3이 1+2의 역설계 가속에 유용할 수는 있지만 그 연결을 초기 필수 범위로 만들지 않는다.

현재 구현의 판정은 [[06_Prior-Model-Differentiation|기존 연구의 시스템 통합에 가깝다]]를 유지한다. 중단된 AUTH-GOAL-01은 10초까지만 관측했고 완료 여부 미평가다. 그 과제를 제작 난제의 실증 반례로 사용하지 않는다.

## 2. 근거 수준과 접근 범위

- **[C] 코드:** 현 작업 트리에서 실제 함수·식 확인.
- **[E] 기존 실행:** PERF-041/042/043, 정상 6조건 검증, 계측 대조의 범위 안에서만 사용.
- **[P] 문헌:** 아래 출처 표에 적힌 관련 절/텍스트만 확인. 초록·검색 색인과 원문 절 읽기를 구분.
- **[D] 분석:** 코드 식에서 유도한 성질·반례. 새로운 정리의 독창성이나 GPU 재현 결과가 아님.
- **[H] 가설:** 후속 실험이 필요한 연구 제안.

문헌 조사는 표적 비교다. ‘세계 최초’, 지정 문헌의 모든 확장 가능성, 조사하지 않은 2026 논문의 부재를 증명하지 않는다. 온라인 PDF의 열람 실패·크기 제한 때문에 모든 논문 전체를 정독했다고 쓰지 않는다. 논문 원문은 저장소에 재배포하지 않는다.

## 3. 세 방향이 공유하는 현재 계산 계약

표면 graph를 고정하고 State 총량을 q, Profile/재질 계수를 θ, 사건 입력을 u, 파생 형상을 G라 한다. Dense·고정 시간 간격 경로를 연구 기준으로 삼으면 개념적으로 다음과 같다.

\[
q_{n+1}=\mathcal R_\theta(\mathcal T_\theta(q_n,G_n,u_n),\;\text{positiveInput}_n,\text{incoming}_n),
\qquad G_{n+1}=\mathcal B_\theta(q_{n+1}).
\]

`T`에는 Input/Transport/Decay, `R`에는 유입 비율 전환과 연속 반응, `B`에는 Height/Normal/Concavity/Weights가 포함된다. 실제 cache·scheduling의 의미는 동일 forward 경로로 검증해야 한다. 모든 GPU scratch가 수학적 물질 상태인 것은 아니다.

| 기능 | [C] 근거 | 1·2·3에 주는 조건 |
|---|---|---|
| 포화도·형상 기반 국소 flux | `Shaders/Simulation/Solver/Transport.glsl::rawFlux` | 양의 부분, cavity 저항 clamp, source/target 의존성이 존재 |
| outgoing 공급량 제한 | `SurfaceSolverPass1.comp::main` | `min`/분기에서 미분 불연속 또는 미분 불능 가능 |
| 실제 incoming·입력 장부 | `SurfaceSolverPass2.comp::main`, ReactionScratch plane | Reaction 생성물과 이웃 유입은 의미가 다름 |
| 선언 반응·경쟁·시간 세분화 | `SurfaceReaction.comp::reactionRequest/main` | threshold, supply limiter, 내부 반복 횟수 변화까지 forward 의미에 포함 |
| State→형상 | `StateMath.glsl::accumulationHeight/effectiveLocalPosition/effectiveLocalNormal` | 형상 기여의 Capacity clamp와 저장 State의 무상한 계약을 구분 |
| 형상→다음 수송 | `SurfaceDynamicGeometryUpdate.comp::main`, `SurfaceDynamicWeightsUpdate.comp::buildTransferWeight` | 목표 State를 수정하면 다른 채널의 전파도 달라질 수 있음 |
| 전체 기록 순서 | `Source/SurfaceState/Solver/SurfaceStateSolver.cpp::RecordSteps` | 다중 State·Reaction·Geometry 연산의 전/후 순서를 보존해야 함 |
| 화면 관측 | `Shaders/Rendering/Surface/Lighting/SurfaceLit.glsl`, `StateTexture/StateSampling.glsl` | 표시 clamp·재질 합성 때문에 관측이 State를 유일하게 정하지 않을 가능성. 실제 장면 반례는 미검증 |
| 입력·최소 계측 | `Contact/SurfaceContactApplication.cpp::ApplyPendingContacts`, `Rendering/Profiling/RendererBenchmark.cpp::WriteAuthoringCheckpoint` | 접촉 입력·같은 Step readback 재사용 가능. 미분·역문제용 자동 반복 인터페이스는 구현한 바 없음 |

[E] PERF-041의 한 판에서 Geometry D/E가 분포를 바꾼 것은 결합 경로가 작동한다는 증거다. 그 값이 gradient에서 중요하거나 역설계를 어렵게 한다는 증거는 아니다. PERF-042는 동일 수치 모델 내부 조정 구조의 확장 부담 우위를 지지하지 않았다. 이 결과를 새 최적화 방법의 Baseline 성능으로 재사용하지 않는다.

## 4. 방향 1 — 목표 진행과 후속 반응을 함께 만족하는 역설계

### 4.1 문제를 더 좁히기

사진 한 장에서 실제 물리 파라미터를 복원하는 문제로 시작하지 않는다. 현재 모델의 초기 상태/재질을 알고, 제작자가 원하는 시점별 지표와 후속 반응을 지정하는 **설계 문제**로 둔다. 유일한 진실의 State를 복원한다고 주장하지 않는다.

예: 3초의 잔존 Ice, 8초의 Waterfilm 도달 영역, 14초의 잔류 흔적을 맞추고, 이후 고정된 Heat pulse에 대한 반응 곡선도 지정한다. 시점/수치는 설명 예시이며 기존 AUTH-GOAL gate를 변경하는 실행 명세가 아니다.

허용 조작 a를 정하고 원래 forward rollout을 \(\Phi(a)\), 관측을 H, 후속 시험 입력 p에 대한 반응을 \(\Psi_p(a)\)라 하면 다음과 같다.

\[
\min_{a\in\mathcal U}\; L_{\mathrm{progress}}(H\Phi(a),y^*)
  +\sum_{p\in\mathcal P_{\rm train}} w_p L_{\mathrm{response}}(\Psi_p(a),r_p^*)
  +\lambda\Omega(a).
\]

이 목적함수와 regularization 자체는 표준적인 최적제어 형식이다. 새 기여로 세지 않는다. 실제 후보는 **관측에 비슷하게 맞는 여러 해 사이의 후속 거동 차이를, 제한된 제작 조작 아래 효율적으로 줄이는 방법**이어야 한다.

### 4.2 먼저 고정할 조작 범위

1. **U-input:** 재질/반응 inventory·초기 State를 고정하고, 정한 source 영역과 시간 basis의 비음수 Heat/Waterfilm 입력만 조절. source별 총량·세기·시간·공간 support에 bounds를 둔다. 잔류 State 직접 칠하기는 허용하지 않는다.
2. **U-profile:** 별도 과제로 제한된 2–6개 Profile 계수까지 허용. 반응 삭제·계수 0으로 무력화·장면별 새 Rule 삽입으로 목표를 우회하지 않도록 범위를 미리 정한다.

두 범위의 결과를 섞지 않는다. 모든 texel·채널에 자유로운 양/음의 source를 주는 방법은 별도 강한 상한 대조가 될 수 있지만 U-input 과제의 공정한 주 비교군으로 두지 않는다.

### 4.3 핵심 가설과 이미 알려진 부분

[H] 비슷한 외관/진행 목표를 만족하는 두 허용 rollout이 후속 입력에서 서로 다른 결과를 낼 수 있고, 이 차이가 제작상 중요할 수 있다. **현재 MDSSP 장면에서 이 쌍을 확보한 것은 아니다.**

[D] 선형화한 관측 Jacobian \(J_y\)에 대해 \(J_y\delta a\simeq0\)이나 후속 반응의 \(J_\Psi\delta a\)는 큰 방향이 있다면, 관측만의 적합도는 그 방향을 충분히 제어하지 못한다. 이는 표준적인 관측/식별성의 국소 설명이며, 비선형 전체 공간의 비식별성 증명이 아니다.

[P] Bosch 2011은 사진에서 flow parameters/detail maps를 추출해 새 target에 적용한다. Mordvintsev 2021은 RD update의 파라미터를 texture loss로 학습하고 mesh/해상도 전이도 보여준다. 따라서 ‘풍화 파라미터 자동 탐색’, ‘RD를 미분해 학습’, ‘다른 mesh에 적용’은 빈 영역이 아니다. 관련 출처 [R1–R3].

### 4.4 최소 진단과 Baseline

| 진단 | 수행할 내용 | 필요한 해석 |
|---|---|---|
| Q1-A 관측 모호성 | 같은 허용 조작 범위에서 관측 오차가 ε 이내인 여러 해를 찾고 고정 후속 pulse 결과를 비교 | 차이가 관측/수치 반복 오차보다 작으면 이 문제 설정의 필요성 약함 |
| Q1-B 추가 제약의 효과 | 관측-only, 관측+시점별 State 지표, 관측+후속 반응을 같은 optimizer로 비교 | loss 추가의 유용성과 새 알고리즘의 우위를 구분 |
| Q1-C 일반화 | fit에 쓴 pulse와 크기/위치/시간이 다른 미사용 pulse에서 평가 | train pulse에 맞춘 해를 모든 미래의 보장으로 확대하지 않음 |

기본 대조는 bounded CMA-ES 또는 다른 derivative-free 방법과 finite-difference+SQP/trust-region이다. 미분 가능한 경로를 만들 경우 full-coupling AD/adjoint를 강한 기준으로 포함한다. 평가는 같은 target/허용 조작/초기화 seed/rollout budget에서 한다. derivative-free 평가 호출 비용을 누락하지 않는다. optimizer를 임의로 약하게 설정해 새 방법을 유리하게 만들지 않는다.

### 4.5 추가 연구가 필요한 경우의 해법 후보

[H] 관측과 후속 반응 민감도를 분리한 control basis, 반응 threshold 근처의 단계적 최적화, surrogate 제안 후 실제 forward 검증을 결합할 수 있다. 각각 기존 개념이며 **MDSSP 결합 구조에서 standard baseline이 실패하는 이유와 해결 효과**가 있어야 새 방법이 된다.

지금 바로 전체 renderer/solver를 미분 가능한 새 엔진으로 재작성하지 않는다. 첫 진단은 적은 수의 입력 basis와 state-space 목표로 가능하다. 실제 구현/실행은 별도 재개 지시 전 하지 않는다.

**지표:** 진행 오차, 미사용 pulse 응답의 평균/최악 오차, 목표 동시 충족률, 입력 총량/공간·시간 복잡도, 실패율, forward 호출 수. 완성된 rendered 과제에서는 고정 camera/lighting의 외관도 확인한다. State 지표만 맞춘 결과를 예술적 품질로 대체하지 않는다.

**중단 기준:** 모호성이 중요하지 않음 / 단순 다중 목적 최적화가 충분함 / 추가 이점이 큰 모델 변경이나 목표 편향에 의존함. 이 경우 독립 논문 Contribution 주장은 중단하고 2의 평가 과제로만 사용한다.

## 5. 방향 2 — 다른 현상을 보존하는 국소 수정

### 5.1 핵심 Research Question

> 허용된 입력/소수 계수만 바꿔 지정한 잔류 흔적을 수정하면서, 보호한 영역의 융해 진행과 후속 반응의 변화를 제한할 수 있는가? 필요한 보상 조작의 범위를 어떻게 선택하는가?

국소라는 말은 **목표 영역**, **조작 영역**, **재계산 영역**을 각각 뜻할 수 있다. 세 영역을 동일하다고 가정하지 않는다. 목표가 국소여도 보상 Heat 입력은 다른 곳/이전 시간에 필요할 수 있다. 보호된 지표가 만족되지 않으면 ‘다른 State를 그대로 유지했다’고 쓰지 않는다.

### 5.2 제약식과 어려움

기존 입력을 a₀, 목표 편집 지표를 E, 보호 지표를 P라 하면:

\[
\min_{\delta a}\;\|E\Phi(a_0+\delta a)-y^*\|^2+\lambda\|\delta a\|_W^2,
\quad \|P\Phi(a_0+\delta a)-P\Phi(a_0)\|\le\epsilon_P,
\quad a_0+\delta a\in\mathcal U.
\]

보호 대상은 초기에는 소수 영역의 Heat/Ice 총량 곡선·후속 pulse 응답처럼 유한한 지표다. 전체 State의 모든 시점을 완전히 고정하면서 연결된 일부 State를 자유롭게 변경할 수 있다고 요구하지 않는다.

[D] Dense 수학 경로에서 \(G=B(q)\), \(q'=F(q,G,a)\)이면 국소 민감도는 개념적으로 \(F_q+F_G B_q\)다. Geometry 경로를 빼면 Heat처럼 축적 계수가 0인 채널도 영향을 받을 가능성을 누락한다. 이 chain rule 자체는 새 이론이 아니다. 실제 bootstrap/이전 Geometry 의존성은 전체 상태·연산 순서로 포함해야 한다.

[C] `max` flux, outgoing limiter, 반응 threshold, supply competition, 내부 시간 세분화, 형상 Capacity clamp가 있다. AD를 연결하면 모든 분기에서 쓸 만한 gradient가 자동으로 생기는 것은 아니다. [P] TeG는 불연속 미분이 이미 별도 연구 분야임을 보여준다 [R8].

### 5.3 논문으로 발전시킬 수 있는 기술적 가설

[H] **반응·형상 경로를 포함한 보호 민감도로 수정 방향을 제한하고, 실제 forward 오차를 확인하면서 조작 영역을 확장하는 방법**이, 고정 반경 편집 또는 전역의 모든 조작을 쓰는 방법보다 같은 edit/protection 품질에서 유용한 trade-off를 줄 수 있다.

첫 알고리즘 초안은 다음 정도로 제한한다.

1. 기존 rollout과 입력 basis에서 \(J_E,J_P\)를 추정한다. 처음에는 finite difference도 허용한다.
2. 작은 편집은 \(J_P\delta a\simeq0\)인 방향에서 constrained least-squares/trust-region으로 제안한다.
3. 원래 production 의미의 full-coupling forward로 edit/protection·비음수·수지를 검증한다.
4. 선형화 예측이 빗나가면 trust region을 줄이고 active regime를 갱신한다. 조작 범위가 부족하면 사전에 허용한 이웃/이전 시간 basis로 확장한다.
5. 더 넓혀도 찾지 못하면 미달/국소 탐색 실패와 완화 선택지를 보고한다. 전역 불가능 인증으로 표현하지 않는다.

**여기서 1–5는 기존 기법의 합성 초안이다.** 새 알고리즘으로 확정하지 않는다. full Jacobian으로 같은 계산을 하는 전역 SQP가 충분히 좋으면 특화 방법을 만들 이유가 없다. finite-difference 민감도 구축 비용·다중 forward·캐시 구축을 모두 비용에 포함한다.

### 5.4 국소 선형 실행 가능성과 전역 불가능의 구분

[D] bound가 활성화되지 않은 선형화에서 N을 \(\ker J_P\)의 basis라 하면, 목표 변화 d가 \(\mathrm{range}(J_E N)\) 안에 있는지가 보호를 유지하는 작은 수정의 조건이다. nullspace나 이 조건 자체는 표준 선형대수다. d가 range 밖이라고 비선형 유한 편집까지 불가능한 것은 아니다. threshold를 건너는 해결책을 현재 Jacobian이 못 볼 수도 있다.

별도의 **정확히 불가능한 단위 과제**는 물질 수지로 만들 수 있다. 닫힌 graph, 물질 외부 입출력·Decay 없음, Ice↔Waterfilm/Waterfilm→AbsorbedWetness만 있는 경우 \(M=I+W+A\)는 보존된다(Heat는 이 합에 넣지 않는다). I 총량과 W 총량도 같게 유지하면서 A 총량만 줄이기는 불가능하다. 이는 통상적인 보존식의 적용이며 새 정리가 아니다. 비현실적인 과제를 optimizer의 실패로 오인하지 않는 sanity case로 사용한다. 외부 sink/입력 또는 다른 반응이 있으면 이 인증을 그대로 쓸 수 없다.

### 5.5 가까운 연구와 실질적 차이의 후보

- [P] Pan 2013: 국소 keyframe을 volume-preserving 기하 변형으로 편집하고 주변 시간·후속 시뮬레이션으로 전달 [R4]. 국소성·시공간 전파 자체는 기존이다.
- [P] Barbič 2012: 기존 세부 운동을 보존하는 reduced spacetime edit와 contact constraints. 관련 절은 모든 force를 허용하는 fully actuated 문제와 제한된 조작 문제의 차이도 설명 [R5]. ‘일부 결과를 보존하며 편집’도 기존이다.
- [P] Kong 등: bulk/subgrid 계층으로 differentiable fluid control의 메모리를 줄임 [R6]. ‘부분 영역에서 역설계한다’만으로 새 기여가 아니다.
- [P, 초록] Ishitobi 등 2026: 시공간적으로 제한된 weathering offset을 pseudo-time reversal로 제어 [R7]. 되돌리기·국소 풍화 수정의 최초성 주장은 불가.

[H] 남는 차이는 **소수의 허용 입력 아래, 반응으로 서로 소비/생성되고 공유 형상을 바꾸는 여러 채널의 지정 동작을 함께 보호하는 문제**다. 위 논문이 이를 원리상 못 한다고 주장하지 않는다. 강한 일반 최적화와 그 모델을 합리적으로 확장한 대조가 필요하다.

### 5.6 최소 비교 과제와 Ablation

| 과제 | 조작/보호 | 목적 |
|---|---|---|
| Q2-A 작은 feasible sanity case | 분리된 영역에서 목표만 변경. 알려진 허용 입력 witness를 보관 | 제한·gradient·보호 구현이 맞는지 확인. 주 기여 사례로 사용하지 않음 |
| Q2-B 연결된 열–물–흔적 편집 | 한 ROI의 흔적을 줄이고 다른 ROI의 Ice/Heat 진행을 보호 | 실제 coupling에서 trade-off·보상 범위 확인. 처음부터 feasible이라고 가정하지 않음 |
| Q2-C Geometry 매개 편집 | Reaction 직접 경로와 Geometry 경로가 모두 있는 사례 | Geometry를 무시한 민감도가 부족한지 확인. 기존 2.95%로 중요성을 대신하지 않음 |
| Q2-D 명시적 불가능 과제 | 위 닫힌 수지 조건 | 불가능한 요구를 실패/오류와 구분 |

주 비교: **같은 허용 조작을 쓰는 전역 bounded SQP/finite difference 또는 full AD**, 동일 budget의 derivative-free 최적화. 보조 비교: edit-only, fixed spatial support, Geometry gradient 경로 제거(실제 forward는 full coupling), 보호를 단순 penalty로 처리. 직접 State painting은 외관만의 상한/미래 반응 비교군으로 별도 표시한다. 원래 허용 조작을 위반하는 painting을 불공정한 주 Baseline으로 삼지 않는다.

지표는 edit error–protection error Pareto 곡선, 동시 충족률, 미사용 probe 오차, 조작 support/크기, full rollout 호출·총 시간·메모리, 실패·재시도다. 목표/보호 tolerance는 반복 수치 오차와 독립적인 품질 요구를 바탕으로 실행 전에 정한다. UI가 같지 않은 인간 생산성 비교는 아직 열지 않는다.

**중단 기준:** 작은 표준 SQP가 모든 중요 과제를 충분히 처리 / Geometry 경로의 중요성이 관측되지 않음 / 보호가 사실상 모든 편집을 막음 / 동일 품질에서 사전 계산 포함 이득 없음. ‘다중 State라 복잡하다’는 이유만으로 계속하지 않는다.

## 6. 방향 3 — 평균량이 잃는 혼합·경로 정보의 coarse 표현

### 6.1 현재 코드에서 직접 유도되는 최소 반례

[C] `IceReactionLab.SRProfile`의 `water_cooling`은 Heat에 비례하는 rate coefficient 0.5와 Waterfilm의 linear modifier(scale=1)를 사용한다. `SurfaceReaction.comp::reactionRequest`를 대입하면 local request는 \(0.5 H_i W_i/a_i\)다. \(a_i\)는 기준 면적 대비 texel 면적이고, 반응이 사용하는 정규화는 **State/면적**이다. 표시용 State/Capacity와 혼동하지 않는다.

[D] 동일 면적 셀에서 \(h_i=H_i/a_i, w_i=W_i/a_i\)라 하면 단위 면적당 초기 냉각률은 \(0.5h_iw_i\)이고

\[
\overline r=0.5(\overline h\,\overline w+\operatorname{Cov}(h,w)).
\]

두 미세 셀을 coarse 한 셀로 합친다고 하자.

| 미세 분포 | 셀 1의 (h,w) | 셀 2의 (h,w) | 평균 (h,w) | 평균 초기 냉각률 |
|---|---|---|---|---|
| 분리 | (2,0) | (0,2) | (1,1) | 0 |
| 혼합 | (1,1) | (1,1) | (1,1) | 0.5 |

동일 Profile·평면에서 Transport/Decay/Geometry 변화를 끄고 이 local reaction을 분리하면, **같은 coarse 총량만으로 두 초기 반응률을 동시에 재현할 수 없다.** 이 계산은 실제 구현 식의 대입이며 새로운 GPU 실험 결과가 아니다. 시간 적분·공급량 제한이 활성화된 장시간 결과가 위 단일률과 같다고 주장하지 않는다. 분리된 재료가 실제로 접촉/혼합되기 전 반응하는 coarse 오차의 단서다.

이것은 1·2보다 문제 존재의 근거가 구체적이다. 그러나 평균의 곱과 곱의 평균 차이, covariance closure 자체는 오래된 문제다. [R9]는 segregated initial conditions의 coarse diffusion–reaction에서 단순 rate 보정 이상이 필요함을 다룬다. **이 반례를 발견한 것만으로 신규성은 없다.**

### 6.2 ‘해상도 독립’ 대신 분명한 목표

고해상도 MDSSP의 discrete forward \(\Phi_h\)를 기준 모델로, q 총량을 합치는 restriction R을 둔다. 물리적 정답이나 아직 유도하지 않은 연속 PDE를 기준으로 가정하지 않는다. coarse 예측기 \(\widehat\Phi_H\)의 목표는 \(R\Phi_h^m(q)\)를 관측량/보존식/비용 범위에서 근사하는 것이다.

[D] 어떤 x,y에서 Rx=Ry이지만 \(R\Phi_h^m(x)\ne R\Phi_h^m(y)\)이면, 평균만을 입력으로 받는 결정론적 coarse 모델은 둘을 동시에 정확히 맞힐 수 없다. norm에서 두 fine 결과의 거리가 d이면 같은 coarse 출력의 두 오차 중 적어도 하나는 d/2 이상이다(삼각부등식). 이는 일반적인 정보 손실의 성질이며 새 정리로 주장하지 않는다. Rx에는 coarse 모델에 허용된 모든 정적 형상/재질 정보도 포함해야 한다.

따라서 연구할 것은 ‘계수 하나를 해상도에 맞게 보정’보다 **어떤 추가 상태 z를 저장하고 어떻게 진화시키면 중요한 fine 결과를 저비용으로 근사하는가**다.

### 6.3 가능한 표현과 명확한 한계

| 후보 표현 | 기대 역할 | 해결되지 않는 문제 |
|---|---|---|
| 필요한 반응 쌍의 covariance/mixing fraction | Heat–Waterfilm처럼 곱 형태의 초기 평균 반응률 보정 | covariance의 시간 진화도 closure 필요. threshold/ramp·다중 소비 경쟁은 같은 이차 모멘트만으로 일반적으로 닫히지 않음 |
| 소수의 양수 quadrature 상태와 면적 가중치 | 혼합/분리·임계값 일부를 평균보다 자세히 보존 | 같은 통계라도 공간 연결이 다르면 flux를 구별하지 못함. node 수를 늘리면 비용 증가 |
| coarse 경계의 소수 통과 특성/국소 그래프 | 좁은 홈·장벽의 통과와 State 분포 상관 반영 | 고정 지형의 사전 conductance만으로 State가 바꾸는 형상을 모두 처리할 수 없음 |
| 필요한 영역만 microcell 사용 | compact closure가 실패하는 곳을 정확하게 처리 | 기존 AMR/microgrid와 다를 수 있음. 결국 대부분을 세분화하면 compact 표현 이점 사라짐 |

이 표는 구현 제안이며 기존 MDSSP 기능이 아니다. 모든 State 쌍의 covariance와 모든 세부 연결을 저장하는 것은 피한다. 반응 graph가 실제로 참조하는 쌍만 보존하는 범위도 지표로 검증해야 한다. 기존 chemistry reduced closure와 hydrology subgrid 방식의 단순 적용이면 기술 개선으로 한정한다.

[P] [R10–R11]에는 subgrid 지형에서 수위에 따른 storage/통과 특성 또는 cell/cell-face 평균화를 쓰는 선례가 있다. 따라서 ‘높이 평균 대신 경계 정보를 저장’도 독창성으로 가정하지 않는다. 동적 다중 State 축적에서 어떤 새 갱신이 필요하고 비용 대비 무엇을 유지하는지가 후보다.

### 6.4 단계와 Baseline

**3A: 반응부터 분리.** 알려진 분리/혼합, Heat threshold 근처의 분포, 공급량 경쟁을 고정 평면에서 비교한다. 기준은 fine operator를 면적 합한 결과다. scalar coarse/단위·dt 보정, best-fit scalar rate, 표준 covariance 보정, 2/4-node positive quadrature, 실제 microgrid를 대조한다. fine에서 매 Step 읽은 covariance를 쓰는 oracle은 표현 상한 진단이며 실용 방법으로 계산하지 않는다.

**3B: 수송만.** Reaction OFF에서 같은 coarse 정보/높이 통계를 갖지만 연결이 다른 홈/장벽을 구성한다. subgrid conductance, cell-face 자료, microgrid/AMR를 대조한다. 정적 지형 사전 계산만으로 충분하면 새 dynamic closure 주장을 하지 않는다.

**3C: 결합.** 3A/3B의 어려움이 확인된 경우에만 State 축적이 통과 특성과 혼합을 바꾸는 문제로 확장한다. Reaction ON/OFF × Geometry 동적/고정의 factorial 대조로 결합 항의 필요성을 확인한다. 세 요소가 있다는 이유로 각각의 baseline을 생략하지 않는다.

초기 범위는 한 평면/한 Profile·Ice/Waterfilm/Heat(+잔류)와 단일 coarsening factor다. 실제 문제 확인 뒤 다른 입력 패턴·기울기·시간 간격/해상도와 한 미사용 형상으로 넓힌다. Bunny seam 누락이나 고해상도 미수렴을 coarse 모델의 오차로 잘못 흡수하지 않는다. UV 재분할·물체 scale·normal 적분 오차는 별도 요인으로 분리한다.

**평가:** 제한한 State량·분포의 trajectory error, 누적 반응 extent, 경계 통과량/도달 시점, 목표 면적·모양, 비음수·알려진 물질 불변량, 메모리·전처리/학습/추론 총 비용. 채널 숫자를 모두 더해 물질 보존이라고 하지 않는다. fine 모델이 물리적 정답이라는 주장도 하지 않는다.

**중단 기준:** 단위/거리 보정 또는 표준 closure로 충분 / 효과가 fine 해상도를 올리면 사라지는 기준 오류 / mixing 정보가 실제 외관·진행에 중요하지 않음 / compact 모델이 microgrid보다 손해 / 매 Step fine oracle이 필요 / 계수·mesh별 대규모 재학습으로 재사용 이점 상실.

### 6.5 `inflowFraction`은 별도로 다룰 문제

현재 유입 비율 f는 실제 Incoming에 적용된다. 완전한 일방향 transfer를 m번 겪는 단순 예에서 남는 양은 \((1-f)^m\)이다. 이 예를 일반 MDSSP의 전체 흡수율 공식으로 쓰지 않는다. 부분 flux·되돌아오는 흐름·시간 간격·지형에 따라 관계가 다르다.

‘유입 1회당 비율’이 의도한 의미라면 그 자체가 버그는 아니다. 단위 거리/체류 시간당 효과를 의도했다면 rate/distance 기반 재정의가 필요할 수 있다. 이 **규칙의 단위 문제**와 위 **fine 혼합 정보의 손실**은 서로 다르다. 표준적인 지수 감쇠/면적·거리 보정으로 전자가 해결되었다고 후자의 closure 문제까지 해결했다고 하지 않는다.

## 7. 문헌 대조와 확인 수준

| ID | 1차 출처 | 이번에 확인한 내용·제한 | 연구 설계에 주는 영향 |
|---|---|---|---|
| R1 | Bosch et al. 2011, [Image-Guided Weathering](https://graphics.cs.yale.edu/publications/image-guided-weathering-new-approach-applied-flow-phenomena), [author PDF](https://graphics.cs.yale.edu/sites/default/files/a20-bosch.pdf) | 저자 기관 소개와 검색 색인에 노출된 원문 §3–4 일부. PDF 직접 열기는 크기 제한으로 실패 | 파라미터·detail map 추출과 새 target 적용은 선례. 다중 State 보호/후속 반응 보장은 확인 못 함 |
| R2 | McNamara et al. 2004, [Fluid Control Using the Adjoint Method](https://grail.cs.washington.edu/wp-content/uploads/2015/08/mcnamara-2004-fcu.pdf) | 저자 기관 PDF 검색 색인의 §6 adjoint 연산 설명. 직접 열기는 timeout | forward 연산을 역순 adjoint로 미분해 keyframe control하는 접근은 기존 |
| R3 | Mordvintsev et al. 2021, [Differentiable Programming of Reaction-Diffusion Patterns](https://selforglive.github.io/alife_rd_textures/ALIFE_Reaction_Diffusion.pdf) | 6p 원문 텍스트 pp.1–5: 모델·loss·학습·해상도/mesh 전이와 제한 | texture 역설계·RD 학습·mesh 일반화 최초성 배제. 비음수/질량 보장은 해당 모델의 전제가 아님 |
| R4 | Pan et al. 2013, [Interactive Localized Liquid Motion Editing](https://www.cs.columbia.edu/~cxz/publications/sketchy_fluids.pdf) | 10p 원문 관련 텍스트: 소개의 deformation/guidance 절차, 검색 색인의 마지막 limitation. 페이지 screenshot은 timeout | shape/keyframe 국소 수정·시공간 전파 선례. 다중 재질 반응 보호까지 비교한 것은 아님 |
| R5 | Barbič et al. 2012, [Interactive Editing of Deformable Simulations](https://www.cs.columbia.edu/cg/pdfs/1344873352-BarbicSinGrinspun-SIGGRAPH2012.pdf) | 8p 원문 텍스트 pp.1–4, 목적·fully actuated 설명·reduced constraints·경계 | 세부 보존·제약 편집·reduction은 기존. 원문의 ‘모든 force 허용’과 MDSSP 제한 입력을 구분 |
| R6 | Kong et al., [Hierarchical Differentiable Fluid Simulation](https://onlinelibrary.wiley.com/doi/abs/10.1111/cgf.70226), [저자 목록](https://www.cs.mcgill.ca/~kry/) | 출판사 원문 HTML 관련 §1/3.2–3.4/4.3과 토의. 2025-10-17 온라인 출판, 사이트의 자동 인용에는 2026 표기도 있음 | bulk/subgrid 역설계·메모리 절감이 이미 존재. 무조건 빠르다고 비교하지 않음 |
| R7 | Ishitobi et al. 2026, [Directable Weathering…Pseudo-Time Reversal](https://doi.org/10.1109/MCG.2026.3734161), [논문 초록](https://pubmed.ncbi.nlm.nih.gov/42747933/) | **논문 초록만**. 2023 학위논문과 동일 전체 알고리즘이라고 가정하지 않음 | 시공간 offset/되돌리기 선례. 세부 다중 State 보호 지원 여부 미확인 |
| R8 | Bangaru et al. 2021, [Systematically Differentiating Parametric Discontinuities](https://people.csail.mit.edu/sbangaru/projects/teg-2021/teg-2021.pdf) | MIT 저자 PDF의 검색 색인에 노출된 서론/결론. 직접 열기는 크기 제한 | 불연속 미분을 새로운 문제로 처음 제시한다고 쓰지 않음. MDSSP에서 적합성은 미검증 |
| R9 | Ostvar & Wood 2016, [A non-scale-invariant form for coarse-grained diffusion-reaction equations](https://doi.org/10.1063/1.4962421) | 출판사 검색 색인 원문 중 covariance/closure 관련 Eq.29–31·설명과 소개. 전체 직접 열기는 실패 | 평균화에 따른 초기 분리·추가 항·rate 유효화 문제는 기존. 현재 코드의 혼합 반례를 새 이론으로 주장하지 않음 |
| R10 | Milzow 등 2010, [Accounting for subgrid scale topographic variations in flood propagation modeling using MODFLOW](https://doi.org/10.1029/2009WR008088) | 출판사 검색 색인의 초록·§1/4 관련 원문. 직접 DOI 열기는 실패 | subgrid 지형의 수위 의존 통과/저장 함수와 fine 사전 계산은 기존 |
| R11 | [Use of upscaled elevation and surface roughness data in two-dimensional surface water models](https://www.sciencedirect.com/science/article/abs/pii/S0309170811000303) | 출판사 초록/소개·highlights 수준 | cell와 face 평균화·통로 정보의 활용도 비교 대상. 전체 세부 수치법 검토는 남음 |

새 세부 해법을 선택하기 전 R7/R9/R10/R11의 미확인 원문 절과 실제 사용할 기존 closure/optimizer의 정확한 조건을 추가 확인해야 한다. 검색 요약만으로 이들의 미지원 기능을 단정하지 않는다. 기존 Günther/Chen/Lo 등과의 비교는 [[06_Prior-Model-Differentiation|이전 분석]]을 함께 적용한다.

## 8. 실행을 재개할 때의 최소 의사결정 순서 — 현재는 미실행

| 순서 | 산출물 | 계속할 조건 | 중단/축소할 조건 |
|---|---|---|---|
| A | 방향 1의 관측-후속 반응 모호성 진단, 방향 2의 허용 조작/보호 지표·실행 가능한 단위 과제 | 제작상 중요한 trade-off가 실제 있음 | 목표가 수치 floor 안의 차이이거나 UI 문제뿐임 |
| B | 작은 차원의 **전역 표준 constrained optimizer** 결과 | 같은 의미·예산에서 해결 안 되는 정확한 이유가 남음 | 표준 방법으로 충분하면 새 특화 알고리즘 보류 |
| C | 방향 2의 coupling-aware 수정 초안·gradient/범위 Ablation | forward 검증과 보호 오차를 맞추며 유용한 품질–비용 차이 | 단순 loss/기본 최적화 조합 이상 차이 없음 |
| D | 방향 3의 reaction-only 최소 반례·표준 closure/microgrid 대조 | 정보 손실이 실제 진행/외관에서 중요하고 기존 closure의 한계가 있음 | 단위 보정/표준 mixing correction으로 충분 |
| E | 한 연구 축만 선택해 사례/미사용 입력/mesh 확대 | 중요한 모델 차이와 해결 방법을 설명 가능 | 세 문제를 통합했다는 이유만으로 범위 확대하지 않음 |

A/B와 D는 독립적인 문제 탐색이지만 실행 자동화나 동시 GPU 부하를 지금 시작하지 않는다. 사람의 제작 시간 연구, UI 제작, 모든 GLSL의 differentiable rewrite, 대규모 학습 데이터 생성은 초기에 필요하지 않다. 1+2의 forward가 지나치게 비싸면 먼저 짧은 headless 상태 목표 진단을 고려하되, production과 일치한 범위·renderer/접촉 생략을 명시한다.

**최종 연구 권고:** 1은 2의 목표·평가 층으로 통합한다. 2는 ‘제약된 조작으로 결합 결과를 선택적으로 수정’하는 방법이 표준 최적화보다 필요한지를 먼저 확인한다. 3은 반응 평균화의 코드 기반 반례에서 출발하되 기존 closure를 반드시 상대한다. 지금 확정할 것은 문제/기각 조건이며, 확정 Contribution이나 채택 확률이 아니다.
