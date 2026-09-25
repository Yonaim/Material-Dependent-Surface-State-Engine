# ADR 0006 — SRProfile 기반 동적 State Registry

- 상태: **Accepted**
- 날짜: 2026-09-25

## Context

초기 GPU 배치에서는 `Wetness`, `Heat`, `Burn`, `Mud` 네 State를 고정 채널로 가정했다. 하지만 State 종류를 C++ 코드에 고정하면 새로운 State를 추가할 때마다 자료형, Solver와 GPU 배치를 함께 수정해야 한다. 또한 State 이름은 `.SRProfile`의 `states` key 및 Transition의 source/target 문자열로 이미 표현된다.

State 종류의 정의와 각 소재의 반응 파라미터를 분리하고, Solver가 특정 State 이름에 의존하지 않는 구조가 필요하다.

## Decision

- State 종류는 C++ `enum` 또는 별도 `SurfaceStateSchema` 파일에 고정하지 않는다.
- 로드된 모든 `.SRProfile`의 `states` key를 수집해 `SurfaceStateRegistry`를 구성한다.
- 각 `.SRProfile`은 State 종류 목록이 아니라, 해당 Profile이 지원하는 State별 반응 파라미터와 Transition을 정의한다.
- 파일과 진단 메시지에는 사람이 읽을 수 있는 문자열을 사용하고, 런타임과 GPU에서는 Registry가 부여한 `StateId` 또는 `ChannelIndex`를 사용한다.
- Solver는 `Wetness`, `Heat` 등의 이름을 하드코딩하지 않고 등록된 채널을 순회한다.
- State 이름 및 Transition의 source/target에는 동일한 정규화 규칙을 적용한다.

| 입력 | 정규화 규칙 | 예시 |
|---|---|---|
| State 이름 | 앞뒤 whitespace 제거 후 lowercase 변환 | `" Wetness "` → `wetness` |
| Transition source/target | State 이름과 같은 규칙 적용 | `"HEAT"` → `heat` |
| 나머지 문자 | 변경하거나 치환하지 않음 | `surface_heat`, `surface-heat`, `surface heat`는 서로 다른 이름 |

Registry는 Profile 로드 집합에 대해 재현 가능한 ID를 부여한다. canonical State 이름을 bytewise 오름차순으로 정렬한 뒤 0부터 `StateId`를 배정한다. Profile에 정의되지 않은 State parameter slot은 `optional`의 empty 값으로 남겨 해당 Profile이 그 State를 지원하지 않음을 구분한다.

## Alternatives Considered

### 1. C++ enum으로 State 종류 고정

컴파일 타임 타입 안정성이 있고 작은 고정 집합에서는 단순하다. 반면 State 추가 때마다 엔진 코드를 변경해야 하므로 데이터 주도 Profile 구조와 맞지 않는다.

### 2. 별도 `SurfaceStateSchema` 파일 사용

모든 State의 메타데이터를 한 곳에서 관리하기 쉽다. 그러나 현재 결정에서는 Profile의 State key가 실제 사용 집합을 제공하므로 추가 스키마 파일은 중복 정의가 된다. 필요한 메타데이터가 생기면 별도 결정으로 재검토한다.

### 3. Profile 로드 시 State key를 수집해 Registry 구성 — 선택

Profile 데이터에 등장하는 State를 자동으로 등록하고 런타임에서는 정수 ID로 효율적으로 접근할 수 있다. 대신 이름 정규화, ID 배정의 재현성, Profile 간 State 집합 병합 규칙을 명시하고 검증해야 한다.

## Consequences

- 새로운 State는 Profile 데이터로 추가할 수 있고 Solver의 State별 분기 없이 처리한다.
- Profile에서 발견된 문자열 이름과 런타임 채널 ID 사이의 매핑을 관리해야 한다.
- State ID는 Profile load order와 무관한 canonical 이름 정렬 순서로 결정된다. Profile 집합 자체가 달라지면 ID 집합과 index가 달라질 수 있으므로 Registry를 참조하는 runtime data는 같은 Registry와 함께 사용해야 한다.
- State 배열의 크기와 GPU 레이아웃은 Registry의 State 수와 연결된다. 고정 네 채널을 전제로 한 기존 GPU 배치 결정은 더 이상 유효하지 않으며, 실제 GPU 표현은 별도 구현 설계에서 정한다.
- 정규화 결과 이름 충돌, 알 수 없는 Transition State, Profile 간 State 결합 규칙을 로딩 검증에서 다뤄야 한다.

## Related

- [[0004-Asset-Profile-Mapping]]
- [[0005-Per-Texel-GPU-Data-Layout]]
- [[../03_Architecture/0003_Assets-and-Profiles|에셋과 프로필]]
- [[../05_Development/Notes/0003_Surface-State-GPU-Resource|Surface State GPU Resource]]
