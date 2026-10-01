# ADR 0046 — Transport 역할별 Profile 키와 곡률 감쇠 제거

> **한 줄 요약:** 이동·홈 이탈·자연 감소를 Profile 키에서 구별하고, 중복되는 선택적 곡률 감쇠를 Transport에서 제거한다.

- 분류: **Simulation**
- Status: **Implemented**
- Date: 2026-10-01
- 관련 문서: [[0002-Transport-Drive-and-Weight|ADR 0002]], [[0015-Geometry-Driven-Transport|ADR 0015]], [[0019-Optional-Curvature-Transfer-Weight|ADR 0019]], [[0045-Directional-Cavity-Transport-Retention|ADR 0045]]

## Context

기존 `CurvatureWeight`는 오목·볼록의 부호를 버리고 양방향 전달을 함께 낮추는 선택 항목이었다. 홈으로 들어오는 흐름까지 막으므로, 방향별 홈 이탈 억제와 역할이 겹치고 튜닝 의미가 불명확했다. 높이 차이는 `GeometryDrive`의 `HeightDrive`에 이미 반영되며, 초기 ADR의 `TransferWeight`에도 별도의 `HeightWeight`는 없었다. Profile의 기존 키 이름은 이동과 보유·감소의 차이를 드러내지 못했다.

## Decision

1. `TransferWeight = DistanceWeight × NormalWeight × ProfileBoundaryWeight`로 단순화한다. 정적 캐시와 적층 피드백의 동적 GPU 갱신에 같은 식을 적용한다. 선택적 `CurvatureWeight` 토글과 flag를 제거한다. 곡률 진단 데이터는 Geometry 표현에 남긴다.
2. 높이 차이와 중력 방향은 `GeometryDrive`에서 이동을 구동한다. `cavityExitResistanceFactor`는 source의 오목도가 target보다 높을 때 합산 raw flux를 `1 − factor × max(Concavity_source − Concavity_target, 0)`으로 낮춘다. `cavityDecayProtectionFactor`는 자연 감소에만 적용한다.
3. `.SRProfile` version 4에서 `saturationSpreadFactor`, `gravityFlowFactor`, `cavityExitResistanceFactor`, `cavityDecayProtectionFactor`를 사용한다. 이들은 차례로 version 3의 `saturationTransferFactor`, `geometryTransferFactor`, `cavityTransportRetentionFactor`, `cavityRetentionFactor`와 같은 값·범위를 갖는다. version 3 파일은 이전 키로 계속 읽고 version 1·2는 거부한다. `cavityExitResistanceFactor`를 생략하면 0이다.
4. GPU Profile ABI와 내부 C++ 필드명은 유지한다. JSON 로더에서 version별 키를 같은 런타임 필드로 매핑한다.

## Alternatives Considered

- 선택적 `CurvatureWeight` 유지: 홈 진입도 감쇠하므로 사용자가 원하는 방향별 보유와 의미가 다르다.
- `HeightWeight`를 `TransferWeight`에 추가: 기존 `HeightDrive`와 높이 차이 효과가 중복된다.
- 기존 Profile 키에 별칭만 추가: 새 파일에서도 역할이 모호한 이름을 계속 허용해 schema를 복잡하게 만든다.

## Consequences

- 동일한 초기 조건과 기존 기본 OFF 설정에서는 곡률 항 제거가 전달량을 바꾸지 않는다.
- version 4에서는 역할별 키를 사용한다. version 3 파일은 호환 로딩되지만 저장 파일은 version 4로 이전한다.
- 홈을 따라 모여 내려가는 정도는 `GeometryDrive`, 입력 형상과 방향별 이탈 저항, 포화도 확산의 균형으로 결정된다. 이 변경만으로 모든 형상에서 해당 시각 효과가 보장되지는 않는다.

## Related

- [[../04_Architecture/0003_Assets-and-Profiles|Assets and Profiles]]
- [[../04_Architecture/0006_Surface-State-Update|Surface State Update]]
- [[0019-Optional-Curvature-Transfer-Weight|ADR 0019 — 이전 선택 항목]]
- [[0045-Directional-Cavity-Transport-Retention|ADR 0045 — 방향별 홈 이탈 억제]]
