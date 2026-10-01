# ADR 0042 — Scene 참조형 데모 애니메이션

> **한 줄 요약:** 각 .Scene은 선택적인 JSON 데모 애니메이션 파일을 참조하고, 공통 C++ 재생기가 오브젝트 변환과 카메라 연출을 실행한다.

- 분류: **Assets**
- Status: **Implemented**
- Date: 2026-10-01
- 관련 문서: [[0012-Scene-Profile-Distribution-Reference|ADR 0012 — Scene의 에셋 참조]], [[0043-Rotation-Invariant-Transfer-Cache|ADR 0043 — 회전과 TransferWeight 캐시]], [[../07_Testing/0004_Dev-Demo|개발 데모]]

## Context

현재 .Scene은 Mesh, SurfaceProfileMap, 초기 transform과 시뮬레이션 해상도를 저장한다. 애니메이션 파일 참조와 데모 재생기는 없다. Simulation의 Running/Paused/Step은 Solver 시간 제어이며, 애니메이션 재생은 별도의 시간과 제어를 가진다. 큐브·산·버니·종합 Scene은 같은 시간표를 C++에 하드코딩하지 않고 서로 다른 연출을 가져야 한다.

## Decision

1. .Scene에 선택적인 animation 경로를 둔다. 경로는 Scene 파일 디렉터리를 기준으로 해석하며 Assets/Animations의 버전이 있는 JSON .DemoAnim 파일을 가리킨다. 애니메이션이 없는 기존 .Scene은 계속 로드한다. Scene loader와 Save는 참조를 검증·보존한다.
2. .DemoAnim은 버전이 있는 JSON 파일이다. 대상 오브젝트의 고유 Scene ID와 transform keyframe, 선택적 카메라 keyframe을 선언한다. 회전은 quaternion keyframe과 Slerp 보간을 사용한다. 정확한 필드 이름과 보간 옵션은 구현 스키마에서 검증한다.
3. 공통 C++ 재생기는 애니메이션 시간을 독립적으로 진행한다. Simulation Running/Paused/Step은 Solver만 제어하며, 어느 한쪽의 재생 상태가 다른 쪽을 자동으로 바꾸지 않는다. 애니메이션에 접촉 입력 이벤트를 넣지 않는다.
   둘 다 실행 중이면 Solver는 각 step에서 평가된 현재 object transform을 사용한다. Simulation이 멈춘 동안 애니메이션만 진행해도 State는 바뀌지 않는다.
4. 애니메이션 재생·일시정지·처음부터 재생은 Surface State 초기화와 분리한다. Scene 교체 시 애니메이션 재생 상태는 새 Scene에 맞게 초기화한다. UI에는 애니메이션 제어와 Simulation 제어를 따로 표시한다.
5. 현재 Assets/Scenes의 Cubes.Scene은 Wetness·WaterFilm·Mud 비교, Mountain.Scene은 경사와 골짜기 흐름 및 고정된 뒤집힌 Mud 산, Bunny.Scene은 굴곡 잔류와 뒤집기, Demo.Scene은 세 형상의 비교를 목표로 한다. 각 Scene에서 회전 대상은 시작 시점과 12초 한 바퀴 회전 주기를 공유한다. 넓은 Mountain 지형은 위 고정 산과 교차하지 않도록 월드 수직축으로 돌리고, 추가 뒤집힌 산은 회전 대상에서 제외한다.

## Implementation

`.Scene`의 선택적 `animation` 경로와 object `id`를 `TSceneLoader`가 처리한다. `Source/Scene/DemoAnimation.cpp`는 버전 1 `.DemoAnim`의 대상·키·보간을 검증하고 위치·크기·카메라 선형/계단 보간 및 회전 quaternion Slerp를 평가한다. `TScene`의 시간·재생 상태는 Simulation과 분리했다. 네 기본 Scene에 각각 반복 회전을 연결했으며, 같은 형상의 오브젝트는 동일한 keyframe 시간과 회전 위상을 쓴다. Mountain Scene의 `mountain_mud_inverted`는 z=1.0에 고정한다. 자동 접촉 입력은 포함하지 않는다.

## Alternatives Considered

- **Scene별 C++ 하드코딩:** 현재 엔진 코드만으로 시작할 수 있지만 연출 시각과 경로를 바꿀 때마다 빌드해야 한다.
- **Python 또는 Lua 실행 파일:** 임의 동작을 표현할 수 있지만 현재 필요한 시간표·보간·이벤트에 비해 실행 환경과 API 노출이 늘어난다.
- **선언형 JSON 파일과 공통 C++ 재생기:** Scene별 수정과 재현이 쉬워 채택한다. 임의 로직이 필요해지면 별도 결정을 기록한다.

## Consequences

- .Scene 및 .DemoAnim 로더의 경로·버전·대상 object·track schema 검증과 Scene 저장 지원이 필요하다. 기존 Scene 파일은 animation이 없어도 유효하다.
- 재생을 반복하면 오브젝트 경로와 카메라가 같아야 한다. 애니메이션은 Simulation State를 초기화하거나 접촉 입력을 만들지 않는다.
- 버니 뒤집기 연출의 잔류 효과는 애니메이션만으로 보장되지 않는다. Macro/Meso 오목도와 방향별 Transport 보유 규칙은 ADR 0044·0045의 구현 결과를 사용한다.
- 이 ADR은 연출 파일 계약이며 물리 Solver의 중력식이나 TransferWeight 값을 변경하지 않는다.

## Related

- [[0012-Scene-Profile-Distribution-Reference|ADR 0012 — Scene 에셋 참조]]
- [[0043-Rotation-Invariant-Transfer-Cache|ADR 0043 — 회전 캐시]]
- [[0044-Macro-Meso-Concavity-Field|ADR 0044 — 통합 오목도]]
- [[0045-Directional-Cavity-Transport-Retention|ADR 0045 — 홈 이탈 억제]]
