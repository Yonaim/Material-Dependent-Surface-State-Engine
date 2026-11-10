# Code Reading Guide — 위에서 아래로 소스 읽기

전체 구조를 처음 파악할 때는 **상위 함수의 호출 순서만 먼저 읽고**, 이해가 필요한 단계에서만 구현 함수를 펼친다. [[02_Architecture/03_System-Map|System Map]]을 먼저 보면 모듈 경계를 이해하기 쉽다.

## 최소 읽기 경로

```text
FApplication::MainLoop
  └─ FApplication::ExecuteFrame
       ├─ PollEvents → FDebugUI::BeginFrame → SubmitInteractiveContact
       ├─ UpdateFrameTiming → AdvanceDemoAndSubmitContacts
       └─ FRenderer::RenderFrame
            └─ FRenderer::RecordCommandBuffer
                 ├─ RecordSimulationAndInspector
                 │    └─ FFrameCoordinator::RecordSimulation
                 │         └─ FSurfaceStateSystem::RecordStep
                 │              └─ FSurfaceStateSolver::RecordSteps
                 │                   ├─ RecordPreSolverGeometryBuild
                 │                   ├─ RecordSparseScheduleBootstrap
                 │                   ├─ RecordSolverFluxPass       → SurfaceSolverPass1.comp
                 │                   ├─ RecordSolverStatePass      → SurfaceSolverPass2.comp
                 │                   └─ RecordPostSolverDerivedGeometry
                 ├─ PrepareDisplayResources
                 ├─ BeginSceneGraphicsPass
                 ├─ RecordSceneViewports
                 └─ RecordUIAndEndCommandBuffer
```

실제 프레임에는 실행 조건에 따라 건너뛰는 경로와 추가 Reaction Pass가 있다. **그 조건을 알아야 할 때만** [[02_Architecture/00_Flows/02_Simulation/00_Step-Overview|Step Overview]]와 [[02_Architecture/01_Contracts/04_Solver|Solver Contract]]를 읽는다.

## 기능별 진입점

| 알아보고 싶은 것 | 먼저 읽는 상위 함수 | 세부 구현을 찾을 때 |
|---|---|---|
| Scene·Mesh가 Surface Runtime이 되는 과정 | `FSurfaceDataManager::LoadSurfaceData` | `BuildSurfaceGeometryInputs`, `LoadOrBuildSurfaceGeometry`, `PrecomputeTransferNormals` |
| Scene 변경 후 GPU 자원 교체 | `FRenderer::ReloadSceneResources` | `BuildSceneReplacement`, `BuildSceneRenderPipelines`, `CommitSceneReplacement` |
| 현재 State를 화면에 표시 | `FRenderer::RecordCommandBuffer` | `PrepareDisplayResources`, `RecordInstanceDisplayResources`, `RecordTexelGeometryViews` |
| Render/Viewport 설정 | `FDebugUI::DrawRenderSettingsWindow` | `DrawVisualRenderSettings`, `DrawDisplayRenderSettings`, 각 효과 설정 함수 |
| Surface Profile 설정 | `FDebugUI::DrawProfileTuningTab` | `DrawProfileParameterControls`, `DrawProfileOverrideActions` |
| GPU State 갱신 | `FSurfaceStateSolver::RecordSteps` | Dispatch 단계, 관련 Compute Shader, GPU Buffer 계약 |

## 실제 파일

- [`Application.cpp`](../../../Source/Application/Application.cpp) — 입력과 프레임 진행
- [`Renderer.cpp`](../../../Source/Rendering/Core/Renderer.cpp) — RenderFrame와 Scene 리소스 수명
- [`RendererCommands.cpp`](../../../Source/Rendering/Core/RendererCommands.cpp) — GPU 명령 순서와 표시 경로
- [`SurfaceDataManager.cpp`](../../../Source/SurfaceState/Runtime/SurfaceDataManager.cpp) — Runtime Surface 준비
- [`SurfaceStateSolver.cpp`](../../../Source/SurfaceState/Solver/SurfaceStateSolver.cpp) — Simulation Pass 순서
- [`DebugUIViewport.cpp`](../../../Source/DebugUI/Panels/DebugUIViewport.cpp) — Viewport·Render 옵션
- [`DebugUIContactProfile.cpp`](../../../Source/DebugUI/Panels/DebugUIContactProfile.cpp) — Profile 편집

## 읽는 기준

1. 함수의 이름과 호출 순서만으로 의도를 파악할 수 있다면 거기서 멈춘다.
2. 세부 계산, 예외 처리, 자원 소유권, GPU Barrier가 궁금할 때만 하위 구현으로 내려간다.
3. 파일이 고수준 함수만 또는 저수준 함수만 포함해도 문제없다. Region 번호는 **탐색 순서**이지 추상화 레벨이 아니다.
4. 의미와 불변식은 Contracts, 실행 순서는 Flows, 코드 위치와 구현은 Source 문서에서 확인한다.

## 프로그램 시작 시 읽기

```text
FApplication::FApplication
  ├─ ResolveStartupScenePath
  ├─ LoadStartupScene                → FSceneLoader::Load
  ├─ InitializeSurfaceStateSystem
  ├─ InitializeFrameServices         → FFrameCoordinator / FRenderer / FDebugUI
  ├─ ConfigureBenchmarkIfEnabled
  └─ InitializeInput
```

- `FSceneLoader::Load`는 `ReadSceneDocument → ValidateSceneDocument → ApplySceneSettings → LoadSceneObjects → LoadInitialSceneContacts → LoadSceneAnimation`의 순서로 읽는다.
- `FRenderer` 생성자는 장치 설정, Gizmo, Surface 표시 리소스, 프레임 타이밍 리소스를 조립하고, 상세 Vulkan 핸들은 각 구현 함수에서 확인한다.
- `FDebugUI::BeginFrame`은 `BeginImGuiFrame → DrawEditorPanels → DrawInjectionCrosshair`를 호출한다. 실제 패널 순서는 `DrawEditorPanels`에서 확인하며 ImGui hit-test 순서를 임의로 바꾸지 않는다.
- 상위 함수에 JSON 키/descriptor/buffer index/ImGui 위치 계산 등이 등장하면 하위 책임으로 추출할 후보인지 먼저 평가한다. **Region은 탐색 수단이지 함수 추출의 대체물이 아니다.**
