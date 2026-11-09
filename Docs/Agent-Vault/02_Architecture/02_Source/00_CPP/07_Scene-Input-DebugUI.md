# Scene, Input and Debug UI

이 페이지는 simulation core 주변의 런타임 Scene 모델, 외부 Contact 생성, 설정·관찰 UI와 로그를 찾는다. Scene object→surface runtime 요청은 `TSceneLoader`/`TSurfaceDataManager`, user Contact→`TSurfaceStateSystem::SubmitContact`는 InputSystem 경로를 따른다.

## Scene과 camera

| 파일 | 주요 코드 | 역할·연결 |
|---|---|---|
| [Transform.h](../../../../../Source/Scene/Transform.h) | `TTransform` | local position·rotation·scale와 model transform 자료 |
| [Camera.h](../../../../../Source/Scene/Camera.h) | `TCamera` setter/getter | position·target·rotation·projection 설정 API |
| [Camera.cpp](../../../../../Source/Scene/Camera.cpp) | view/projection 계산 | camera 파라미터를 renderer가 쓰는 view/projection matrix로 변환 |
| [StaticMeshInstance.h](../../../../../Source/Scene/StaticMeshInstance.h) | `TStaticMeshInstance` | asset handle, transform, surface runtime handle 등 scene object data |
| [StaticMeshInstance.cpp](../../../../../Source/Scene/StaticMeshInstance.cpp) | transform·surface handle 변경 | mesh instance의 상태 갱신과 Runtime Surface Data 연결 |
| [Scene.h](../../../../../Source/Scene/Scene.h) | `TScene`, initial Contact/state, animation API | Scene이 소유하는 mesh instance·camera·초기화 조건 |
| [Scene.cpp](../../../../../Source/Scene/Scene.cpp) | instance lookup, initial state capture/restore | scene-level object query, restart 시 원래 transform/state 복구 |
| [SceneLoader.h](../../../../../Source/Scene/SceneLoader.h) | `TSceneLoader` | scene asset 파일 load/save API |
| [SceneLoader.cpp](../../../../../Source/Scene/SceneLoader.cpp) | JSON field parser·validation·save | asset path를 로드하고 instance/profile/camera/initial-contact 정보를 Scene으로 조립 |
| [DemoAnimation.h](../../../../../Source/Scene/DemoAnimation.h) | animation property/key/track/clip/event 타입 | 파일 기반 demo animation 입력 자료 |
| [DemoAnimation.cpp](../../../../../Source/Scene/DemoAnimation.cpp) | `LoadDemoAnimation`, `ApplyDemoAnimation` | 시간에 맞춰 transform·state cue·Contact event를 Scene에 적용 |

## Input과 raycast

| 파일 | 주요 코드 | 역할·데이터 흐름 |
|---|---|---|
| [InputSystem.h](../../../../../Source/InputSystem/InputSystem.h) | `TInputSystem` | keyboard/mouse input에서 camera edit·Contact event 생성 |
| [InputSystem.cpp](../../../../../Source/InputSystem/InputSystem.cpp) | update/poll·debug inject 처리 | 현재 Scene과 UI 조작 상태를 보고 simulation contact 요청을 보냄 |
| [Raycaster.h](../../../../../Source/InputSystem/Raycaster.h) | `TSurfaceRayHit`, `TRaycaster` | scene mesh raycast 결과와 선택 surface/triangle 데이터 |
| [Raycaster.cpp](../../../../../Source/InputSystem/Raycaster.cpp) | front-facing triangle intersection·surface texel pick | 화면 ray에서 hit triangle과 simulation UV/texel 중심을 찾아 Contact 대상으로 반환 |

Contact 입력 수식과 적용 시점은 [[02_Architecture/01_Contracts/01_State-and-Profile#Contact Input|Contact Input]]을 따른다.

## Debug UI와 profiling

| 파일 | 주요 코드 | 역할 |
|---|---|---|
| [DebugUI.h](../../../../../Source/DebugUI/Core/DebugUI.h) | `TDebugUI` public API/state | panel 조작·setting·renderer/solver update 요청의 중심 interface |
| [DebugUI.cpp](../../../../../Source/DebugUI/Core/DebugUI.cpp) | ImGui lifecycle·frame/window orchestration | UI frame 시작·메뉴·패널 호출·상태 전달 |
| [DebugUIShared.h](../../../../../Source/DebugUI/Core/DebugUIShared.h) | shared UI enum/format/helper | panel 간 공유하는 label, style, display helper |
| [DebugUIEditor.cpp](../../../../../Source/DebugUI/Editor/DebugUIEditor.cpp) | Scene/object editor widget | Scene·transform·camera 편집 UI와 변경 통지 |
| [DebugUISimulation.cpp](../../../../../Source/DebugUI/Panels/DebugUISimulation.cpp) | clock·step·solver·Contact controls | simulation 실행 상태, test input, State/profile 제어 |
| [DebugUIContactProfile.cpp](../../../../../Source/DebugUI/Panels/DebugUIContactProfile.cpp) | Contact/Profile widgets | Contact 형상과 현재 Scene Profile override를 조작 |
| [DebugUIRender.cpp](../../../../../Source/DebugUI/Panels/DebugUIRender.cpp) | render/debug mode controls | renderer 옵션, surface/overlay 표시, smoothing·culling 토글 전달 |
| [DebugUIViewport.cpp](../../../../../Source/DebugUI/Panels/DebugUIViewport.cpp) | viewport image·interaction | render image 표시, mouse position/pick 처리 연결 |
| [DebugUILog.cpp](../../../../../Source/DebugUI/Panels/DebugUILog.cpp) | log entry view/filter/clear | `TLogger` entry를 화면에서 탐색 |
| [DebugUIProfiling.cpp](../../../../../Source/DebugUI/Profiling/DebugUIProfiling.cpp) | profiling panel coordination | renderer timing 및 memory stats 화면을 구성 |
| [DebugUIPerformanceTab.cpp](../../../../../Source/DebugUI/Profiling/DebugUIPerformanceTab.cpp) | benchmark/settings display | 계측값과 실행 조건을 읽고 performance UI에 표시 |

## Logger

| 파일 | 주요 코드 | 역할 |
|---|---|---|
| [Logger.h](../../../../../Source/Logger/Logger.h) | `TLogLevel`, `TLogEntry`, `TLogger` | 공통 log level·entry 형식과 static Write/read/clear API |
| [Logger.cpp](../../../../../Source/Logger/Logger.cpp) | thread-safe storage·terminal/file output | module별 log를 저장하고 console/UI 소비 경로에 제공 |

UI는 실험 조건을 바꿀 수 있으므로 실행값을 기록할 때 실제 toggle을 함께 확인한다. 안정된 계산 의미는 [[02_Architecture/01_Contracts/04_Solver|Solver Contract]], 테스트 상태는 [[05_Experiments/01_Validation-and-Tests|Validation and Tests]]가 기준이다.

## Scene object에서 Contact까지

`SceneLoader.cpp`는 Scene의 mesh path·profile distribution·simulation resolution을 `TStaticMeshInstance`와 runtime surface handle에 연결한다. `TScene::CaptureInitialState`/`RestoreInitialState`는 restart에서 복구할 transform·camera를 다룬다. `InputSystem.cpp`는 viewport camera에서 ray를 만들고 `Raycaster.cpp`가 mesh triangle과 simulation UV를 찾아 contact 대상을 정한다. `TApplication::MainLoop`가 반환된 contact를 `TSurfaceStateSystem::SubmitContact`에 전달하며 실제 texel별 delta는 다음 실행 step의 `SurfaceContactApplication.cpp`에서 만들어진다.

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

- [TSceneLoader::Load](../../../../../Source/Scene/SceneLoader.cpp): Scene JSON의 resolution과 `surfaceProfileMap` 경로를 파싱해 `TSurfaceDataManager::LoadSurfaceData`에 전달한다. profile map이 없는 instance는 simulation runtime handle이 없을 수 있다.
- [Raycaster.cpp](../../../../../Source/InputSystem/Raycaster.cpp) → [TInputSystem::PollDebugContact](../../../../../Source/InputSystem/InputSystem.cpp): hit 정보를 Contact로 변환한 뒤 `TApplication::MainLoop`의 `SubmitContact` 경로에서 소비한다.
- [Simulation UI](../../../../../Source/DebugUI/Panels/DebugUISimulation.cpp)와 [Render UI](../../../../../Source/DebugUI/Panels/DebugUIRender.cpp)는 다른 제어면이다. Solver rate·sparse/geometry 옵션 변경과 표시 smoothing·coverage 옵션 변경을 구분해 기록한다.
- [TTexelInspector](../../../../../Source/SurfaceState/Debug/TexelInspector.cpp)의 GPU readback과 [Inspector shader](../../../../../Shaders/Debug/TexelInspector.comp)는 별도의 검증 통로다. Debug View가 같다고 GPU 상태와 렌더 합성 결과가 동일하다는 뜻은 아니다.
