# Decision 0023 — 방향별 홈 이탈 억제와 Decay 계수 분리

> **한 줄 요약:** 오목한 곳에서 바깥으로 나가는 이동을 줄이는 계수를 자연 감소 설정과 분리한다.

- 분류: **Simulation**
- Status: **Implemented**
- Date: 2026-10-01
- 관련 문서: [[05_Decisions/0008_Normalized-Transport-Factors|Decision 0008 — 전달 계수]], [[05_Decisions/0022_Macro-Meso-Concavity-Field|Decision 0022 — 통합 오목도]], [[05_Decisions/0024_Transport-Role-Names-and-Curvature-Removal|Decision 0024 — 곡률 감쇠 제거]]

> **후속 결정 (2026-10-01):** [[05_Decisions/0024_Transport-Role-Names-and-Curvature-Removal|Decision 0024]]은 선택적 CurvatureWeight를 제거하고 `.SRProfile` 키를 역할별 이름으로 변경했다. 아래 기존 키와 비교용 항 유지 결정은 당시 기록이다.

## 쉽게 읽기

오목한 곳에서 밖으로 나가는 흐름을 줄이는 State별 계수를 자연 감소 계수와 분리한 초기 결정을 기록한다. 현재 필드명과 곡률 감쇠 정책은 Decision 0024을 따른다.

## Context — 왜 필요했나

현재 ConcavityWeight × cavityRetentionFactor는 texel의 Decay만 줄인다. 선택적 CurvatureWeight는 mean curvature 절댓값을 양 끝점에 대칭적으로 적용하며 기본 OFF다. 따라서 오목한 곳의 물질이 밖으로 빠져나가는 흐름과 밖에서 안으로 들어오는 흐름을 구분하지 못한다. GeometryDrive는 회전된 물체의 월드 높이 차와 중력 방향을 반영하지만, SaturationDrive는 별도로 상태 차이를 평준화한다. Decay 감소만으로 뒤집힌 홈의 진흙 이동을 억제할 수 없다.

## Decision — 무엇을 정했나

1. Decision 0022의 ConcavityWeight를 Transport에도 제공한다. 최초 결정은 자연 감소 계수 `cavityRetentionFactor`와 별도로 v3 Profile 키 `cavityTransportRetentionFactor`를 두고, 생략 시 0으로 처리하는 것이었다. 현재 v4 키는 각각 `cavityDecayProtectionFactor`와 `cavityExitResistanceFactor`이며, v3 파일은 이전 키로 호환해 읽는다 (Decision 0024).
2. 이동 억제는 방향별 source→target raw flux에 적용한다. 오목한 영역으로 진입하거나 같은 홈을 따라 움직이는 경로보다, 더 낮은 오목도 영역으로 이탈하는 경로를 감쇠한다. 첫 검증 후보는 exit(i→j)=max(Concavity_i−Concavity_j, 0), retention=1−cavityTransportRetentionFactor×exit다. 구체 함수는 그릇 바닥이 평탄한 Mesh와 긴 홈 테스트에서 이탈·내부 이동을 확인한 뒤 확정한다.
3. 홈 보유 효과가 필요한 State에서는 GeometryDrive와 SaturationDrive를 합친 raw flux에 방향별 계수를 적용한다. GeometryDrive만 줄이면 SaturationDrive 경로로 같은 State가 계속 빠져나갈 수 있다. 이 계수는 State 양을 없애지 않고 기존 outgoing·incoming 및 alpha 보존 경로 안에서 유량만 조절한다.
4. Mud와 WaterFilm·Wetness는 동일한 물성값을 강제하지 않는다. 데모의 State별 계수는 각 .SRProfile에서 조절한다. 뒤집힌 홈에 물막까지 무조건 붙는 효과를 기본 물리 규칙으로 선언하지 않는다.
5. 당시 대칭적인 CurvatureWeight는 비교용 선택 항목으로 유지했다. 새 방향별 보유 항을 기존 CurvatureWeight나 Decay 계수에 합치지 않는다. 적층으로 홈이 메워지면 Decision 0022의 동적 오목도 갱신 결과를 다음 step에서 사용한다. 비교용 곡률 항은 후속 Decision 0024에서 제거했다.

## Implementation — 현재 구현

최초 구현은 `.SRProfile` v3의 선택 필드 `cavityTransportRetentionFactor`를 사용했다. 현재 v4는 `cavityExitResistanceFactor`를 사용하며 v3 파일은 loader가 이전 키로 호환해 읽는다 (Decision 0024). 기존 48바이트 GPU Profile 레코드의 `AccumulationThickness.y`에 저장하고 UI에서 조절한다. 두 Solver pass의 raw flux에 `1 − factor × max(sourceConcavity − targetConcavity, 0)`을 적용한다. 당시 GPU 검사는 홈 이탈 억제·진입 허용·총량 보존·계수 0 호환을 cache ON/OFF에서 확인했다. Decision 0025 이후 재계산 경로만 남는다. 실제 데모의 형상별 강도는 시각 조정 대상이다.

## Alternatives Considered — 다른 방법

- **Decay의 cavityRetentionFactor를 Transport에 그대로 곱하기:** 두 현상의 강도를 독립적으로 조절할 수 없고, decay 감소가 이동 억제와 같은 의미가 아니다.
- **기존 절댓값 CurvatureWeight만 활성화:** 오목·볼록 및 source→target 방향을 구분하지 않아 홈 밖 이탈만 선택적으로 줄이지 못한다.
- **State 전체 transfer factor만 낮추기:** Mud 전체를 느리게 만들 수 있지만 홈 안·밖의 상대적인 보유 효과가 없다.
- **별도 State별 방향 보유 계수:** 채택한다.

## Consequences — 결정의 영향

- .SRProfile loader·검증·GPU 레코드·shader와 Profile Tuning UI에 새 계수가 필요하다. 누락 시 0으로 처리하는 기존 파일 호환 정책을 시험한다.
- 같은 초기량에서 평면·그릇·홈·볼록부의 양방향 이동을 비교한다. source와 target을 바꾸었을 때 이탈 억제가 동일한 대칭 Weight로 환원되지 않아야 한다.
- 닫힌 이웃 그래프에서 Decay/Input을 끄면 State 총량이 보존되어야 한다. 기존 alpha가 source 보유량을 넘는 유출을 막는 계약도 유지한다.
- 계수 0에서는 기존 Solver 결과를 재현한다. 실제 진흙 보유 정도와 홈의 평탄한 바닥 문제는 시각 데모와 수치 fixture에서 검증한다.
- ConcavityWeight를 Decay에만 쓰던 초기 결정을 이 Decision의 독립 방향별 Transport 항으로 확장한다. 당시 비교용으로 남긴 대칭 곡률 항은 후속 Decision 0024에서 제거했다.

## Related — 관련 문서

- [[05_Decisions/0024_Transport-Role-Names-and-Curvature-Removal|Decision 0024 — 곡률 감쇠 제거]]
- [[05_Decisions/0009_Texel-Area-and-State-Amounts|Decision 0009 — 총량 보존]]
- [[05_Decisions/0022_Macro-Meso-Concavity-Field|Decision 0022 — 통합 오목도]]
- [[05_Decisions/0020_Scene-Referenced-Demo-Animation|Decision 0020 — 씬별 데모]]
