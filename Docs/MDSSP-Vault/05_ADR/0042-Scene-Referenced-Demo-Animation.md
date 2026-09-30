# ADR 0042 — Scene 참조형 데모 애니메이션

> **한 줄 요약:** 각 .Scene은 선택적인 데모 시간표 파일을 참조하고, 공통 C++ 재생기가 접촉 입력·변환·카메라 연출을 실행한다.

- 분류: **Assets**
- Status: **Accepted — 구현 전**
- Date: 2026-10-01
- 관련 문서: [[0012-Scene-Profile-Distribution-Reference|ADR 0012 — Scene의 에셋 참조]], [[0043-Rotation-Invariant-Transfer-Cache|ADR 0043 — 회전과 TransferWeight 캐시]], [[../07_Testing/0004_Dev-Demo|개발 데모]]

## Context

현재 .Scene은 Mesh, SurfaceProfileMap, 초기 transform과 시뮬레이션 해상도를 저장한다. 애니메이션 파일 참조와 데모 재생기는 없다. Simulation의 Running/Paused/Step은 Solver 시간 제어이며, Scene별 접촉 입력이나 카메라·오브젝트 연출을 재생하지 않는다. 큐브·산·버니·종합 Scene은 같은 시간표를 C++에 하드코딩하지 않고 서로 다른 연출을 가져야 한다.

## Decision

1. .Scene에 선택적인 animation 경로를 둔다. 경로는 Scene 파일 디렉터리를 기준으로 해석하며 Assets/Animations의 버전이 있는 JSON .DemoAnim 파일을 가리킨다. 애니메이션이 없는 기존 .Scene은 계속 로드한다. Scene loader와 Save는 참조를 검증·보존한다.
2. .DemoAnim에는 시각, 대상 오브젝트, 접촉 입력 이벤트, transform 및 카메라 keyframe을 선언한다. 접촉 State는 고정 채널 번호가 아니라 로드된 .SRProfile의 states key 이름으로 기록하고 Scene Registry에서 해석한다. 대상 오브젝트는 배열 순서가 아닌 Scene 안의 고유 식별자로 찾는다. 정확한 JSON 필드 이름과 보간 함수는 구현 스키마에서 검증한다.
3. 공통 C++ 재생기는 데모 시간을 시뮬레이션 시간과 동기화한다. 접촉 이벤트는 지정 시각을 지날 때 한 번만 TSurfaceStateSystem::SubmitContact에 제출한다. 움직이는 오브젝트의 접촉 위치는 이벤트 시점의 transform을 사용해 월드 위치로 변환한다.
4. Play Demo는 초기 transform·카메라·Surface State와 이벤트 커서를 복원한 뒤 시작한다. 일시정지·재시작과 Scene 교체 시 재생 상태를 일관되게 처리한다. 기존 Simulation Running 버튼과 데모 재생 버튼의 역할을 UI에 구분해 표시한다.
5. 현재 Assets/Scenes의 Demo_Cubes.Scene은 Wetness·WaterFilm·Mud 비교, Mountain.Scene은 경사와 골짜기 흐름, Bunny.Scene은 굴곡 잔류와 뒤집기, Demo.Scene은 세 형상의 순차 소개를 목표로 한다. 실제 입력 강도·시간·카메라 경로는 각 .DemoAnim에서 조정하고 시각 검증 후 확정한다.

## Alternatives Considered

- **Scene별 C++ 하드코딩:** 현재 엔진 코드만으로 시작할 수 있지만 연출 시각과 경로를 바꿀 때마다 빌드해야 한다.
- **Python 또는 Lua 실행 파일:** 임의 동작을 표현할 수 있지만 현재 필요한 시간표·보간·이벤트에 비해 실행 환경과 API 노출이 늘어난다.
- **선언형 JSON 파일과 공통 C++ 재생기:** Scene별 수정과 재현이 쉬워 채택한다. 임의 로직이 필요해지면 별도 결정을 기록한다.

## Consequences

- .Scene 및 .DemoAnim 로더의 경로·버전·대상·State 검증과 Scene 저장 지원이 필요하다. 기존 Scene 파일은 animation이 없어도 유효하다.
- 재생을 반복해도 접촉 이벤트 수, State 초기값, 오브젝트 위치와 카메라가 같아야 한다. Renderer가 Solver를 기록하기 전에 해당 시각의 이벤트를 제출해야 한다.
- 버니 뒤집기 연출의 잔류 효과는 애니메이션만으로 보장되지 않는다. Macro/Meso 오목도와 방향별 Transport 보유 규칙은 ADR 0044·0045의 구현 결과를 사용한다.
- 이 ADR은 연출 파일 계약이며 물리 Solver의 중력식이나 TransferWeight 값을 변경하지 않는다.

## Related

- [[0012-Scene-Profile-Distribution-Reference|ADR 0012 — Scene 에셋 참조]]
- [[0014-Surface-Contact-Target-API|ADR 0014 — 접촉 입력 API]]
- [[0043-Rotation-Invariant-Transfer-Cache|ADR 0043 — 회전 캐시]]
- [[0044-Macro-Meso-Concavity-Field|ADR 0044 — 통합 오목도]]
- [[0045-Directional-Cavity-Transport-Retention|ADR 0045 — 홈 이탈 억제]]
