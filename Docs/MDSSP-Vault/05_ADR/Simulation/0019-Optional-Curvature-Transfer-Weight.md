# ADR 0019 — 선택적 사전 계산 CurvatureWeight

> **한 줄 요약:** 선택형 CurvatureWeight는 기본 OFF이며, 활성화하면 Meso mean curvature 기반 가중치를 캐시에 적용한다.

- Status: **Accepted**
- Date: 2026-09-28

## Context

초기안은 CurvatureWeight를 1.0으로 유지했다. Meso 전처리는 signed mean curvature를 이미 생성하지만 Transport에는 연결하지 않았다. 고정값과 곡률 기반 전달 감쇠를 같은 UI에서 비교할 필요가 있다. NormalWeight와 효과가 중복될 가능성은 여전히 남아 있다.

## Decision

- Solver debug의 `CurvatureWeight (precomputed)`를 기본 OFF로 제공한다. OFF는 기존 고정값 1.0이며 ON은 사전 계산된 Meso mean curvature `H`를 사용하는 CPU TransferWeight cache를 선택한다.
- 간선 가중치는 `1 / (1 + 0.5 × (abs(H_i) + abs(H_j)) × dLocal(i,j))`다. `dLocal`은 `Position + Normal × MesoVirtualHeight` 사이의 mesh-local 직선 거리다. H는 1/mesh-local length이므로 곱은 무차원이다. 유한하지 않은 곡률·거리는 해당 간선 가중치를 0으로 만든다.
- 양 endpoint 교환에 대칭이며 평탄 영역은 1, 곡률 크기가 커지면 전달을 감쇠한다. 오목·볼록의 부호를 구별하지 않고 ConcavityWeight를 재사용하지 않는다. Gaussian curvature와 Macro curvature는 이 옵션의 입력이 아니다.
- 이는 비교용 감쇠 규칙이며 물리적인 응집·응결 모델의 완성을 뜻하지 않는다. Non-uniform instance scale로 변환한 world curvature도 아니다. instance scale과 무관한 mesh-local 요철 지표를 채택한다.
- 토글은 cache를 dirty로 만들고 다음 Solver step에서 모든 instance cache를 다시 만든다. Pause 중 변경도 Step/재개 때 적용한다. 현재 State는 유지한다.
- 추가 GPU buffer 또는 Profile ABI 변경은 없다. 기존 TransferWeights에 곱해 저장한다.

## Alternatives Considered

- 고정 1.0만 유지: 기존 전달과 NormalWeight 중복 위험을 피하지만 같은 UI에서 곡률 효과를 비교할 수 없다. 기본값으로는 유지한다.
- ConcavityWeight 재사용: 기존 Decay retention과 결합되고 볼록 영역을 표현하지 못하므로 채택하지 않는다.
- 주곡률·주방향 기반 감쇠: ADR 0016의 후속 검토 항목이며 현재 보유 데이터보다 넓은 전처리가 필요하다. 이번 옵션에는 채택하지 않는다.

## Consequences

- 기본 결과는 기존 중립 곡률 동작을 유지한다. ON 결과는 NormalWeight와 추가 감쇠가 중복될 수 있으므로 같은 초기 State에서 비교해야 한다.
- Meso fit이 불가능해 H가 0인 영역은 이 옵션에서도 중립이다. Macro curvature 및 적층에 따른 동적 곡률은 아직 구현되지 않았다.
- 토글 시 기존 CPU cache rebuild와 queue idle 비용이 발생한다. 매 rawFlux에서 곡률을 계산하지 않는다.
- 검증은 기본값, 평탄/오목/볼록 endpoint, 양방향 대칭, instance scale 독립성, NaN 차단을 포함한다. 시각적·물리적 이득은 별도 검증 대상이다.

## Related

- [[0016-Transport-Transfer-Weights|ADR 0016]]
- [[0018-Normal-Map-Meso-Geometry|ADR 0018]]
- [[../../04_Architecture/0010_UI-Interface|UI Interface]]
- [[../../04_Architecture/0007_Surface-Solver-Cache|Surface Solver Cache]]
