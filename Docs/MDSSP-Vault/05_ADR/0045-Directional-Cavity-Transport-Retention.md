# ADR 0045 — 방향별 홈 이탈 억제와 Decay 계수 분리

> **한 줄 요약:** 통합 ConcavityWeight를 홈 밖으로 나가는 Transport의 방향별 보유 입력으로 사용하되 Decay의 cavityRetentionFactor와 독립된 State별 계수를 둔다.

- 분류: **Simulation**
- Status: **Accepted — 구현 전**
- Date: 2026-10-01
- 관련 문서: [[0016-Transport-Transfer-Weights|ADR 0016 — TransferWeight]], [[0019-Optional-Curvature-Transfer-Weight|ADR 0019 — 대칭 곡률 감쇠]], [[0044-Macro-Meso-Concavity-Field|ADR 0044 — 통합 오목도]]

## Context

현재 ConcavityWeight × cavityRetentionFactor는 texel의 Decay만 줄인다. 선택적 CurvatureWeight는 mean curvature 절댓값을 양 끝점에 대칭적으로 적용하며 기본 OFF다. 따라서 오목한 곳의 물질이 밖으로 빠져나가는 흐름과 밖에서 안으로 들어오는 흐름을 구분하지 못한다. GeometryDrive는 회전된 물체의 월드 높이 차와 중력 방향을 반영하지만, SaturationDrive는 별도로 상태 차이를 평준화한다. Decay 감소만으로 뒤집힌 홈의 진흙 이동을 억제할 수 없다.

## Decision

1. ADR 0044의 ConcavityWeight를 Transport에도 제공한다. Decay의 cavityRetentionFactor는 자연 감소 조절에만 사용한다. .SRProfile State에는 별도 0–1 계수인 cavityTransportRetentionFactor를 추가하고, 지정하지 않은 기존 Profile은 0으로 해석해 기존 Transport 결과를 보존한다. 파일 버전·호환 처리와 GPU Profile ABI 변경을 함께 검증한다.
2. 이동 억제는 방향별 source→target raw flux에 적용한다. 오목한 영역으로 진입하거나 같은 홈을 따라 움직이는 경로보다, 더 낮은 오목도 영역으로 이탈하는 경로를 감쇠한다. 첫 검증 후보는 exit(i→j)=max(Concavity_i−Concavity_j, 0), retention=1−cavityTransportRetentionFactor×exit다. 구체 함수는 그릇 바닥이 평탄한 Mesh와 긴 홈 테스트에서 이탈·내부 이동을 확인한 뒤 확정한다.
3. 홈 보유 효과가 필요한 State에서는 GeometryDrive와 SaturationDrive를 합친 raw flux에 방향별 계수를 적용한다. GeometryDrive만 줄이면 SaturationDrive 경로로 같은 State가 계속 빠져나갈 수 있다. 이 계수는 State 양을 없애지 않고 기존 outgoing·incoming 및 alpha 보존 경로 안에서 유량만 조절한다.
4. Mud와 WaterFilm·Wetness는 동일한 물성값을 강제하지 않는다. 데모의 State별 계수는 각 .SRProfile에서 조절한다. 뒤집힌 홈에 물막까지 무조건 붙는 효과를 기본 물리 규칙으로 선언하지 않는다.
5. ADR 0019의 대칭적인 CurvatureWeight는 비교용 선택 항목으로 유지한다. 새 방향별 보유 항을 기존 CurvatureWeight나 Decay 계수에 합치지 않는다. 적층으로 홈이 메워지면 ADR 0044의 동적 오목도 갱신 결과를 다음 step에서 사용한다.

## Alternatives Considered

- **Decay의 cavityRetentionFactor를 Transport에 그대로 곱하기:** 두 현상의 강도를 독립적으로 조절할 수 없고, decay 감소가 이동 억제와 같은 의미가 아니다.
- **기존 절댓값 CurvatureWeight만 활성화:** 오목·볼록 및 source→target 방향을 구분하지 않아 홈 밖 이탈만 선택적으로 줄이지 못한다.
- **State 전체 transfer factor만 낮추기:** Mud 전체를 느리게 만들 수 있지만 홈 안·밖의 상대적인 보유 효과가 없다.
- **별도 State별 방향 보유 계수:** 채택한다.

## Consequences

- .SRProfile loader·검증·GPU 레코드·shader와 Profile Tuning UI에 새 계수가 필요하다. 누락 시 0으로 처리하는 기존 파일 호환 정책을 시험한다.
- 같은 초기량에서 평면·그릇·홈·볼록부의 양방향 이동을 비교한다. source와 target을 바꾸었을 때 이탈 억제가 동일한 대칭 Weight로 환원되지 않아야 한다.
- 닫힌 이웃 그래프에서 Decay/Input을 끄면 State 총량이 보존되어야 한다. 기존 alpha가 source 보유량을 넘는 유출을 막는 계약도 유지한다.
- 계수 0에서는 기존 Solver 결과를 재현한다. 실제 진흙 보유 정도와 홈의 평탄한 바닥 문제는 시각 데모와 수치 fixture에서 검증한다.
- ADR 0016의 ConcavityWeight를 Decay에만 쓰는 초기 결정을 이 ADR의 독립 방향별 Transport 항으로 확장한다. ADR 0019의 비교용 대칭 곡률 항은 그대로 남는다.

## Related

- [[0016-Transport-Transfer-Weights|ADR 0016 — 기존 대칭 Weight]]
- [[0019-Optional-Curvature-Transfer-Weight|ADR 0019 — 선택적 곡률 감쇠]]
- [[0030-Texel-Area-and-State-Amounts|ADR 0030 — 총량 보존]]
- [[0044-Macro-Meso-Concavity-Field|ADR 0044 — 통합 오목도]]
- [[0042-Scene-Referenced-Demo-Animation|ADR 0042 — 씬별 데모]]
