# Branch 4 — Solver Debug Controls

브랜치: `feat/solver-debug-controls`  
선행 조건: `feat/solver-outgoing-flux-debug` 병합  
관련 설계: [[03_Architecture/0009_UI-Interface|UI Interface]], [[03_Architecture/0008_Surface-Input|Surface Contact Input 아키텍처]]

## 목표

Debug UI에서 Solver 실행을 멈추고 한 step씩 진행하거나 State를 초기화한다. 렌더링과 접촉 입력 UI는 사용할 수 있게 유지한다.

## 제어 동작

| Control | 규칙 |
|---|---|
| Pause | solver dispatch와 A/B swap을 멈춘다. 화면 render는 계속한다. 이미 제출된 입력은 다음 Solver update가 소비하도록 보존한다. |
| Step | pause 중 현재 frame `DeltaTime`을 사용해 solver를 한 번 기록·실행하고 A/B 역할을 한 번 교환한다. |
| Reset State | 모든 simulated instance의 State A/B와 InputDelta를 0으로 초기화하고 Current 역할을 A로 되돌린다. OutgoingFluxScale은 지원 channel에서 1, invalid/unsupported 위치에서 0으로 둔다. CPU에 대기 중인 Contact도 비운다. |

Reset buffer 변경은 GPU가 해당 버퍼를 읽거나 쓰는 중일 때 수행하지 않는다. 현재 queue/fence 수명 규칙에 맞춰 안전한 command 경로를 사용하고, 입력을 잃거나 다음 step에서 다시 나타나지 않는지 확인한다.

## 구현 대상

- `TSurfaceStateSystem`의 자동 step과 단일 step 요청 경로.
- Debug UI의 Pause, Step, Reset State controls.
- 모든 instance State resource 초기화와 CPU pending contact 목록 초기화.
- UI 그룹은 기존 `Surface Debug` 아래 `Solver` 섹션으로 둔다.

## 검증

- Pause 상태에서 여러 frame을 그려도 State와 A/B 역할이 바뀌지 않는다.
- 한 번 Step하면 정확히 한 번만 update하고, 다음 frame에는 다시 멈춰 있다.
- pause 동안 들어온 InputDelta는 유실되지 않고 다음 step에서 정확히 한 번 소비된다.
- Reset 후 State/InputDelta가 0, Current가 A, 지원 channel의 alpha가 1이다.
- 공유 Geometry를 사용하는 두 instance를 함께 reset해도 자원 간 값이 섞이지 않는다.
- Vulkan validation layer에서 buffer 동기화 오류가 없다.

## 완료 조건

Demo Scene에서 pause→step→pause와 reset을 반복해 결과를 안정적으로 재현한다.

## 제외 범위

texel 통계, GPU timing, playback/recording, UI 전반 재배치.
