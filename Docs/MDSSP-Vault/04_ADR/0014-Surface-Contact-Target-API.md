# ADR 0014 — Surface Contact Target API

- 상태: **Accepted**
- 날짜: 2026-09-26
- 관련: [[../03_Architecture/0008_Surface-Input|Surface Contact Input]], [[../02_Planning/02_Weekly-Details/Week-04/0006_Branch-Surface-Input-Integration|Branch 6 — Surface Input Integration]]

## Context

Assets의 `SurfaceContactInput` 예시는 `targetSurface`와 접촉 정보를 한 구조체에 담는다. 게임 모듈이 이 API를 사용할 때 내부 `SurfaceInstanceID`를 직접 조회하고 매 접촉 이벤트마다 값으로 전달하게 하면, 모듈 내부 식별자를 알아야 하고 매번 객체와 Surface 관계를 찾아야 한다.

충돌 이벤트는 Collider 또는 충돌 객체를 알려주지만, 그 객체가 어떤 Surface instance를 나타내는지는 별도의 Scene 관계로 정해야 한다. Debug Raycast 역시 hit한 Scene instance에서 대상 Surface를 얻을 수 있어야 한다.

## Decision

- Scene 설정 시 Collider와 Surface instance의 관계를 한 번 등록한다.
- Branch 6 MVP에서는 Collider 하나가 Surface instance 하나를 가리키도록 한다.
- 외부 입력 API는 대상 Surface에 바인딩된 제출 함수를 제공하는 형태로 설계한다. 접촉 이벤트별 데이터에는 Surface ID를 넣지 않고, State 종류, 월드 접촉 위치와 방향, 반경, 세기, 감쇠 정도를 전달한다.
- 접촉 이벤트는 Collider-Surface 연결을 통해 대상 Surface에 제출한다. Debug Raycast도 hit 결과의 Surface를 사용해 같은 접촉 제출 경로를 호출한다.
- `SurfaceContact::SubmitContact(...)` 형태는 이 계약을 설명하기 위한 예시이며, 구체적인 C++ 공개 선언은 구현 중 코드 구조에 맞춰 정한다.

## Alternatives Considered

### 1. 매 접촉 구조체에 `SurfaceInstanceID` 포함

Assets 예시와 직접 대응하며 입력이 자체적으로 대상을 식별한다. 그러나 사용자가 모듈 내부 ID를 알아내고 매 이벤트마다 채워야 해 공개 API 사용성이 떨어진다. 선택하지 않는다.

### 2. 제출 함수의 인자로 Surface ID/핸들 전달

`SubmitContact(surfaceId, contact)` 형태로 명시적 대상이 드러난다. 내부 ID 조회 문제는 해결하지 못하며 매번 대상 값을 전달해야 한다. 공개 MVP API로 선택하지 않는다.

### 3. Scene에서 Collider-Surface 관계를 설정하고 Surface에 접촉 데이터 제출 — 선택

게임 개발자는 객체 설정 시 관계를 한 번 연결하고, 충돌 이벤트에서는 해당 Surface에 접촉 정보만 보낸다. 내부 ID를 매 이벤트마다 조회하거나 전달하지 않는다. Collider와 Surface의 관계를 Scene에 명확히 구성해야 한다.

## Consequences

- 접촉 입력의 대상 지정은 Scene의 Collider-Surface 연결로 해결한다.
- 입력 이벤트 payload는 대상 식별자와 분리되어 재사용하기 쉽다.
- 충돌 입력 경로는 Collider에서 Surface instance로 가는 관계를 해석해야 한다.
- MVP는 Collider 하나당 Surface 하나의 관계를 전제한다. 한 Collider가 여러 Surface에 대응해야 할 경우 선택 규칙을 별도로 결정한다.
- Assets API 예시의 `targetSurface`는 논리적 대상 의미를 나타내며, 외부 공개 API에서 해당 필드를 그대로 요구하지 않는다.

## Related

- [[../03_Architecture/0008_Surface-Input|Surface Contact Input]]
- [[../07_Assets/Documents/0004_Contact-Input|Assets — Contact Input API]]
- [[../02_Planning/02_Weekly-Details/Week-04/0006_Branch-Surface-Input-Integration|Branch 6 — Surface Input Integration]]
