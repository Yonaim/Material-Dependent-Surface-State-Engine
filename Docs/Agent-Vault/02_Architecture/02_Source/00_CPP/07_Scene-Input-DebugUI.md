# Scene, Input and Debug UI

이 페이지는 simulation core 주변의 런타임 Scene 모델, 외부 Contact 생성, 설정·관찰 UI와 로그를 찾는다. Scene object→surface runtime 요청은 `FSceneLoader`/`FSurfaceDataManager`, user Contact→`FSurfaceStateSystem::SubmitContact`는 InputSystem 경로를 따른다.

## Scene과 camera

| 파일 | 주요 코드 | 역할·연결 |
|---|---|---|
| [Transform.h](../../../../../Source/Scene/Transform.h) | `FTransform` | local position·rotation·scale와 model transform 자료 |
| [Camera.h](../../../../../Source/Scene/Camera.h) | `FCamera` setter/getter | position·target·rotation·projection 설정 API |
| [Camera.cpp](../../../../../Source/Scene/Camera.cpp) | view/projection 계산 | camera 파라미터를 renderer가 쓰는 view/projection matrix로 변환 |
| [StaticMeshInstance.h](../../../../../Source/Scene/StaticMeshInstance.h) | `FStaticMeshInstance` | asset handle, transform, surface runtime handle 등 scene object data |
| [StaticMeshInstance.cpp](../../../../../Source/Scene/StaticMeshInstance.cpp) | transform·surface handle 변경 | mesh instance의 상태 갱신과 Runtime Surface Data 연결 |
| [Scene.h](../../../../../Source/Scene/Scene.h) | `FScene`, initial Contact/state, animation API | Scene이 소유하는 mesh instance·camera·초기화 조건 |
| [Scene.cpp](../../../../../Source/Scene/Scene.cpp) | instance lookup, initial state capture/restore | scene-level object query, restart 시 원래 transform/state 복구 |
| [SceneLoader.h](../../../../../Source/Scene/SceneLoader.h) | `FSceneLoader` | scene asset 파일 load/save API |
| [SceneLoader.cpp](../../../../../Source/Scene/SceneLoader.cpp) | JSON field parser·validation·save | asset path를 로드하고 instance/profile/camera/initial-contact 정보를 Scene으로 조립 |
| [DemoAnimation.h](../../../../../Source/Scene/DemoAnimation.h) | animation property/key/track/clip/event 타입 | 파일 기반 demo animation 입력 자료 |
| [DemoAnimation.cpp](../../../../../Source/Scene/DemoAnimation.cpp) | `LoadDemoAnimation`, `ApplyDemoAnimation` | 시간에 맞춰 transform·state cue·Contact event를 Scene에 적용 |

## Input과 raycast

| 파일 | 주요 코드 | 역할·데이터 흐름 |
|---|---|---|
| [InputSystem.h](../../../../../Source/InputSystem/InputSystem.h) | `FInputSystem` | keyboard/mouse input에서 camera edit·Contact event 생성 |
| [InputSystem.cpp](../../../../../Source/InputSystem/InputSystem.cpp) | update/poll·debug inject 처리 | 현재 Scene과 UI 조작 상태를 보고 simulation contact 요청을 보냄 |
| [Raycaster.h](../../../../../Source/InputSystem/Raycaster.h) | `FSurfaceRayHit`, `FRaycaster` | scene mesh raycast 결과와 선택 surface/triangle 데이터 |
| [Raycaster.cpp](../../../../../Source/InputSystem/Raycaster.cpp) | front-facing triangle intersection·surface texel pick | 화면 ray에서 hit triangle과 simulation UV/texel 중심을 찾아 Contact 대상으로 반환 |

Contact 입력 수식과 적용 시점은 [[02_Architecture/01_Contracts/01_State-and-Profile#Contact Input|Contact Input]]을 따른다.

## Debug UI와 profiling

| 파일 | 주요 코드 | 역할 |
|---|---|---|
| [DebugUI.h](../../../../../Source/DebugUI/Core/DebugUI.h) | `FDebugUI` public API/state | panel 조작·setting·renderer/solver update 요청의 중심 interface |
| [DebugUI.cpp](../../../../../Source/DebugUI/Core/DebugUI.cpp) | ImGui lifecycle·frame/window orchestration | UI frame 시작·메뉴·패널 호출·상태 전달 |
| [DebugUIShared.h](../../../../../Source/DebugUI/Core/DebugUIShared.h) | shared UI enum/format/helper | panel 간 공유하는 label, style, display helper |
| [DebugUIEditor.cpp](../../../../../Source/DebugUI/Editor/DebugUIEditor.cpp) | Scene/object editor widget | Scene·transform·camera 편집 UI와 변경 통지 |
| [DebugUISimulation.cpp](../../../../../Source/DebugUI/Panels/DebugUISimulation.cpp) | clock·step·solver·Contact controls | simulation 실행 상태, test input, State/profile 제어 |
| [DebugUIContactProfile.cpp](../../../../../Source/DebugUI/Panels/DebugUIContactProfile.cpp) | Contact/Profile widgets | Contact 형상과 현재 Scene Profile override를 조작 |
| [DebugUIRender.cpp](../../../../../Source/DebugUI/Panels/DebugUIRender.cpp) | render/debug mode controls | renderer 옵션, surface/overlay 표시, smoothing·culling 토글 전달 |
| [DebugUIViewport.cpp](../../../../../Source/DebugUI/Panels/DebugUIViewport.cpp) | viewport image·interaction | render image 표시, mouse position/pick 처리 연결 |
| [DebugUILog.cpp](../../../../../Source/DebugUI/Panels/DebugUILog.cpp) | log entry view/filter/clear | `FLogger` entry를 화면에서 탐색 |
| [DebugUIProfiling.cpp](../../../../../Source/DebugUI/Profiling/DebugUIProfiling.cpp) | profiling panel coordination | renderer timing 및 memory stats 화면을 구성 |
| [DebugUIPerformanceTab.cpp](../../../../../Source/DebugUI/Profiling/DebugUIPerformanceTab.cpp) | benchmark/settings display | 계측값과 실행 조건을 읽고 performance UI에 표시 |

## Logger

| 파일 | 주요 코드 | 역할 |
|---|---|---|
| [Logger.h](../../../../../Source/Logger/Logger.h) | `ELogLevel`, `FLogEntry`, `FLogger` | 공통 log level·entry 형식과 static Write/read/clear API |
| [Logger.cpp](../../../../../Source/Logger/Logger.cpp) | thread-safe storage·terminal/file output | module별 log를 저장하고 console/UI 소비 경로에 제공 |

UI는 실험 조건을 바꿀 수 있으므로 실행값을 기록할 때 실제 toggle을 함께 확인한다. 안정된 계산 의미는 [[02_Architecture/01_Contracts/04_Solver|Solver Contract]], 테스트 상태는 [[05_Experiments/01_Validation-and-Tests|Validation and Tests]]가 기준이다.

## Scene object에서 Contact까지

`SceneLoader.cpp`는 Scene의 mesh path·profile distribution·simulation resolution을 `FStaticMeshInstance`와 runtime surface handle에 연결한다. `FScene::CaptureInitialState`/`RestoreInitialState`는 restart에서 복구할 transform·camera를 다룬다. `InputSystem.cpp`는 viewport camera에서 ray를 만들고 `Raycaster.cpp`가 mesh triangle과 simulation UV를 찾아 contact 대상을 정한다. `FApplication::MainLoop`가 반환된 contact를 `FSurfaceStateSystem::SubmitContact`에 전달하며 실제 texel별 delta는 다음 실행 step의 `SurfaceContactApplication.cpp`에서 만들어진다.

Scene animation event도 `MainLoop`에서 stable object ID로 instance를 찾아 월드 위치·법선을 계산한 뒤 동일한 `SubmitContact` 경로로 들어간다. 이 경로는 연구용 Solver 입력의 한 공급자다. 입력이 예상 위치와 다르면 ray hit triangle, UV와 mapping texel, 월드 transform, Contact의 평면/shape 설정을 차례로 확인한다.

## UI 설정이 바꾸는 코드 경로

| UI 작업 | 전달 지점 | 영향 |
|---|---|---|
| pause·step·reset·time scale | `DebugUI.cpp`의 consume/getter → `FrameCoordinator.cpp` | step 목록과 State/clock 초기화 |
| Contact shape·strength | `DebugUIContactProfile.cpp`/`DebugUISimulation.cpp` → `InputSystem.cpp` | Contact payload와 다음 step의 InputDelta |
| Solver term·sparse·geometry toggle | DebugUI → `SurfaceStateSystem.cpp` option setter | step flags, scratch/weight invalidation, shader variant |
| render mode·smoothing | `DebugUIRender.cpp` → `RendererSettings.cpp` | State texture·overlay geometry·fragment sampling 경로 |
| viewport·선택 texel | `DebugUIViewport.cpp` → `RendererCommands.cpp` | debug view와 `TexelInspector::Record` |

UI 표시값은 실행 조건을 확인하는 출발점이다. 성능 측정에서는 benchmark runner가 기록한 실제 설정과 자동 출력 원자료를 [[05_Experiments/02_Performance-Measurements|성능 기록]]에 남긴다. `Logger.cpp`의 메시지는 load·Contact 거절·resource 변경의 진단에 쓰되, 수치 결과의 근거는 자동 기록한 결과 파일이다.

## 현재 소스의 UI / Debug / Scene 연결 보강

- [FSceneLoader::Load](../../../../../Source/Scene/SceneLoader.cpp): Scene JSON의 resolution과 `surfaceProfileMap` 경로를 파싱해 `FSurfaceDataManager::LoadSurfaceData`에 전달한다. profile map이 없는 instance는 simulation runtime handle이 없을 수 있다.
- [Raycaster.cpp](../../../../../Source/InputSystem/Raycaster.cpp) → [FInputSystem::PollDebugContact](../../../../../Source/InputSystem/InputSystem.cpp): hit 정보를 Contact로 변환한 뒤 `FApplication::MainLoop`의 `SubmitContact` 경로에서 소비한다.
- [Simulation UI](../../../../../Source/DebugUI/Panels/DebugUISimulation.cpp)와 [Render UI](../../../../../Source/DebugUI/Panels/DebugUIRender.cpp)는 다른 제어면이다. Solver rate·sparse/geometry 옵션 변경과 표시 smoothing·coverage 옵션 변경을 구분해 기록한다.
- [FTexelInspector](../../../../../Source/SurfaceState/Debug/TexelInspector.cpp)의 GPU readback과 [Inspector shader](../../../../../Shaders/Debug/TexelInspector.comp)는 별도의 검증 통로다. Debug View가 같다고 GPU 상태와 렌더 합성 결과가 동일하다는 뜻은 아니다.


## DemoAnimation 로더의 탐색 순서 — 2026-11-09

`DemoAnimation.cpp`의 공개 `LoadDemoAnimation`에서 `ReadAnimationJson → ReadClipSettings → ReadWeatherCue → ReadTracks → ReadContactEvents` 순서를 찾는다. 그 아래 `ApplyDemoAnimation`, 파싱 단계, 저수준 값 검증/보간 구현을 배치한다. 버전·duration·옵션·weather·track/property/key 순서·contact의 검증 순서와 오류 메시지를 유지한다. 35개 정상/오류 입력의 old/new 비교는 [Validation](../../../05_Experiments/01_Validation-and-Tests.md)에 기록한다.

## Debug UI의 2단계 구조화

- `DebugUI.h`: 공개 API → 기능별 내부 메서드 → 기존 선언 순서대로 묶은 상태의 14개 섹션. 1단계에서는 선언/접근/초기화 순서와 로직을 보존했다.
- [ProfilingUIState.h](../../../../../Source/DebugUI/Profiling/ProfilingUIState.h) / [ProfilingUIState.cpp](../../../../../Source/DebugUI/Profiling/ProfilingUIState.cpp): `FProfilingUIState`가 13개 필드와 최근 sample deque를 값으로 소유하고 Reset/Accumulate/PublishWindow를 처리한다. 소유자는 `FDebugUI::Profiling`이며 추가 포인터·독립 수명은 없다.
- `DebugUIProfiling.cpp`는 renderer metric 수집 → frame 시간/표시 mode guard → `Profiling.Accumulate` → `Profiling.PublishWindow` → ImGui 출력을 조율한다. `ResetProfilingAverages`는 기존 모든 호출 지점에서 `Profiling.Reset`으로 위임한다.
- 기본 생성의 평균 0과 reset 후 평균 -1이라는 기존 sentinel 차이, 1초 누적 창·최근 100ms 만료·최댓값 갱신, 유효하지 않은 metric 무시는 유지한다. private UI 레이아웃 변경을 GPU/직렬화 ABI 변경으로 취급하지 않으며 소비자는 함께 빌드한다.
- 전체 적용에서 Presentation/Simulation/ProfileEditing/Gizmo/ViewportState/LogState도 각각 값 소유 상태로 추출했다. 바인딩 포인터와 작은 camera/dock 제어는 조율자의 직접 상태로 유지했다. 공개 getter/설정·리셋·ImGui 순서를 유지한다.

## Render toolbar와 설정의 모듈 경계

- `DebugUIRender.cpp`: toolbar 위치·뷰 선택·viewport/State 선택 및 상위 popup 호출.
- [DebugUIViewSettings.cpp](../../../../../Source/DebugUI/Panels/DebugUIViewSettings.cpp): `DrawViewSettingsPopup`이 현재 mode의 popup·legend를 담당한다. BeginPopup/EndPopup과 외부 toolbar Begin/End 호출 순서를 유지한다.
- [RenderViewDescriptions.h](../../../../../Source/DebugUI/Panels/RenderViewDescriptions.h): 순수 형상 설명 선택. 기존 문자열·mode switch를 보존하며 공유 widget/state 수명과 분리한다.

## UI의 상위 책임과 하위 구현

- `FDebugUI::DrawRenderSettingsWindow`는 화면 설정 탭을 조율한다. Visual/Display의 세부 값은 `DrawVisualRenderSettings`, `DrawDisplayRenderSettings` 및 하위 효과 설정 함수에서 다룬다.
- `FDebugUI::DrawProfileTuningTab`는 Profile 선택, State별 편집 데이터, 파라미터 UI 및 적용/복원 동작을 조율한다. `DrawProfileParameterControls`와 `DrawProfileOverrideActions`가 하위 책임을 수행한다.
- 편집 중의 Dirty Draft와 원본 `.SRProfile` Asset은 별개다. ImGui Begin/End, 편집값 반영 및 GPU 옵션 갱신의 실제 순서는 `DebugUIViewport.cpp`, `DebugUIContactProfile.cpp`와 관련 계약을 확인한다.

## Scene 입력과 UI 진입 함수 읽기

- `FSceneLoader::Load`는 Scene 문서 읽기 → 스키마 검증 → 기본 Scene 설정 → 객체/Surface 로드 → 초기 Contact → 애니메이션 순서다. JSON 필드·경로 검증은 세부 helper에서 읽는다.
- `FDebugUI` 생성자는 ImGui Context → Font → Style → GLFW/Vulkan backend를 준비한다. 실패 시 backend/context 정리 정책을 유지한다.
- `FDebugUI::BeginFrame`은 `BeginImGuiFrame` → `DrawEditorPanels` → `DrawInjectionCrosshair`로 표현한다. 임의의 패널 순서 변경은 hit-test 및 UI 입력 동작에 영향을 줄 수 있다.
- 뷰 선택 툴바는 `DrawToolbarViewSelector`에서 `ResolveViewModeName` → `DrawViewModeCombo` → `DrawViewportCollectionControls`를 읽고, 실제 mode enum·Renderer 설정은 하위 함수에서 확인한다.
