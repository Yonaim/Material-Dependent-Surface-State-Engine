# 목표 진행과 보호 편집

목표 역설계·국소 보호 편집의 제약, 조건부 입력 구성, 실제 Heat contact 자유도를 묶었다. 제한 연속 모델의 해와 실제 출력 보호 성공은 구분한다.

## 먼저 볼 결론

- 목표 진행 역설계는 보호 편집의 목표와 평가 층으로 결합한다. 목표를 만들 수 있다는 제한 모델의 해가 실제 제작 입력의 가능성을 보장하지 않는다.
- 실제 자산의 Heat Add 48개는 유한 입력 자유도를 제공한다. 두 사건의 `±ε` 변경은 명목상 시간 합산 입력을 보존하지만 최종 State와 후속 반응의 보호는 미검증이다.
- 허용 조작과 보호 조건을 먼저 고정하고, 같은 자유도·예산의 표준 constrained optimizer와 비교해야 한다. 공유 형상 경로는 시간별 Geometry 재생 대조로 분리한다.

## 이 문서의 원본 절

- [[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-04-inverse-authoring|10_Authoring/04_Inverse-Authoring.md]]
- [[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-05-protected-editing|10_Authoring/05_Protected-Editing.md]]
- [[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-19-protected-editing-feasibility-and-response-coordinates|10_Authoring/19_Protected-Editing-Feasibility-and-Response-Coordinates.md]]
- [[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-21-constructive-controls-and-reaction-contracts|10_Authoring/21_Constructive-Controls-and-Reaction-Contracts.md]]
- [[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-22-protected-editability-and-geometry-mediation|10_Authoring/22_Protected-Editability-and-Geometry-Mediation.md]]
- [[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-23-contact-control-space-audit|10_Authoring/23_Contact-Control-Space-Audit.md]]

원본 25개 파일의 수정 전 사본과 SHA-256 목록은 `Original-25-Notes.zip`에 있다. 아래 절은 원문의 수식과 당시 판단을 보존하며, 링크와 제목 수준만 새 위치에 맞췄다.

---

## source-authoring-04-inverse-authoring

원본: `10_Authoring/04_Inverse-Authoring.md`

### 방향 1 — 목표 진행과 후속 반응을 만족하는 역설계

마지막 갱신: 2026-10-27. [[04_Development/08_Research-Directions-123|최초 통합 검토]]에서 분리한 후속 연구 정본이다. 이 파일의 내용은 제안·수식 분석이며 새 구현/실험 결과가 아니다. 로컬 앱·GPU·빌드 실행 중지를 유지한다.

공통 근거는 [[90_Research-Archive/Research-Notes-2026-10/03_Solver/01_Forward-and-Reaction-Semantics#source-solver-10-forward-model-and-controls|forward 모델]], 문헌 출처는 [[90_Research-Archive/Research-Notes-2026-10/04_Literature/01_Related-Work#source-reference-07-related-work|문헌 확인 수준]], 이후 연구는 [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-reference-09-analytical-findings|분석 노트]]를 따른다.


#### 1. 문제를 더 좁히기

사진 한 장에서 실제 물리 파라미터를 복원하는 문제로 시작하지 않는다. 현재 모델의 초기 상태/재질을 알고, 제작자가 원하는 시점별 지표와 후속 반응을 지정하는 **설계 문제**로 둔다. 유일한 진실의 State를 복원한다고 주장하지 않는다.

예: 3초의 잔존 Ice, 8초의 Waterfilm 도달 영역, 14초의 잔류 흔적을 맞추고, 이후 고정된 Heat pulse에 대한 반응 곡선도 지정한다. 시점/수치는 설명 예시이며 기존 AUTH-GOAL gate를 변경하는 실행 명세가 아니다.

허용 조작 a를 정하고 원래 forward rollout을 \(\Phi(a)\), 관측을 H, 후속 시험 입력 p에 대한 반응을 \(\Psi_p(a)\)라 하면 다음과 같다.

\[
\min_{a\in\mathcal U}\; L_{\mathrm{progress}}(H\Phi(a),y^*)
  +\sum_{p\in\mathcal P_{\rm train}} w_p L_{\mathrm{response}}(\Psi_p(a),r_p^*)
  +\lambda\Omega(a).
\]

이 목적함수와 regularization 자체는 표준적인 최적제어 형식이다. 새 기여로 세지 않는다. 실제 후보는 **관측에 비슷하게 맞는 여러 해 사이의 후속 거동 차이를, 제한된 제작 조작 아래 효율적으로 줄이는 방법**이어야 한다.

#### 2. 먼저 고정할 조작 범위

1. **U-input:** 재질/반응 inventory·초기 State를 고정하고, 정한 source 영역과 시간 basis의 비음수 Heat/Waterfilm 입력만 조절. source별 총량·세기·시간·공간 support에 bounds를 둔다. 잔류 State 직접 칠하기는 허용하지 않는다.
2. **U-profile:** 별도 과제로 제한된 2–6개 Profile 계수까지 허용. 반응 삭제·계수 0으로 무력화·장면별 새 Rule 삽입으로 목표를 우회하지 않도록 범위를 미리 정한다.

두 범위의 결과를 섞지 않는다. 모든 texel·채널에 자유로운 양/음의 source를 주는 방법은 별도 강한 상한 대조가 될 수 있지만 U-input 과제의 공정한 주 비교군으로 두지 않는다.

#### 3. 핵심 가설과 이미 알려진 부분

[H] 비슷한 외관/진행 목표를 만족하는 두 허용 rollout이 후속 입력에서 서로 다른 결과를 낼 수 있고, 이 차이가 제작상 중요할 수 있다. **현재 U-input의 실제 rollout 쌍을 확보한 것은 아니다.**

[C,D] 후속 분석에서는 Ice=4a/8a가 같은 표시 포화도·capped 형상을 만들지만 후속 융해 request는 다른 조건부 State 쌍을 유도했다. 이는 저장량→관측의 비단사성이다. 초기 Ice 고정인 U-input의 두 해로 간주하지 않는다. 초기 Ice seed strength를 허용하는 별도 U-seed 진단과 실제 외관 확인은 미실행이다. 식·가정·조작 범위의 제한은 [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#3. 방향 1: 같은 포화 외관·형상, 다른 후속 반응|후속 분석 §3]]을 따른다.

[D] 선형화한 관측 Jacobian \(J_y\)에 대해 \(J_y\delta a\simeq0\)이나 후속 반응의 \(J_\Psi\delta a\)는 큰 방향이 있다면, 관측만의 적합도는 그 방향을 충분히 제어하지 못한다. 이는 표준적인 관측/식별성의 국소 설명이며, 비선형 전체 공간의 비식별성 증명이 아니다.

[P] Bosch 2011은 사진에서 flow parameters/detail maps를 추출해 새 target에 적용한다. Mordvintsev 2021은 RD update의 파라미터를 texture loss로 학습하고 mesh/해상도 전이도 보여준다. 따라서 ‘풍화 파라미터 자동 탐색’, ‘RD를 미분해 학습’, ‘다른 mesh에 적용’은 빈 영역이 아니다. 관련 출처 [R1–R3].

#### 4. 최소 진단과 Baseline

| 진단 | 수행할 내용 | 필요한 해석 |
|---|---|---|
| Q1-A 관측 모호성 | 같은 허용 조작 범위에서 관측 오차가 ε 이내인 여러 해를 찾고 고정 후속 pulse 결과를 비교 | 차이가 관측/수치 반복 오차보다 작으면 이 문제 설정의 필요성 약함 |
| Q1-B 추가 제약의 효과 | 관측-only, 관측+시점별 State 지표, 관측+후속 반응을 같은 optimizer로 비교 | loss 추가의 유용성과 새 알고리즘의 우위를 구분 |
| Q1-C 일반화 | fit에 쓴 pulse와 크기/위치/시간이 다른 미사용 pulse에서 평가 | train pulse에 맞춘 해를 모든 미래의 보장으로 확대하지 않음 |

기본 대조는 bounded CMA-ES 또는 다른 derivative-free 방법과 finite-difference+SQP/trust-region이다. 미분 가능한 경로를 만들 경우 full-coupling AD/adjoint를 강한 기준으로 포함한다. 평가는 같은 target/허용 조작/초기화 seed/rollout budget에서 한다. derivative-free 평가 호출 비용을 누락하지 않는다. optimizer를 임의로 약하게 설정해 새 방법을 유리하게 만들지 않는다.

#### 5. 추가 연구가 필요한 경우의 해법 후보

[H] 관측과 후속 반응 민감도를 분리한 control basis, 반응 threshold 근처의 단계적 최적화, surrogate 제안 후 실제 forward 검증을 결합할 수 있다. 각각 기존 개념이며 **MDSSP 결합 구조에서 standard baseline이 실패하는 이유와 해결 효과**가 있어야 새 방법이 된다.

지금 바로 전체 renderer/solver를 미분 가능한 새 엔진으로 재작성하지 않는다. 첫 진단은 적은 수의 입력 basis와 state-space 목표로 가능하다. 실제 구현/실행은 별도 재개 지시 전 하지 않는다.

**지표:** 진행 오차, 미사용 pulse 응답의 평균/최악 오차, 목표 동시 충족률, 입력 총량/공간·시간 복잡도, 실패율, forward 호출 수. 완성된 rendered 과제에서는 고정 camera/lighting의 외관도 확인한다. State 지표만 맞춘 결과를 예술적 품질로 대체하지 않는다.

**중단 기준:** 모호성이 중요하지 않음 / 단순 다중 목적 최적화가 충분함 / 추가 이점이 큰 모델 변경이나 목표 편향에 의존함. 이 경우 독립 논문 Contribution 주장은 중단하고 2의 평가 과제로만 사용한다.

후속 [[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#7. 방향 1: 후속 probe가 줄 수 있는 것과 줄 수 없는 것|심화 추론 §7]]에서 복제한 한 texel의 두 초기 probe로 i,h를 구별하는 조건과, 유한 probe의 보장 한계를 유도했다. 내부 State는 이미 알려져 있으므로 이를 미지의 물리 State 복원으로 포장하지 않는다. 참조 State 거리 제약도 강한 Baseline에 포함한다.

---

## source-authoring-05-protected-editing

원본: `10_Authoring/05_Protected-Editing.md`

### 방향 2 — 다른 현상을 보존하는 수정

마지막 갱신: 2026-10-27. [[04_Development/08_Research-Directions-123|최초 통합 검토]]에서 분리한 후속 연구 정본이다. 이 파일의 내용은 제안·수식 분석이며 새 구현/실험 결과가 아니다. 로컬 앱·GPU·빌드 실행 중지를 유지한다.

공통 근거는 [[90_Research-Archive/Research-Notes-2026-10/03_Solver/01_Forward-and-Reaction-Semantics#source-solver-10-forward-model-and-controls|forward 모델]], 문헌 출처는 [[90_Research-Archive/Research-Notes-2026-10/04_Literature/01_Related-Work#source-reference-07-related-work|문헌 확인 수준]], 이후 연구는 [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-reference-09-analytical-findings|분석 노트]]를 따른다.


#### 1. 핵심 Research Question

최신 심화는 [[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-19-protected-editing-feasibility-and-response-coordinates|19: 보호 편집의 가능 범위·반응 진행 좌표]]를 따른다. 전체 Heat 진행의 고정이 Ice 편집을 막는 조건과, 최종 Ice 보호를 누적 반응 조건으로 표현하는 범위를 추가했다. 아래 최초 과제는 이 조건을 적용해 해석하며 실제 입력 witness·새 해법의 우위는 여전히 미검증이다.

> 허용된 입력/소수 계수만 바꿔 지정한 잔류 흔적을 수정하면서, 보호한 영역의 융해 진행과 후속 반응의 변화를 제한할 수 있는가? 필요한 보상 조작의 범위를 어떻게 선택하는가?

국소라는 말은 **목표 영역**, **조작 영역**, **재계산 영역**을 각각 뜻할 수 있다. 세 영역을 동일하다고 가정하지 않는다. 목표가 국소여도 보상 Heat 입력은 다른 곳/이전 시간에 필요할 수 있다. 보호된 지표가 만족되지 않으면 ‘다른 State를 그대로 유지했다’고 쓰지 않는다.

#### 2. 제약식과 어려움

기존 입력을 a₀, 목표 편집 지표를 E, 보호 지표를 P라 하면:

\[
\min_{\delta a}\;\|E\Phi(a_0+\delta a)-y^*\|^2+\lambda\|\delta a\|_W^2,
\quad \|P\Phi(a_0+\delta a)-P\Phi(a_0)\|\le\epsilon_P,
\quad a_0+\delta a\in\mathcal U.
\]

보호 대상은 초기에는 소수 영역의 Heat/Ice 총량 곡선·후속 pulse 응답처럼 유한한 지표다. 전체 State의 모든 시점을 완전히 고정하면서 연결된 일부 State를 자유롭게 변경할 수 있다고 요구하지 않는다.

[D] Dense 수학 경로에서 \(G=B(q)\), \(q'=F(q,G,a)\)이면 국소 민감도는 개념적으로 \(F_q+F_G B_q\)다. Geometry 경로를 빼면 Heat처럼 축적 계수가 0인 채널도 영향을 받을 가능성을 누락한다. 이 chain rule 자체는 새 이론이 아니다. 실제 bootstrap/이전 Geometry 의존성은 전체 상태·연산 순서로 포함해야 한다.

[C] `max` flux, outgoing limiter, 반응 threshold, supply competition, 내부 시간 세분화, 형상 Capacity clamp가 있다. AD를 연결하면 모든 분기에서 쓸 만한 gradient가 자동으로 생기는 것은 아니다. [P] TeG는 불연속 미분이 이미 별도 연구 분야임을 보여준다 [R8].

#### 3. 논문으로 발전시킬 수 있는 기술적 가설

[H] **반응·형상 경로를 포함한 보호 민감도로 수정 방향을 제한하고, 실제 forward 오차를 확인하면서 조작 영역을 확장하는 방법**이, 고정 반경 편집 또는 전역의 모든 조작을 쓰는 방법보다 같은 edit/protection 품질에서 유용한 trade-off를 줄 수 있다.

첫 알고리즘 초안은 다음 정도로 제한한다.

1. 기존 rollout과 입력 basis에서 \(J_E,J_P\)를 추정한다. 처음에는 finite difference도 허용한다.
2. 작은 편집은 \(J_P\delta a\simeq0\)인 방향에서 constrained least-squares/trust-region으로 제안한다.
3. 원래 production 의미의 full-coupling forward로 edit/protection·비음수·수지를 검증한다.
4. 선형화 예측이 빗나가면 trust region을 줄이고 active regime를 갱신한다. 조작 범위가 부족하면 사전에 허용한 이웃/이전 시간 basis로 확장한다.
5. 더 넓혀도 찾지 못하면 미달/국소 탐색 실패와 완화 선택지를 보고한다. 전역 불가능 인증으로 표현하지 않는다.

[D,H] 후속 분석에서 `ice_melting`은 Heat/면적=0.20 근방의 작은 변화에는 request 민감도가 0이지만, 허용된 +0.10 입력으로 threshold=0.25를 넘으면 양의 request를 만들 수 있음을 유도했다. 따라서 진전이 없을 때 단순히 trust region을 줄이는 것으로 충분하지 않을 수 있다. 선언된 event margin을 보고 유한 trial을 검토하는 초안을 추가하되, 다중 크기 finite difference·multi-start·derivative-free와 비교한다. 이것은 전체 보호 제약의 feasible witness나 새 알고리즘 검증이 아니다. [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#4. 방향 2: 0 민감도와 유한 편집의 실행 가능성은 다르다|후속 분석 §4]]를 따른다.

**여기서 1–5는 기존 기법의 합성 초안이다.** 새 알고리즘으로 확정하지 않는다. full Jacobian으로 같은 계산을 하는 전역 SQP가 충분히 좋으면 특화 방법을 만들 이유가 없다. finite-difference 민감도 구축 비용·다중 forward·캐시 구축을 모두 비용에 포함한다.

#### 4. 국소 선형 실행 가능성과 전역 불가능의 구분

[D] bound가 활성화되지 않은 선형화에서 N을 \(\ker J_P\)의 basis라 하면, 목표 변화 d가 \(\mathrm{range}(J_E N)\) 안에 있는지가 보호를 유지하는 작은 수정의 조건이다. nullspace나 이 조건 자체는 표준 선형대수다. d가 range 밖이라고 비선형 유한 편집까지 불가능한 것은 아니다. threshold를 건너는 해결책을 현재 Jacobian이 못 볼 수도 있다.

별도의 **정확히 불가능한 단위 과제**는 물질 수지로 만들 수 있다. 닫힌 graph, 물질 외부 입출력·Decay 없음, Ice↔Waterfilm/Waterfilm→AbsorbedWetness만 있는 경우 \(M=I+W+A\)는 보존된다(Heat는 이 합에 넣지 않는다). I 총량과 W 총량도 같게 유지하면서 A 총량만 줄이기는 불가능하다. 이는 통상적인 보존식의 적용이며 새 정리가 아니다. 비현실적인 과제를 optimizer의 실패로 오인하지 않는 sanity case로 사용한다. 외부 sink/입력 또는 다른 반응이 있으면 이 인증을 그대로 쓸 수 없다.

#### 5. 가까운 연구와 실질적 차이의 후보

- [P] Pan 2013: 국소 keyframe을 volume-preserving 기하 변형으로 편집하고 주변 시간·후속 시뮬레이션으로 전달 [R4]. 국소성·시공간 전파 자체는 기존이다.
- [P] Barbič 2012: 기존 세부 운동을 보존하는 reduced spacetime edit와 contact constraints. 관련 절은 모든 force를 허용하는 fully actuated 문제와 제한된 조작 문제의 차이도 설명 [R5]. ‘일부 결과를 보존하며 편집’도 기존이다.
- [P] Kong 등: bulk/subgrid 계층으로 differentiable fluid control의 메모리를 줄임 [R6]. ‘부분 영역에서 역설계한다’만으로 새 기여가 아니다.
- [P, 초록] Ishitobi 등 2026: 시공간적으로 제한된 weathering offset을 pseudo-time reversal로 제어 [R7]. 되돌리기·국소 풍화 수정의 최초성 주장은 불가.

[H] 남는 차이는 **소수의 허용 입력 아래, 반응으로 서로 소비/생성되고 공유 형상을 바꾸는 여러 채널의 지정 동작을 함께 보호하는 문제**다. 위 논문이 이를 원리상 못 한다고 주장하지 않는다. 강한 일반 최적화와 그 모델을 합리적으로 확장한 대조가 필요하다.

#### 6. 최소 비교 과제와 Ablation

| 과제 | 조작/보호 | 목적 |
|---|---|---|
| Q2-A 작은 feasible sanity case | 분리된 영역에서 목표만 변경. 알려진 허용 입력 witness를 보관 | 제한·gradient·보호 구현이 맞는지 확인. 주 기여 사례로 사용하지 않음 |
| Q2-B 연결된 열–물–흔적 편집 | 한 ROI의 흔적을 줄이고 다른 ROI의 Ice/Heat 진행을 보호 | 실제 coupling에서 trade-off·보상 범위 확인. 처음부터 feasible이라고 가정하지 않음 |
| Q2-C Geometry 매개 편집 | Reaction 직접 경로와 Geometry 경로가 모두 있는 사례 | Geometry를 무시한 민감도가 부족한지 확인. 기존 2.95%로 중요성을 대신하지 않음 |
| Q2-D 명시적 불가능 과제 | 위 닫힌 수지 조건 | 불가능한 요구를 실패/오류와 구분 |

주 비교: **같은 허용 조작을 쓰는 전역 bounded SQP/finite difference 또는 full AD**, 동일 budget의 derivative-free 최적화. 보조 비교: edit-only, fixed spatial support, Geometry gradient 경로 제거(실제 forward는 full coupling), 보호를 단순 penalty로 처리. 직접 State painting은 외관만의 상한/미래 반응 비교군으로 별도 표시한다. 원래 허용 조작을 위반하는 painting을 불공정한 주 Baseline으로 삼지 않는다.

지표는 edit error–protection error Pareto 곡선, 동시 충족률, 미사용 probe 오차, 조작 support/크기, full rollout 호출·총 시간·메모리, 실패·재시도다. 목표/보호 tolerance는 반복 수치 오차와 독립적인 품질 요구를 바탕으로 실행 전에 정한다. UI가 같지 않은 인간 생산성 비교는 아직 열지 않는다.

**중단 기준:** 작은 표준 SQP가 모든 중요 과제를 충분히 처리 / Geometry 경로의 중요성이 관측되지 않음 / 보호가 사실상 모든 편집을 막음 / 동일 품질에서 사전 계산 포함 이득 없음. ‘다중 State라 복잡하다’는 이유만으로 계속하지 않는다.

후속 [[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#6. 방향 2: 최적화 실패 전에 불가능을 판정할 수 있는 범위|심화 추론 §6]]은 닫힌 I+W+A 수지에서 보호 오차의 하한, 선언 stoichiometry/graph로 만든 도달 가능성 superset의 제한적 infeasibility 판정을 추가했다. 기존 Petri-net/수지 기반 방법의 적용이며, 같은 사전 검사를 제공한 표준 optimizer와 비교해야 한다. 외부 물 입력을 허용하는 U-input에 닫힌 보존 예를 그대로 적용하지 않는다.

---

## source-authoring-19-protected-editing-feasibility-and-response-coordinates

원본: `10_Authoring/19_Protected-Editing-Feasibility-and-Response-Coordinates.md`

### 보호 편집의 가능 범위와 반응 진행 좌표



작성: **2026-10-28 / Asia/Seoul**. [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-18-research-landscape-and-selection|18]]의 A를 심화한다. 제목의 보호는 지정 관측의 허용오차를 뜻하며 전체 시뮬레이션 불변을 뜻하지 않는다.



**문헌·코드·기호 유도만 수행했다. 새 구현·실험·실행 가능 입력 witness는 없다.** 제품 코드는 변경하지 않았다. 기존 수식에 대한 새 알고리즘의 우위도 입증하지 않았다.



#### 1. 이번 분석의 결론



보호 편집의 첫 문제는 optimizer 선택보다 **어떤 보호 요구가 편집 자유도를 남기는가**다. 현재 IceReactionLab에서는 다음 구분이 중요하다.



1. 같은 위치의 Heat 전 과정을 고정하면, 제한된 조건에서 Ice 전 과정도 결정된다. 이때 Ice를 바꾸라는 요구를 못 푸는 것은 optimizer 결함이 아니다.

2. Ice의 최종량만 보호한다면, Heat 전 과정보다 훨씬 약한 ‘누적 유효 반응량’을 보호하면 된다. 중간 융해 시점과 Geometry 경로를 바꿀 자유도가 남을 수 있다.

3. 이 자유도가 실제 Heat contact 입력으로 도달 가능한지, 그리고 다른 보호 관측과 동시에 만족되는지는 별도 문제다. 자유도가 남는다는 말만으로 feasible task라고 판정하지 않는다.



따라서 더 구체적인 연구 질문은 다음이다.



> 제작자가 보호하려는 결과를 반응 진행·경계 응답 등의 필요한 조건으로 표현해, 불필요한 고정을 피하면서 결합된 현상을 수정할 수 있는가?



‘보호 loss를 추가한다’보다 과제는 명확해졌다. 그러나 반응 진행 좌표·제약 최적화·nullspace 자체는 기존 도구다.



#### 2. 코드에서 쓰는 제한 모델



[IceReactionLab.SRProfile](../../../../../Assets/SurfaceProfiles/IceReactionLab.SRProfile)의 Ice는 수송·Decay가 0이며, `ice_melting`에서만 소비된다. 초기 입력 이후 Ice 추가/제거가 없다고 한정한다. 면적당 Ice·Heat를 i,H라 하고 `k=0.5`, `τ=0.25`다.



[reactionRequest](../../../../../Shaders/Simulation/Solver/SurfaceReaction.comp)의 continuous request는 이 Profile에서 `k i (H−τ)+`에 해당한다. 같은 파일의 `main`은 반응들을 동일 snapshot에서 평가하고 공급량 비율과 적응 substep을 적용한다. 다음 연속식은 **공급 제한과 시간 이산 오차를 제거한 이상 모델**이다.



$$

\dot i=-k\,i\,(H-\tau)_+.

$$



다른 위치의 수송·Geometry가 H를 바꿀 수 있지만, 주어진 H에 대한 이 국소 반응식은 그대로다. 새로운 Ice 반응, 추가 입력, 이동하는 Ice를 허용하면 아래 식을 그대로 사용할 수 없다.



#### 3. 보호 요구가 편집을 막는 조건



##### 3.1 Heat 전 과정 보호 → Ice 진행 고정



i(0)>0이면



$$

Z(t)=k\int_0^t(H(s)-\tau)_+\,ds,

\qquad i(t)=i(0)e^{-Z(t)}.

$$



같은 초기 Ice와 같은 H(t)를 유지하면 i(t)도 같다. 따라서 **동일 texel의 전체 Heat 진행과 초기 Ice를 고정하면서 융해 진행만 바꾸기**는 이 제한 모델에서 불가능하다. 전체 공간에서 Heat를 보호하지 않더라도 편집하려는 texel에서 같은 조건이면 해당 결론이 성립한다.



이는 초기 총량 보존식보다 더 구체적인 ‘동작 종속성’이다. 표준 선형 1차 ODE 해의 적용이며 새 정리가 아니다. 실제 GPU에서는 공급 비율·substep이 달라질 수 있으므로 ‘Heat가 같으면 항상 Ice가 bitwise 동일’이라는 주장으로 확대하지 않는다.



##### 3.2 거의 같은 Heat가 허용하는 Ice 변화



positive-part는 1-Lipschitz이므로 같은 초기량을 가진 두 연속 실행에 대해



$$

\left|\log\frac{\hat i(T)}{i(T)}\right|

\le k\int_0^T|\hat H-H|\,dt.

$$



예를 들어 Heat 보호가 `sup_t |δH|≤ε_H`라면 Ice 비율은 `exp(−kTε_H)`와 `exp(kTε_H)` 사이에 있어야 한다. 목표 Ice 비율이 이 범위를 벗어나면 이 모델의 해당 보호 요구와 양립하지 않는다.



시간 평균 Heat 또는 최종 Heat만 보호하는 경우에는 이 상한을 그대로 적용할 수 없다. threshold 아래/위의 노출 분포가 다를 수 있기 때문이다. Heat의 공간 총량만 같은 경우에도 texel별 융해는 달라질 수 있다.



#### 4. 실제 이산 반응식에 맞춘 좌표



##### 4.1 공급 경쟁·적응 substep을 포함한 항등식



각 실제 반응 substep에서 melting rule의 공급 비율을 λ_j, 길이를 δ_j라 두면, 수학적 실수 연산에서



$$

a_j=\delta_j\lambda_j k(H_j-\tau)_+,

\qquad i_{j+1}=i_j(1-a_j).

$$



Ice가 이 rule에서만 소비되고 다른 연산으로 변하지 않는 조건에서 0≤a_j≤1이다. 모든 a_j<1이면



$$

Z_{\rm discrete}=-\sum_j\log(1-a_j),

\qquad i_N=i_0e^{-Z_{\rm discrete}}.

$$



이 식은 반응별 공급 비율과 실제 substep을 포함하므로 연속식보다 production 계약에 가깝다. 두 실행의 substep 개수가 같을 필요도 없다. a_j=1이면 완전 소진으로 별도 처리하며 Z는 무한대다. 부동소수점 clamp/오차나 arithmetic-failure fallback까지 정확히 표현한다고 주장하지 않는다.



**현재 코드가 이 Z를 별도로 계산·저장한다는 뜻은 아니다.** 상태와 반응 장부에서 유도되는 분석 좌표다. 최종 Ice가 이미 알려져 있으면 Z는 그 값을 다시 쓴 것이므로 정보 압축이나 속도 향상이 자동으로 생기지 않는다.



##### 4.2 단순 누적률을 쓸 때의 차이



0≤a_j≤a_max<1일 때



$$

0\le Z_{\rm discrete}-\sum_j a_j

\le\frac{\sum_j a_j^2}{2(1-a_{\max})}.

$$



이는 `−log(1−a)`의 급수에서 얻는 표준 상한이다. 연속 exposure를 이산 보호 조건의 대용으로 쓸 때, 반응 분할 오차를 무시하면 안 된다는 점을 보여준다. 코드가 일반 continuous substep을 10% depletion 이하로 제한하려 하지만 마지막 budget 소진 fallback 등에서는 이를 무조건 가정할 수 없다.



#### 5. 무엇을 보호하고 무엇을 풀어줄 것인가



| 제작자가 지정한 보호 | 이 제한 모델에서 필요한 조건 | 남을 수 있는 자유도 / 한계 |

|---|---|---|

| 모든 시점의 Ice | 모든 prefix의 Z 유지 | Ice 변화로 생기는 Geometry 경로 수정은 사실상 막힘 |

| 최종 Ice만 | 최종 Z 유지 | 융해 시점 재배치 가능성. 중간 Geometry·Water는 바뀔 수 있음 |

| 최종 Heat만 | H(T) 유지 | 융해량도 지켜진다고 말할 수 없음 |

| 최종 Ice와 Heat | Z(T), H(T) 유지 | 조작 수·민감도에 따라 유용한 자유도 존재 여부 달라짐 |

| 최종 Height만 | capped State 조합 유지 | 숨은 저장량과 미래 반응은 보호되지 않음 |

| 후속 입력에 대한 Water 경로 | 해당 rollout 관측/경계 응답 유지 | 위 국소 Z 하나로 대체 불가. 전체 결합 평가 필요 |



**유망한 초기 과제:** 지정 영역의 최종 Ice·Heat를 유지하면서 중간 물의 배출 시점 또는 다른 영역의 잔류 분포를 바꾼다. 이때 중간 Geometry 변화는 허용한다. 제작 목적이 모든 순간의 형상 보존이라면 이 과제로 바꾸면 안 된다.



단지 같은 exposure를 갖는 두 Heat 곡선을 그리는 것은 실제 허용 입력 witness가 아니다. Heat를 임의로 내리는 조작, 직접 State 덮어쓰기, 음의 입력을 몰래 허용하지 않는다. production contact의 입력 범위를 통과한 witness는 여전히 미확인이다.



#### 6. 실제 입력에서 자유도를 판별하는 표준 검사



입력 basis 계수를 u∈R^m, 보호 관측을 P(u)∈R^r, 수정 관측을 E(u)라 하자. branch가 고정되어 P,E가 매끄럽고 u가 허용 집합 내부이며 DP의 rank가 r이면, 음함수 정리에 의해 국소 보호 집합의 차원은 m−r이다.



그 안에서 수정 가능한 1차 방향이 있으려면



$$

\exists v:\quad DP(u)v=0,\qquad DE(u)v\ne0

$$



가 필요하다. orthonormal nullspace basis N을 쓰면 `DE·N`을 확인하는 검사다. 방향이 있어도 크기·입력 비용·event까지의 거리 때문에 실제 편집 폭이 작을 수 있다. bound 위 입력에서는 nullspace와 허용 tangent cone의 교집합을 써야 한다.



`DE·N=0`은 국소 1차 판정일 뿐 전역 불가능 증명이 아니다. cap/threshold 너머나 2차 방향에 해가 있을 수 있다. rank가 수치적으로 작다는 판단도 Jacobian 오차·tolerance와 함께 정해야 한다. 이번에는 Jacobian을 계산하거나 이 검사를 실행하지 않았다.



이 절차는 일반 SQP·nullspace method가 이미 이용하는 구조다. 새 방법에도 대조군에도 동일하게 허용해야 한다.



#### 7. 실제 알고리즘 후보와 기존 방법의 경계



제안은 다음처럼 제한한다. 모두 **미구현**이다.



1. Profile의 특정 rule에서 유도 가능한 반응 진행 조건을 찾아 보호 요구의 종속성·불가능 조건을 검사한다. 일반 반응망의 자동 해석을 달성했다고 주장하지 않는다.

2. 실제 입력의 국소 민감도로 목표를 바꾸는 보호 방향을 찾는다. 내부 bound·branch에서는 표준 constrained optimizer를 쓴다.

3. local gain이 부족하면 목표/보호 영향이 있는 cap·threshold만 유한 변화 후보로 고려한다. 선택한 후보가 전체 forward에서 보호되는지 다시 평가한다.

4. 불가능 증명, 조건부 feasible witness, 탐색 실패를 서로 다른 결과로 돌려준다. 실패를 ‘해 없음’으로 출력하지 않는다.



**남은 연구 공백:** 많은 texel×time event 중 어떤 것만 봐도 중요한 편집을 놓치지 않는지, 반응 진행 조건이 forward 횟수를 실제로 줄이는지, 보호 관측을 과하게 약화하지 않는지다. 아직 이에 대한 새 선택법·보장·비용 이점은 없다.



R53의 hybrid sensitivity와 R54의 사전 event gradient/정확 forward 분리 선례 때문에 ‘0 gradient를 피하기 위한 smoothing’ 자체는 신규 기여가 아니다. 특히 현 `rampAbove`, min/cap은 연속이지만 미분 불가능한 branch가 많다. 이를 모두 연속시간 reset event로 간주해 saltation matrix를 그대로 붙이면 안 된다. 고정 discrete GPU Step과 제안 surrogate의 차이를 명시해야 한다.



#### 8. 향후 판별 과제와 강한 대조



| 과제 | 확인할 것 | 실패 시 해석 |

|---|---|---|

| A0: Heat 전 과정 보호 + 다른 Ice 진행 요구 | §3의 불가능 조건을 정확히 구분 | 이를 성공시키면 허용 입력/보호/모델이 달라졌는지 감사 |

| A1: 최종 Ice·Heat 보호 + 중간 진행 수정 | 동일 U-input에서 feasible witness와 국소 자유도 | witness가 없으면 알고리즘 성능 과제로 쓰지 않음 |

| A2: threshold/cap 전환이 필요한 같은 보호 과제 | 표준 multi-start·continuation보다 적은 비용의 개선 여부 | 기존법이 충분하면 특화 알고리즘 중단 |

| A3: 실제 Geometry 경로가 중요한 과제 | Geometry를 생략한 민감도와 full coupling의 차이 | 차이가 수치 floor 수준이면 MDSSP 특수성 주장 약화 |



대조는 **같은 보호 관측·입력 bounds·반응 좌표 정보를 받는 SQP/FD·AD, multi-start/continuation, derivative-free**다. 새 방법에는 Z 보호를 주고 대조에는 과도한 Heat 전 과정 보호를 주는 비교는 불공정하다. 공급 제한/step 변화가 없는 부드러운 과제와 있는 과제를 분리한다.



지표는 목표–보호 Pareto, 실제 forward 호출·총 비용, 허용 입력 witness, 미사용 후속 입력의 분포 오차다. false infeasibility는 별도 집계한다. 예쁜 결과나 한 substep의 대수적 사례만으로 제작 도구의 우수성을 주장하지 않는다.



#### 9. 판정



A는 **보호 대상을 반응 진행에 맞게 정하고, 실제 입력의 자유도를 찾아 편집하는 문제**로 더 좁혀졌다. 목적 적합성은 여전히 높다. 반면 ‘모든 Heat를 유지하면서 같은 위치 Ice를 바꾼다’ 같은 과제는 일부 조건에서 기각해야 한다.



이번 유도는 잘못된 과제를 제거하고 비교 기준을 강화했다. 새 optimizer의 필요성이나 SIGGRAPH 수준의 기여를 입증한 것은 아니다. R2/R4/R5의 기존 편집과 R53/R54의 hybrid optimization을 넘어서는 차이를 아직 확보하지 못했다.



#### 후속: 필요한 Heat 입력까지 구성



[[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-21-constructive-controls-and-reaction-contracts|21]]은 반응 진행 z에서 필요한 양수 Heat 입력을 복원한다. 입력 여유가 있는 제한 연속 모델에서 동일 endpoint·동일 총 입력과 다른 중간 진행의 국소 해가 존재함을 유도했다. 실제 event/contact 입력과 공간 Geometry 결합의 성공은 아직 확인하지 않았다. Heat decay는 실제 코드에 맞춰 비례 감쇠가 아닌 상수 양 감소로 대응했다.

---

## source-authoring-21-constructive-controls-and-reaction-contracts

원본: `10_Authoring/21_Constructive-Controls-and-Reaction-Contracts.md`

### 보호 편집의 양수 입력 구성과 유입 반응의 경쟁 계약

작성: **2026-10-28 / Asia/Seoul**. [[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-19-protected-editing-feasibility-and-response-coordinates|19]]와 [[90_Research-Archive/Research-Notes-2026-10/03_Solver/01_Forward-and-Reaction-Semantics#source-solver-20-transport-reaction-semantics-and-refinement|20]]의 후속.

**코드·문헌·기호 분석만 수행했다. 앱·GPU·빌드·수치 실험·학습 및 제품 코드 변경은 없다.** 아래 양수 입력의 존재는 제한된 연속 국소 모델의 결과이며 production contact/GPU의 성공 사례가 아니다.

#### 1. 이번에 추가된 결론

- **A:** 단순히 같은 exposure를 갖는 Heat 곡선을 제안하는 단계를 넘어, 주어진 반응식을 만족하는 **음이 아닌 Heat 입력**을 역으로 구성할 수 있다. 충분한 입력 여유가 있는 제한 모델에서는 최종 Ice·Water·Heat를 같게 두고 중간 진행을 다르게 만드는 해가 존재한다. 추가 조건에서는 총 Heat 입력도 같게 만들 수 있다.
- **B:** 현 `inflowFraction`은 신규 유입으로 request를 계산하지만, 경쟁의 공급 상한은 **반응 직전 전체 보유량**이다. 여러 rule을 합치면 신규 유입보다 많이 소비할 수 있다. 이는 코드의 명시된 계산에서 유도한 것이며 ‘유입에 의해 촉발’과 ‘유입분만 소비’라는 다른 의미를 구분해야 한다.
- 두 결과 모두 새 원리의 발명이 아니다. A는 constrained trajectory generation/반응 좌표 선례, B는 표준 자원 배분으로 먼저 다룰 수 있다. **A의 feasible 기준 문제는 진전했고, B는 새 알고리즘을 찾기 전에 모델 계약을 명확히 해야 한다는 판단이 강해졌다.**

#### 2. A의 국소 연속 모델 — 실제 계수와 제외 조건

[IceReactionLab](../../../../../Assets/SurfaceProfiles/IceReactionLab.SRProfile)의 반응 계수를 쓰되, 공간 수송을 제거한 닫힌 한 영역으로 한정한다. 초기 이후 Ice/Water 입력은 없고 Heat 입력 u(t)≥0만 허용한다. i,w,H는 기준 면적당 양이다.

$$
\dot i=-k i(H-\tau),\quad
\dot w=k i(H-\tau),\quad
\dot H=u-\nu k i(H-\tau)-\gamma H w-d,
$$

여기서 `k=0.5`, `τ=0.25`, `ν=0.45`, `γ=0.5`, `d=0.015`, 그리고 H>τ인 구간만 사용한다. M=i+w는 일정하다. 공급 limiter·이산 splitting·반응 substep은 이 연속 모델에 없다.

**Decay 확인:** [StateMath.glsl::decayAmountPrepared](../../../../../Shaders/Simulation/Solver/StateMath.glsl)는 `DecayRate×AreaScale×Retention×dt`를 양으로 빼고 현재량으로 제한한다. 이 Profile의 protection은 0이므로 양수 H 구간의 연속 대응은 상수 d다. `−dH`라는 비례 감쇠로 바꾸면 코드와 다른 분석이다. 온도·열역학의 물리 모델이라는 뜻도 아니다.

현재 Profile의 Water/Heat는 실제로 수송하므로 위 국소 축약이 production 전체와 같지 않다. 수송을 제거하거나 균일 영역으로 이상화한 sanity reference다.

#### 3. 진행 곡선으로부터 필요한 입력을 직접 구한다

z(t)=−log(i(t)/i₀), i₀>0라고 하면 z'=k(H−τ)>0다. 따라서

$$
i=i_0e^{-z},\quad w=M-i_0e^{-z},\quad H=\tau+z'/k,
$$

$$
u[z]=z''/k+\nu i_0e^{-z}z'
 +\gamma(\tau+z'/k)(M-i_0e^{-z})+d.
$$

**검사 가능한 조건:** z는 C²이고 z(0)=0, z'>0, w≥0, 0≤u[z]≤u_max를 만족해야 한다. 초기/최종 Heat는 z'(0),z'(T)를 고정하고, 최종 Ice는 z(T)를 고정한다.

이 식은 ‘원하는 Heat를 직접 덮어쓴다’는 조작과 다르다. 반응과 냉각 손실을 포함해 필요한 입력을 계산한다. 그러나 여전히 자유로운 연속 스칼라 Heat source를 허용한 것이며, 제한된 contact 위치·입력 횟수·event 양만으로 같은 곡선을 만들었다는 뜻은 아니다.

[R59–R61] 반응 진행 좌표와 출력 곡선으로 상태/입력을 복원하는 접근은 기존이다. 이 유도 자체를 새로운 differential flatness 결과나 전체 MDSSP의 flatness 증명으로 주장하지 않는다.

#### 4. 음의 입력 없이 다른 중간 진행이 존재하는 조건

baseline `z₀(t)=a t`, a>0를 택한다. H₀=τ+a/k는 일정하고 i₀(t)=i₀exp(−at), w₀=M−i₀(t)다. 필요한 baseline 입력은

$$
u_0(t)=\nu i_0e^{-at}a+\gamma H_0(M-i_0e^{-at})+d>0.
$$

유한 T에서 이 입력이 u_max보다 **양의 margin**만큼 작다고 가정한다. 양의 margin이 실제 앱에서 주어졌다는 뜻은 아니다. 입력 상한이 너무 낮다면 이 구성은 적용되지 않는다.

`b(t)=t²(T−t)²`로 두고 `z_ε=at+εb(t)`라 하자. b와 b'는 양 끝에서 0이다. 따라서 모든 ε에 대해 z의 시작·끝 값과 양 끝 도함수가 baseline과 같고, 최종 i,w,H도 같다.

충분히 작은 |ε|에서는 compact 구간의 연속성으로 z'>0와 0<u[z]<u_max가 유지된다. ε≠0이면 중간 i,w,H는 달라진다. **따라서 이 연속 국소 모델에서는 허용 범위 안의 양수 입력으로 endpoint를 보호하며 중간 진행을 바꾸는 해가 실제로 존재한다.** 수치 적분으로 찾은 결과가 아니라 명시적 구성과 continuity에 의한 존재 결과다.

이 사례는 공간 수송/변화된 Geometry의 효과가 없어서 주 연구 데모가 될 수 없다. 새 optimizer가 풀어야 할 최소 sanity 과제로 쓴다. 이 모델에서도 못 푼다면 복합 장면으로 확대할 이유가 없다.

#### 5. 최종 상태가 같아도 입력 비용은 같지 않다

초기/최종 상태와 Z=z(T)를 고정하고, ΔI=i₀−i(T), U=∫u dt라 두면 Heat 수지를 적분해

$$
U=H(T)-H(0)+\nu\Delta I
 +\gamma\tau\int_0^T w\,dt
 +\frac{\gamma}{k}(MZ-\Delta I)+dT.
$$

유도에는 `∫z'w dt=MZ−ΔI`를 쓴다. 따라서 두 endpoint-preserving 경로 사이의 입력량 차이는

$$
\Delta U=\gamma\tau\int_0^T\Delta w(t)\,dt.
$$

위 b≥0, ε>0 구성은 중간에 Ice를 더 일찍 줄여 w를 늘리므로 U도 증가한다. **같은 endpoint의 예쁜 진행을 얻었다고 같은 입력 비용에서 우수하다고 비교하면 안 된다.** 이 제한 모델에서는 냉각과 threshold가 timing–input trade-off를 만든다.

##### 총 입력량까지 같게 유지할 수 있는가

서로 다른 시간 구간에 지지집합을 갖는 음이 아닌 smooth bump b₁,b₂를 두고, 둘 모두 양 끝에서 값과 도함수가 0이라고 한다. `z=at+εb₁−ηb₂`를 사용한다.

`F(ε,η)=∫[w(z)−w(at)]dt`라 하면 원점에서

$$
\partial_\epsilon F=\int i_0e^{-at}b_1dt>0,\qquad
\partial_\eta F=-\int i_0e^{-at}b_2dt<0.
$$

음함수 정리에 의해 충분히 작은 ε에 대해 F=0을 만족하는 η(ε)가 존재한다. 입력/상태의 margin도 작은 perturbation 아래 유지된다. 따라서 **동일 초기·최종 상태와 동일 총 Heat 입력을 가지면서 중간 진행이 다른 국소 해**도 구성 가능하다. 이때 한동안 빨라진 융해를 다른 구간에서 보상하며, 모든 시간에서 일방적으로 더 빠른 융해를 공짜로 만드는 것은 아니다.

이는 구체적인 숫자의 production witness가 아니다. 연속 입력 표현과 양의 margin을 가정한 국소 존재 정리이며, 표준 음함수 정리의 적용이다.

#### 6. A에서 이제 정말 남는 공백

이 국소 문제가 표준 방법으로 풀린다는 점은 약점이 아니라 **강한 대조를 확보한 것**이다. 새 방법의 필요성은 다음에서 찾아야 한다.

| production으로 돌아갈 때 추가되는 조건 | 아직 하지 못한 일 |
|---|---|
| Heat가 접촉 영역·event 시점·양으로만 주어짐 | u[z]가 실제 허용 input basis의 범위 안에 들어가는 witness |
| Water/Heat의 공간 수송 | 보상 입력이 다른 보호 영역을 훼손하지 않는지 판정 |
| State-Driven Geometry Coupling | 국소 진행 변경이 물길·weights를 바꾸는 효과와 보호 응답 연결 |
| cap/threshold·공급 제한·적응 substep | 연속 해를 이산 production에 옮긴 오차 및 branch 변화 |
| 제작자가 정한 보호·입력 비용 | 동일 요구를 받는 SQP/continuation 대비 새 방법의 필요성 |

첫 새로운 알고리즘 후보는 여전히 **반응 진행으로 구성한 입력 후보를 공간적 보호 제약에 맞게 보정하는 방법**이다. 그러나 이것은 후보 생성+표준 SQP만으로 충분할 수 있다. 기준 대조에도 위 analytic seed를 허용한다. 새 방법만 좋은 초기값을 받게 만들지 않는다.

#### 7. B에서 확인된 구현 의미: 신규 유입은 trigger인가, 소비 자격인가

다음 네 경로를 읽었다.

- [SRProfileLoader.cpp](../../../../../Source/AssetManager/Loaders/SRProfileLoader.cpp): inflowState·fraction·source mask를 읽는다.
- [ValidateSurfaceReactions](../../../../../Source/SurfaceState/Types/SurfaceReactions.cpp): 각 fraction∈[0,1]을 검증한다. 동일 inflowState의 여러 rule fraction 합에 대한 제한은 이 함수에 없다.
- [ProfilePacking.cpp](../../../../../Source/SurfaceState/GPU/Packing/ProfilePacking.cpp): rule별 fraction/source/state를 각각 패킹한다.
- [SurfaceReaction.comp](../../../../../Shaders/Simulation/Solver/SurfaceReaction.comp): request는 신규 유입에서 만들고 공급 비율의 Available은 reaction snapshot의 전체 State다.

이 조합은 **유입으로 촉발된 request를 전체 보유량에서 처리**하는 계약이다. 신규분의 소유권이나 이동 이력을 따로 추적하지 않는다.

##### 코드로부터 유도되는 작은 사례

반응 직전 W 총량이 `old 1 + incoming 1 = 2`이고, 다른 소비/생성이 없다고 하자. 같은 incoming W를 기준으로 W→A, W→B가 각각 fraction=0.75, 소비 계수 1로 선언되어 있다.

request는 각각 0.75이고 총 소비 수요 1.5≤Available 2이므로 공급 비율은 1이다. 최종 W=0.5, A/B는 각각 0.75 증가한다. **신규 incoming은 1인데 총 W 전환은 1.5**다. 총량 보존·비음수성은 지켜지며, 코드의 전체 보유량 제한 계약과 모순되지 않는다.

이는 static code/dataflow의 대수적 결과다. 해당 Profile을 생성하거나 GPU로 재현하지 않았고 모든 asset에서 발생하는 버그라고 판정하지 않았다. 기존 단일 inflowFraction sanity 결과를 무효화하는 것도 아니다.

**정확한 설명:** 현재는 ‘새 유입량에 비례해 반응을 요청한다’. ‘여러 rule을 조합해도 새로 들어온 물질만 소비한다’는 더 강한 의미는 보장하지 않는다. ‘가만히 있는 물은 유입 없이 전환되지 않는다’는 성질과도 구분한다.

#### 8. 신규분만 소비하고 싶다면 표준 배분 문제가 먼저다

이는 **제안된 다른 계약**이며 현재 코드를 바꾸지 않았다. Step 안의 Input/Incoming 자격을 다루며, 사용자가 제외한 축적 순서·층 모델을 재도입하는 제안이 아니다.

선택된 신규분이 N이고 여러 rule이 같은 신규분을 나눠 쓴다면, 최소한 그 inflowState의 총 소비가 N 이하라는 제약이 필요하다. 그러나 단순히 모든 rule의 fraction 합≤1로 제한하는 것은 source mask가 겹치지 않는 경우까지 과하게 제한한다.

positive Input pool P와 Incoming pool I가 있다고 하자. 같은 trigger State에 대해 Input-only rule들의 실제 소비 합이 D_P, Incoming-only가 D_I, 둘 다 허용한 rule들의 소비 합이 D_B이면 다음 조건이 필요하고, 이 두 pool만의 배분에는 충분하다.

$$
D_P\le P,\quad D_I\le I,\quad D_P+D_I+D_B\le P+I.
$$

마지막 rule들은 앞의 두 소비 뒤 남은 pool을 나눠 쓰면 된다. 추가 reactant와 여러 trigger State가 있으면 전체 State의 stoichiometric 공급 제약도 함께 요구해야 한다. 수송 중 old 물량이 얼마나 남았는지와 별개로, incoming은 이번 Step의 새 소비 자격으로만 사용한다는 선택이다.

이런 resource allocation/선형 제약 자체는 새 알고리즘이 아니다. 비례 축소·우선순위·최적화 중 어느 배분을 쓸지는 제작 계약이며, 그 차이가 결과의 재사용성에 중요하다는 증거가 필요하다.

#### 9. ‘규칙을 추가해도 기존 결과는 그대로’라는 보장은 일반적으로 불가능

신규 pool이 1이고 기존 rule A가 모두 소비하도록 정해져 있을 때, A의 소비 1을 그대로 유지하면서 새 rule B에 양의 소비를 허용하면 공급을 초과한다. 따라서 다음 셋을 동시에 항상 만족시킬 수 없다.

1. 모든 기존 rule의 실제 반응량 불변.
2. 새로 추가한 rule도 양의 반응량 확보.
3. 동일한 유한 공급량과 비음수성 유지.

이는 단순한 수지 반례다. 일반적인 조합성은 **기존 결과의 불변**이 아니라 **어떤 자원 경쟁·공유 Geometry 효과가 생기는지 일관된 규칙으로 설명하고 예측하는 것**으로 정의해야 한다. 이전 시스템 기여 가설의 재사용/조합성 평가에서도 두 의미를 혼동하지 않는다.

#### 10. 순수송 의미를 선택할 때도 새 Solver가 필수는 아니다

20의 양방향 flux 사례에서 실제 limiter 이후 flux F_ij,F_ji를 얻었다면

$$
\widetilde F_{ij}=(F_{ij}-F_{ji})_+
$$

로 상쇄한 directed flux를 구성할 수 있다. 각 cell의 net divergence는 원래와 같고 outgoing 합은 늘지 않으므로 **그 수송 단계만의 State 결과·공급 상한**은 유지된다. 그러나 incoming 반응 장부는 달라지므로 이후 coupled 결과는 다른 모델이다.

이것은 표준 대수적 상쇄이며 신규 기여가 아니다. 실제 cache/FP16 결과에서의 재현성·추가 비용을 측정하지 않았다. 또한 stencil 방향성·연속 flux 복원·reaction/Geometry 시간 분할 문제를 해결하지 않는다. R62 같은 기존 구조 보존 drift–diffusion 방법도 관련 대조지만 현재 임의 Profile·Geometry가 그 energy 구조를 갖는다고 가정할 수 없다.

#### 11. 최신 우선순위와 기각 기준

| 방향 | 이번에 강화된 점 | 여전히 부족한 점 | 판단 |
|---|---|---|---|
| A 보호 편집 | 양수 입력·동일 endpoint, 조건부 동일 총 입력의 analytic reference 확보 | 실제 contact 제약·공간 결합·표준 optimizer 대비 필요성 | **주력 탐색 유지**, 기초 feasibility는 진전 |
| B 규칙 의미 일관성 | trigger/eligible-pool 차이를 현 코드에서 분리, 조합성의 불가능 요구 제거 | 표준 배분/kinetics/FV를 넘어서는 새 해법 | **계약 정립을 우선**, 현재는 모델·시스템 후보 |
| E 계산 축소 | 이번에는 새 결과 없음 | 기존 경계/closure 비용 문제 유지 | 별도 제한 탐색 유지 |

다음 비실행 작업은 A의 analytic input이 실제 event basis로 표현 가능한 조건과, 공간 수송을 복원했을 때 필요한 추가 제약이다. B는 현재 trigger 계약과 제안된 신규분 소비 계약 중 어느 의미를 비교하는지 먼저 고정해야 한다. 현재 계약을 조용히 수정하거나 이를 곧바로 연구 Contribution으로 포장하지 않는다.

기각 기준도 유지한다. A가 analytic seed+표준 constrained optimizer로 충분하면 특화 최적화 기여를 낮춘다. B가 표준 배분·거리/시간 kinetics·기존 공간 이산화로 충분하면 중요한 모델 개선은 될 수 있어도 새로운 수치 알고리즘 주장으로 확대하지 않는다.

**후속 조작 범위 보정:** 이 문서의 `u[z]≥0`는 두 실행의 입력이 각각 양수라는 뜻이다. 기존 입력에 대한 차이 `δu`까지 양수라는 뜻은 아니다. 동일 총 입력의 비자명한 구성은 기존 Heat 공급의 시간 재배치를 사용할 수 있어야 한다. 유한 contact basis·한쪽 방향 조작·보호 조건의 국소 차원과 시간별 Geometry 재생 대조는 [[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-22-protected-editability-and-geometry-mediation|22]]를 따른다.

---

## source-authoring-22-protected-editability-and-geometry-mediation

원본: `10_Authoring/22_Protected-Editability-and-Geometry-Mediation.md`

### 보호 편집의 실제 자유도와 공유 형상의 기여 판정

작성: **2026-10-28 / Asia/Seoul**. [[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-19-protected-editing-feasibility-and-response-coordinates|19]], [[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-21-constructive-controls-and-reaction-contracts|21]]의 후속.

**코드 읽기·기호 추론·문헌 대조만 수행했다. 앱·GPU·빌드·수치 실험·학습 및 제품 코드 변경은 없다.** 본문은 제한 모델의 정리, 현재 구현 계약, 미구현 연구 가설을 분리한다.

#### 1. 이번 판단: A는 편집 가능성의 범위를 먼저 밝혀야 한다

보호 편집을 주력 질문으로 유지한다. 그러나 21의 양수 입력 존재는 **연속 시간의 자유로운 Heat 입력**에 대한 결과다. 현 제작 과제는 특정 위치·시간·양의 접촉 사건으로 입력한다. 하나의 수학적 witness를 실제 조작 능력 또는 새로운 최적화 알고리즘으로 해석하지 않는다.

현실적인 과제는 세 질문으로 나눌 수 있다.

1. **가능성:** 허용 입력으로 목표 편집과 보호 요구를 동시에 만족할 수 있는가?
2. **구성:** 가능할 때 계산 예산 안에서 입력을 찾을 수 있는가?
3. **설명:** 불가능하거나 품질이 떨어질 때 어떤 보호 조건·입력 범위가 원인인가?

첫 질문을 최적화 실패와 혼동하면 결과 해석이 틀린다. 하지만 가능성 판정 자체는 일반 제약 제어·선형대수·수지 분석에 선례가 있으므로 그 적용만으로 신규성을 주장하지 않는다. [R5, R60]의 기존 Spacetime Editing 및 제약 궤적 생성과 경쟁해야 한다.

#### 2. ‘양수 입력’과 ‘추가만 가능한 편집’은 다르다

21은 두 입력 `u₀(t)`, `u₁(t)`가 각각 **항상 0 이상**이면서, 같은 최종 상태와 경우에 따라 같은 적분량을 갖는다고 보였다. 그러나 차이 `δu=u₁−u₀`는 양·음의 구간을 모두 가질 수 있다. 이는 기존 Heat 공급의 **시점 재배치**를 허용한다는 뜻이다. 음의 최종 Heat 입력을 사용했다는 뜻은 아니다.

편집 조작이 기존 입력을 건드리지 않고 `δu(t)≥0`인 Heat 추가만 허용한다면

$$
\int_0^T u_1dt-\int_0^T u_0dt=\int_0^T\delta u\,dt\ge0.
$$

총 입력량까지 정확히 같게 요구하고 입력이 보통의 비음수 함수라면 `δu=0` 거의 모든 시간이다. 따라서 **추가만 가능·총량 완전 동일·비자명한 입력 편집**은 이 조작 범위에서 동시에 불가능하다. 접촉 위치를 옮기거나 기존 event 양을 줄일 수 있으면 결론이 달라진다.

21의 국소 열 수지는 한 가지 더 강한 제한을 준다. 같은 최종 Ice/Water/Heat에서 전 구간의 물막이 baseline 이상이고 어느 구간에서는 더 많으면, `γτ>0`인 그 모델에서는 `ΔU=γτ∫Δw dt>0`다. 따라서 **항상 더 이른 융해**와 **같은 총 Heat 입력**도 양립하지 않는다. 같은 예산의 편집은 다른 시간대에서 융해 지연으로 보상해야 한다. 이는 국소 연속 모델의 수지 결과이지 전체 공간 GPU의 정리는 아니다.

#### 3. 유한 접촉 입력에서의 차원 조건

입력 사건의 위치·시간 support를 미리 고정하고 크기 `c∈R^m`만 바꾼다고 하자. 각 사건의 시간·공간 shape를 `φ_j`라 두면 `u(c)=u₀+Σc_jφ_j`다. 보호 관측 `p(c)∈R^r`와 목표 관측 `e(c)`를 같은 production forward 모델에서 평가해야 한다.

매끄러운 branch의 기준 입력 `c=0`에서 `J_P=∂p/∂c`, `J_E=∂e/∂c`라면, 작은 보호 오차 0의 1차 후보 방향은 `J_Pδc=0`이다. `J_P`가 full row rank `r<m`이고 기준 입력이 허용 범위의 내부라면 음함수 정리로 국소 보호 해 집합의 차원은 `m−r`이다. `J_E`가 그 접공간에서 0이 아니면 목표를 바꾸는 작은 보호 편집이 존재한다. 이는 **표준 국소 분석**이다.

반대로 `m=r`이고 `J_P`가 가역이면 기준점 주변에 보호를 정확히 유지하는 연속 자유도는 없다. 먼 비선형 해나 threshold 횡단까지 불가능하다는 뜻은 아니다. 예를 들어 단일 texel의 최종 Ice·Heat·총 입력을 세 개의 독립 제약으로 정하고 조절 가능한 접촉 크기가 세 개뿐이면, 일반적인 매끄러운 경우 국소 자유도 0을 예상한다. 실제 제약의 독립성은 계산 전 가정할 수 없다.

입력 크기가 상한/하한에 닿으면 `ker J_P` 중 허용 입력 cone에 속하는 방향만 사용할 수 있다. 21의 무한차원 bump 증명은 이 유한 basis·부호 제약을 자동으로 만족하지 않는다. 현재 시스템에서 Jacobian이나 편집 가능 영역을 계산하는 구현은 없다.

**판별용 수치 없는 지표:** 보호 Jacobian의 영공간을 `N`이라 하면 `J_EN`이 0인지, 목표 방향이 그 범위에 들어가는지 먼저 볼 수 있다. 이 또한 기존 선형대수이며 branch 변화·큰 편집의 인증은 아니다. 편집 전처리의 비용을 정량 비교할 때 Jacobian 구축과 full rollout 비용을 모두 포함한다.

#### 4. 무엇을 보호할지에 따라 이미 불가능한 요구가 있다

[[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#source-analysis-12-exact-submodels-and-limits|12 §5.2]]가 유도한 현 IceReactionLab의 cap 아래·cavity 0·반응만 있는 한 texel에서, world 요청 높이는

$$
g(i,w)=0.035i+0.0063w,\quad i+w=M,\quad
\delta g=0.0287\,\delta i.
$$

실제 반환되는 local height에는 `StateMath.glsl::accumulationHeight`의 고정 world-to-local 계수가 곱해진다. 같은 texel·같은 시점의 높이를 **정확히 보호**하면서 Ice 양을 바꾸는 것은 이 branch에서 불가능하다. 높이 허용오차가 `ε_g`이면 허용되는 Ice 변화량에도 12의 비례 상한이 생긴다. **새로 발견한 정리라고 기록하지 않는다.** 이번 연구의 역할은 이 제약을 21의 ‘중간 진행 변경’ witness와 결합해 편집 과제 설계에서 배제해야 할 보호 요구를 명확히 한 것이다.

Cap 위에서는 높이가 같아도 저장 State는 다를 수 있다. 따라서 현재 화면이나 높이만 보호하면 이후 이동/반응까지 보호했다고 말할 수 없다. 반대로 이상적인 결정론 모델에서 **전체** State·형상·필요한 cache와 이후 입력이 같다면 미래가 같으므로, 그 경우 미래 probe는 독립 정보를 주지 않는다. 실제 연구 과제의 probe는 **일부 관측만 같을 때 숨은 차이가 후속 동작을 바꾸는지**를 확인하는 데 사용한다.

#### 5. State-Driven Geometry Coupling이 편집 자유도를 늘리는지 분리하는 대조

`SurfaceStateSolver.cpp::RecordSteps`는 현 Step의 수송·반응 후 Height/Geometry/Weight를 갱신한다. 따라서 작은 입력 변화가 형상을 통해 **다음 Step**의 State에 미치는 길이 존재한다. 제한된 매끄러운 notation으로

$$
q_{n+1}=F(q_n,G_n,u_n),\qquad G_{n+1}=B(q_{n+1}),
$$

$$
\frac{\partial q_{n+2}}{\partial u_n}
=\bigl(F_q+F_GB_q\bigr)F_u.
$$

`F_GB_qF_u`는 매개 경로의 **국소 chain-rule 항**이다. 수식 자체는 새로운 방법이 아니고, 이후 Step의 반응·수송 branch가 같다는 제한을 둔다. Geometry가 바뀌어 보인다는 사실만으로 보호 편집에 도움이 된다고 볼 수 없다. 이 항은 목표로 갈 자유도를 늘릴 수도, 보호 제약을 더 강하게 만들어 편집을 막을 수도 있다.

기여 가설을 반증하기 위한 대조는 다음처럼 정의한다. 기준 입력의 **시간별 Geometry 시퀀스** `G⁰_n`을 기록하고, 비교 실행의 State는 수정 입력으로 갱신하되 Solver가 읽는 Geometry만 해당 시점의 `G⁰_n`으로 재생한다. 기준 입력에서 full/재생 모델의 궤적은 같아야 한다. 그러면 perturbation 결과의 차이를 geometry-mediated 경로로 해석하기 쉽다. 이는 초기 Ice 축적 직후의 Geometry 하나를 계속 고정하는 기존 A–E의 E 조건과 **다른 진단**이다. Geometry 시퀀스 재생 기능은 현재 구현되지 않았다.

같은 입력 집합·보호 요구에서 full과 재생 모델의 목표 달성 가능 범위를 비교한다. full만 의미 있는 보호 편집을 허용하면 공유 형상이 **그 편집에 실제로 필요한 기전**이라는 증거가 된다. 양쪽이 같거나 재생 쪽이 낫다면, 해당 편집 사례에서 Coupling을 이점으로 주장하지 않는다. 기전 증거가 나와도 표준 optimizer나 합리적인 모듈식 시스템보다 새로운 연구 기여라는 판정은 별도다.

**왜 결과의 방향을 미리 가정하지 않는가:** 두 입력 변화 `(c₁,c₂)`의 장난감 선형 예에서 목표 변화가 `c₁`이고, Geometry 재생 모델의 보호 변화가 `c₁`이면 정확 보호 아래 목표를 바꿀 수 없다. full 모델의 보호 변화가 `c₁+c₂`라면 `(d,−d)`라는 보상 방향이 생길 수 있다. 반대로 재생 모델에서는 보호 변화가 0인데 full 모델에서 `c₁`이 되면 Coupling이 편집을 **더 어렵게** 만든다. 이 계수는 실제 MDSSP에서 측정한 값이 아니고, 공유 형상 경로가 조합 자유도를 늘린다는 선입견을 배제하기 위한 선형 반례다. 입력이 추가만 가능한 경계에서는 `(d,−d)`도 사용할 수 없다는 §2–3의 제약을 다시 적용해야 한다.

#### 6. A의 실제 Go / No-Go를 더 좁힌다

| 질문 | Go에 필요한 관측 | No-Go 또는 주장 축소 |
|---|---|---|
| 허용 입력의 자유도 | 기존 event 조정·재배치 또는 다수 독립 contact로 보호 방향이 남음 | 양의 추가만 허용하고 동일 예산을 요구해 입력 변화가 원천 차단됨 |
| 편집 가능성 | 목표·보호·조작의 일관된 feasible 사례와 불가능 사례를 구별 | 국소 해의 존재만 보이고 실제 contact basis로는 접근 불가 |
| 공유 형상의 역할 | 기준 Geometry 시퀀스 재생과 full의 보호 가능 영역이 실질적으로 다름 | 영상 차이만 있고 편집 가능 영역은 동일하거나 더 나쁨 |
| 기존법 대비 방법 가치 | 같은 초기값·입력 basis·rollout 예산의 표준 bounded SQP/AD/derivative-free보다 일관된 이점 | 표준법이 핵심 과제를 동일 비용·품질로 해결 |
| 제작 가치 | 보호 조건·실패 이유의 표현이 실제 제작 과제에서 유용함 | 수학적 충돌 설명만 있고 작업 부담 감소가 없음 |

**현재 판정:** A는 가장 목적에 맞는 연구 후보지만, 새 방법과 SIGGRAPH 수준의 기여는 미입증이다. B의 inflowFraction 의미는 편집기의 forward model에 반드시 반영해야 하는 계약이다. E의 계산 축소는 독립된 제한 탐색으로 둔다. 다음 비실행 분석은 가능한 접촉 basis의 실제 범위와 목표·보호 관측의 독립성을 문서/코드에서 감사하는 것이다.

**후속 감사:** [[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-23-contact-control-space-audit|23]]에서 실제 DemoAnim의 `b_heated` Heat Add 48개를 확인했다. 같은 stamp의 서로 다른 두 사건 strength를 `±ε`로 바꾸면, 일정한 transform/Profile 조건에서 각 texel의 시간 합산 요청 InputDelta를 보존하는 유한 입력 방향이 존재한다. 이는 실제 최종 State/보호 목표의 일치나 새 최적화 방법의 성공을 뜻하지 않는다.

---

## source-authoring-23-contact-control-space-audit

원본: `10_Authoring/23_Contact-Control-Space-Audit.md`

### 실제 Contact 입력 공간 감사와 예산 중립 편집의 최소 구성

작성: **2026-10-28 / Asia/Seoul**. [[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-22-protected-editability-and-geometry-mediation|22]]의 ‘실제 조작 가능 범위’ 질문을 코드와 현 자산에서 감사했다.

**코드·자산의 정적 읽기와 대수 분석만 수행했다. 앱·GPU·빌드·수치 실험·학습 및 제품 코드 변경은 없다.** 아래의 48개 사건은 파일에 선언된 입력 좌표이며, 서로 독립적인 최종 결과를 만든다고 검증한 것은 아니다.

#### 1. 현재 입력의 실제 계약

| 항목 | 확인한 코드·자산 | 연구에 주는 의미 |
|---|---|---|
| 사건 형식 | `Source/SurfaceState/State/SurfaceInput.h::TSurfaceContactInput` | instance·State·world 위치·반경·양·falloff·Add/Remove·shape 등을 가진 discrete event. 음의 Strength 자체는 허용하지 않음 |
| Scene 초기 입력 | `Source/Scene/SceneLoader.cpp::Load`, `Source/SurfaceState/SurfaceStateSystem.cpp::QueueInitialContacts` | `initialContacts`는 양의 strength와 world stamp를 읽으며 `Operation` 필드를 설정하지 않아 기본 Add. Scene 재시작 때 다시 queue |
| 시간별 입력 | `Source/Scene/DemoAnimation.cpp::LoadDemoAnimation`, `Source/Scene/Scene.cpp::AdvanceDemoAnimation` | `.DemoAnim`의 sorted `contactEvents`가 시간·위치·양과 Add/Remove를 선언. 지정 시간이 진행 구간을 통과한 frame에 한 번 발화. Event 시간 자체를 연속 입력으로 해석하지 않음 |
| 접촉의 실제 raster | `Source/SurfaceState/Contact/SurfaceContactApplication.cpp::ApplyPendingContacts` | 기본 `Texel.Position`의 world 변환을 사용하며 현재 DynamicGeometry를 읽지 않음. 같은 transform/Profile/stamp 조건에서 State가 바뀌어도 요청 입력의 공간 계수는 같음 |
| Step에서의 의미 | `Shaders/Simulation/Solver/SurfaceSolverPass2.comp::main` | 같은 Step의 signed `InputDelta`를 합산해 `max(Current+EventInput+Incoming−Outgoing−Decay,0)`에 적용. `positiveInput` 장부는 `max(EventInput,0)`이며 Step 뒤 delta를 비움 |

`SurfaceContactApplication.cpp`에서 `Strength≥0`을 검사하고 `Remove`면 부호를 음수로 바꾼 뒤 접촉별 값을 합산한다. 같은 Step·texel·State에 Add와 Remove를 보내면 먼저 **signed net delta**로 합쳐진다. Remove는 현재 보유 State를 깎는 새 사건이다. 이미 발생한 융해·형상·수송을 시간 역행해 지우는 조작이 아니다.

#### 2. IceReactionLab에 이미 존재하는 유한 시간 입력 basis

현재 [IceReactionLab.DemoAnim](../../../../../Assets/Animations/IceReactionLab.DemoAnim)을 정적으로 읽었다. `b_heated`의 Heat Add는 `t=0.25,0.50,…,12.00`의 **서로 다른 48개 시점**에 하나씩 있고, 모두 `strength=0.2`다. 48개의 target·State·operation·shape·localPosition·radius·secondaryRadius·depth·falloff·rotationDegrees가 같다. Animation track은 camera position 하나이고 이 instance의 transform track은 없다. 원래 Scene의 `b_heated` 초기 Heat contact는 별도의 strength 2.0이며 여기서는 고정한다.

`c_cooled`의 Heat Add 48개와 Waterfilm Add 47개는 **다른 instance**의 사건이다. `b_heated` 한 instance의 보호 편집 자유도로 합쳐서 96개라고 세지 않는다. `a_finite`에는 이 DemoAnim의 Heat 시간 사건이 없다.

이 입력은 파일에 strength/time/shape/위치를 다시 지정해 로드할 수 있는 **offline scripted authoring 좌표**다. 현재 최적화기가 자동으로 이 사건을 조정하거나 기존 사건을 GUI timeline에서 직접 수정하는 기능을 확인한 것은 아니다. 반복 평가에는 파일 설정·재로드·State 재시작이 필요할 수 있으며, 이를 사용자 생산성 증거로 여기지 않는다.

#### 3. 실제 자산으로 표현 가능한 정확한 입력 예산 중립 방향

기존 48개 Heat Add strength를 `a_j=0.2`라 한다. 두 시점 `j≠k`를 골라

$$
a'_j=0.2+\epsilon,\qquad a'_k=0.2-\epsilon,\qquad 0<\epsilon\le0.2
$$

로 변경하고 나머지는 유지할 수 있다. 파일 parser의 비음수 Strength 조건을 만족한다. **현재 사건을 추가하는 것이 아니라 기존 두 사건의 양을 재배치**하는 작업이다. 새 둘의 Operation은 여전히 Add이므로 Remove의 다른 반응 의미가 끼지 않는다.

같은 instance transform·Profile/InputFactor·stamp를 고정하면 `ApplyPendingContacts`의 한 texel `x`에 대한 요청량은 `D_{x,j}=a_j K_x`다. 여기서 `K_x`는 그 texel의 footprint/falloff/InputFactor·면적 계수다. 48개가 같은 stamp이므로

$$
\sum_j D'_{x,j}-\sum_j D_{x,j}=K_x(\epsilon-\epsilon)=0
$$

가 **각 texel마다** 성립한다. 즉 이 입력 family는 전체 strength 합 9.6뿐 아니라 각 texel의 **시간 합산 요청 InputDelta**를 보존한다. 고정된 동일 stamp의 strength만 조정하면 48차원 계수 공간에 `Σδa_j=0`인 **47차원 명목상 예산 중립 방향 공간**이 있다. 양의 strength margin 0.2는 충분히 작은 양·음의 계수 편집을 허용한다.

이는 실수 대수와 고정 stamp 조건의 결과다. 부동소수점 업로드·합산의 bitwise 동일성을 보장하지 않는다. 또 **시간별** 입력, retention, 반응, 냉각, Decay, Geometry, 최종 State가 같다는 결과는 전혀 아니다. 21의 동일 최종 State witness가 이 48차원 basis 안에 놓이는지도 확인하지 않았다. 따라서 22의 실제 입력 gap 중 ‘예산 중립의 유한 입력 방향이 존재하는가’만 닫혔다.

##### 첫 변경 사건에서 보장되는 좁은 차이

`j`가 `k`보다 이른 사건이면, `j` 직전까지 두 실행의 입력·State·Geometry는 동일한 결정론적 경로다. `SurfaceSolverPass1.comp::main`은 현재 State에서 Decay를 빼고 남은 양으로 outgoing을 제한한다. 따라서 Pass 2의 입력 전 기본량 `Current+Incoming−Outgoing−Decay`는 정상적인 비음수·유한 경로에서 0 이상이다. 같은 spatial stamp의 첫 Add를 `+ε`로 바꾸면, 해당 stamp가 실제로 닿는 texel에서 **첫 변경 Step의 반응 직전 Heat snapshot**은 실수 대수상 `εK_x` 증가한다. 뒤의 `k` 사건은 이 시점에 영향을 줄 수 없다. 부동소수점의 bitwise 차이·반응 후 State 증가·Ice/Geometry 변화는 별도 문제다.

이 사실은 제안된 입력 차이가 Solver에 도달하는지에 관한 **최소 causal sanity**다. 최종 보호 편집의 feasibility를 증명하지 않는다. `k` 이전의 측정 창을 잡으면 초기 변화와 나중 보상 사건의 효과를 분리할 수 있다.

#### 4. 공간 자유도와 시간 자유도를 혼동하지 않는다

이 48개 사건은 같은 spatial stamp `K_x`와 서로 다른 시점을 가진다. 입력의 공간 shape는 하나이고, time-amplitude 좌표만 48개다. 장면 내 목표·보호 영역의 spatial 분리에는 Heat 수송·반응·동적 형상의 **시간별 응답 차이**가 필요하다. 48개 strength가 최종 관측에 48개의 독립적인 방향을 제공한다는 결론은 불가능하다.

매끄러운 한 branch에서 전체 State의 Step 선형화를 `A_n`, 사건의 입력 방향을 `B_jK`, 최종 관측을 `Cq_T`로 쓰면 각 strength의 1차 최종 응답 열은

$$
J_j=C A_{T-1}A_{T-2}\cdots A_{j+1}B_jK.
$$

같은 `K`여도 서로 다른 시점의 전파 연산자 때문에 `J_j`들이 달라질 수 있다. 그러나 모두 거의 평행하면 사건 48개가 있어도 유효 편집 방향은 작다. `A_n`에는 기준 경로의 반응·수송·형상 매개 효과가 들어가야 한다. 이 식은 표준 선형화/도달성 표현이지 새 알고리즘이나 현재 계산된 Jacobian이 아니다. 유효 rank만이 아니라 입력 범위·허용오차·조건수도 평가해야 한다.

**접촉 좌표의 제한:** 현 raster는 `DynamicGeometry`가 아닌 정적 texel 위치를 사용한다. 따라서 State가 만든 돌출부를 따라 stamp support가 움직이는 입력 모델을 현재 구현의 기능으로 주장할 수 없다. 같은 위치의 script 입력을 비교할 때 `K`가 고정된다는 이점이 있지만, 제작자가 현재 보이는 형상에 직접 접촉한다고 해석하면 다른 입력 의미다. 동적 표면에서의 접촉 불일치가 시각적·수치적 문제를 만들었는지는 실행으로 확인하지 않았다. 그 자체를 독창적인 기여 후보로 승격하지 않는다.

22의 `J_P` rank/허용 cone 조건을 이 구체적인 계수 벡터에 적용할 수 있다. 예를 들어 같은 총 요청 InputDelta 조건 한 개를 포함하면, 보호 제약 추가 전의 내부 방향은 47개다. 이후 최종 Ice·Heat·Water 분포 등 보호 지표를 더하면 실질 자유도는 Jacobian의 rank 및 사건이 닿는 branch에 따라 줄어든다. 다수 텍셀의 동일 분포 보호는 47개를 모두 고정할 수도 있다. 현 Jacobian·rank는 계산하지 않았다.

기존 실험의 `D–E` 분포 차이와 이 입력 basis의 보호 편집 가능성은 서로 다른 증거다. 전자는 forward coupling 기전이고, 후자는 입력→목표/보호 응답의 역문제다.

특히 **두 사건의 `±ε` 대조는 입력 경로 sanity**로만 쓴다. 21의 닫힌 국소 모델에서조차 총 입력·최종 Ice·최종 Heat가 세 독립 제약인 regular branch라면, strength 두 개만 조절해서 비자명한 정확 보호 편집을 기대할 수 없다. 같은 세 제약 아래 국소 자유도를 남기려면 일반적으로 네 개 이상의 독립 strength가 필요하다. 공간 수송이 있으면 최종 Water 분포 등 독립 제약이 더 생길 수 있어 이 숫자를 production 보장으로 옮기지 않는다. 48개 입력의 실제 효용은 보호 Jacobian의 rank와 허용 범위에 달려 있다.

#### 5. 앞으로의 연구 판정에 바로 쓸 축소 과제

1. **Sanity 과제:** 같은 stamp의 두 Heat strength를 `±ε`로 조정해 저장된 사건 형식과 명목상 texel별 입력 예산을 유지한다. 48개 전체를 조정하기 전 입력 경로의 정확성을 판정할 수 있다. 실행은 현재 중지 상태다.
2. **핵심 가능성 질문:** 48개 strength 중 제한된 일부를 조정해 정해진 중간 Ice/Water/Geometry 목표를 바꾸면서 지정한 최종 또는 이웃 영역 관측을 보호할 수 있는가? 성공을 미리 가정하지 않는다.
3. **기전 질문:** 기준 Geometry 시퀀스 재생 대조와 full coupling에서 같은 contact basis·목표·보호를 사용한다. 형상 경로가 보호 편집의 달성 범위를 넓히는지, 좁히는지 판정한다.
4. **기여 판단:** analytic seed를 포함한 표준 bounded 최적화와 같은 파일 조작·같은 rollout budget을 준다. 새 방법이 필요하지 않으면 알고리즘 기여 주장을 낮춘다.

**현재 결론:** 현 코드/자산에는 실제로 표현 가능한 예산 중립의 시간 입력 family가 있다. 이는 21의 연속 입력 해를 production에서 재현한 것이 아니고 보호 편집의 성공 증거도 아니다. 지금 가장 날카로운 미해결 질문은 **이 제한된 temporal basis가 State-Driven Geometry Coupling 아래에서 실제 보호 편집 방향을 남기는가**다.

---
