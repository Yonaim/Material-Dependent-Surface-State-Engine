# Contribution 후보

2026-11-03 기준. **확정된 후보는 없다.** 유력도는 연구 탐색 순위이며 논문 채택 확률이 아니다.

| 순위·유형 | 연구 질문과 현재 근거 | 강한 심사 반론 / 기각 기준 |
|---|---|---|
| **1 · 제작/알고리즘** 보호 편집·목표 진행 | 실제 Heat contact 48개에 제한적 조정 여지와 연속 모델의 조건부 해가 있다. 공유 형상을 통해 후속 효과를 지키며 목표를 바꿀 수 있는지는 미검증 | 일반 constrained optimizer·다중 시작으로 충분할 수 있다. 동일 조작 범위·예산에서 우위가 없으면 중단 |
| **2 · 모델/알고리즘** 반응·수송 의미 일관성 | 현 `inflowFraction`은 신규 유입으로 요청하고 전체 보유 State로 소비를 제한한다. 이동 방향·거리·분할에 따른 제한 모델 반례가 있다 | 표준 거리 보정·kinetics·자원 배분·operator splitting으로 충분하면 독립 기여 아님 |
| **3 · 알고리즘** 미래 반응을 고려한 계산 축소 | 닫힌 제한 모델의 오차 상한과 생략 충분조건을 유도했다 | 기존 sparse/LOD와 동등하거나 판정 비용이 fine 갱신만큼 크면 중단 |
| **보류 · 시스템** 공통 State–Reaction–Geometry 조합 | 결합 기전은 확인했다 | Günther 2012와 합리적 모듈식 대조 대비 중요한 표현·제작 이점 미확인 |

GPU·다중 State·flux limiter·Normal Map 적분·cavity·sparse·개발용 속도 개선은 단독 신규성이 아니다. State-Driven Geometry Coupling 자체도 넓은 최초성으로 주장하지 않는다. 물리 부피 모델 전환과 chart 대표 높이의 독립 기여화는 현재 권고가 아니다. 축적 순서/층 표현 방향은 제외한다.

상세: [[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-23-contact-control-space-audit|입력 감사]], [[90_Research-Archive/Research-Notes-2026-10/03_Solver/01_Forward-and-Reaction-Semantics#source-solver-20-transport-reaction-semantics-and-refinement|반응·수송]], [[90_Research-Archive/Research-Notes-2026-10/03_Solver/03_Error-Control-and-Sparse#source-analysis-16-low-cost-certificates-and-sleep-horizons|계산 축소]], [[90_Research-Archive/Research-Notes-2026-10/04_Literature/01_Related-Work#source-reference-07-related-work|문헌]]. 이전 후보 판단은 [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-03-contribution-candidates|원본]]에 보존했다.
