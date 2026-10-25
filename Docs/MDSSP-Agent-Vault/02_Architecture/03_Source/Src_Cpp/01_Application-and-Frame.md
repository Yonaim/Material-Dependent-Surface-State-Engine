# Application and Frame

`main.cpp`가 명령행 옵션을 파싱해 `TApplication`을 만들고 실행한다. `TApplication`은 설정·창·Vulkan·Scene·Surface State·Renderer·Debug UI 수명을 잇는다. 프레임 내부 시뮬레이션 준비와 step 기록은 `TFrameCoordinator`로 위임한다.

| 파일 | 코드에서 찾을 것 | 역할과 연결 |
|---|---|---|
| [main.cpp](../../../../../Source/main.cpp) | `main` | `ParseApplicationLaunchOptions` 후 `TApplication::Run`을 호출하는 프로세스 진입점 |
| [Application.h](../../../../../Source/Application/Application.h) | `TApplication` 선언 | 엔진 주요 객체 소유, 앱 lifecycle·main loop API |
| [Application.cpp](../../../../../Source/Application/Application.cpp) | 생성·초기화·`Run`·정리 경로 | 설정→window/Vulkan→asset/scene→surface/render/UI 초기화와 종료 순서를 연결 |
| [BenchmarkOptions.h](../../../../../Source/Application/BenchmarkOptions.h) | `TBenchmarkOptions`, `TApplicationLaunchOptions` | CLI에서 정해지는 benchmark·Scene·옵션 데이터 |
| [BenchmarkOptions.cpp](../../../../../Source/Application/BenchmarkOptions.cpp) | `ParseApplicationLaunchOptions` | `argv`에서 실행 옵션을 검증·구성해 `main`에 반환 |
| [EngineConfig.h](../../../../../Source/Application/EngineConfig.h) | 설정 구조와 read/write API | 사용자 설정값과 설정 파일 경로의 선언 |
| [EngineConfig.cpp](../../../../../Source/Application/EngineConfig.cpp) | 설정 읽기·파싱·기본값 처리 | 파일에서 runtime 설정을 읽고 잘못된 값에 기본 정책 적용 |
| [PerformanceSettings.h](../../../../../Source/Application/PerformanceSettings.h) | performance setting 구조·serialization | 성능 비교용 실행 옵션을 파일로 저장·복원 |
| [FrameCoordinator.h](../../../../../Source/Application/FrameCoordinator.h) | `TFrameCoordinator` API | frame begin, simulation 준비, command 기록, clock/state restart 경계 |
| [FrameCoordinator.cpp](../../../../../Source/Application/FrameCoordinator.cpp) | `BeginFrame`, `PrepareSteps`, `RecordSimulation`, `RestartState` | UI·pause·reset 상태를 반영해 누적 시간을 Solver step으로 바꾸고 surface simulation 기록 |
| [Window.h](../../../../../Source/Application/Window.h) | `TWindow` | GLFW 창 상태·framebuffer 크기·대기 API |
| [Window.cpp](../../../../../Source/Application/Window.cpp) | GLFW init/terminate, callbacks | 창 lifecycle과 크기 변경을 관리하고 resize를 renderer에 전달할 기반 제공 |
| [SceneFileDialog.h](../../../../../Source/Application/SceneFileDialog.h) | `TSceneFileDialog` | Scene 선택 창의 외부 API |
| [SceneFileDialog.cpp](../../../../../Source/Application/SceneFileDialog.cpp) | 파일 선택·확장자 처리 | UI에서 선택한 Scene 경로를 앱에 돌려줌 |

수정할 때 확인할 흐름은 frame clock이 suspend/pause/reset을 반영하는 지점, Scene 재시작 시 State와 renderer resource가 함께 갱신되는 지점이다. [프레임 순서](../../01_Flows/00_Index.md)와 [검증 문서](../../../04_Experiments/01_Validation-and-Tests.md)를 함께 본다.

## `MainLoop`에서 한 프레임 추적하기

[`TApplication::MainLoop`](../../../../../Source/Application/Application.cpp)는 window event를 poll하고 `TDebugUI::BeginFrame`을 호출한다. `TInputSystem::PollDebugContact`가 반환한 contact와 Scene animation event를 `TSurfaceStateSystem::SubmitContact`로 넘긴다. 그 다음 `TRenderer::RenderFrame`에서 simulation과 rendering command가 기록된다. benchmark mode에서는 고정 simulation step을 전달하고 CPU frame duration을 모은다. UI 설정·파일 대화상자가 오래 걸린 프레임은 `ConsumeFrameTimeResetRequest`로 simulation clock을 중단하고 GPU 완료 뒤 시간 기준을 다시 잡는다.

[`TFrameCoordinator::BeginFrame`](../../../../../Source/Application/FrameCoordinator.cpp)은 `PrepareScratchStorage` → reset 요청 처리 → clock 누적·pending 제한 순서다. reset이면 `TSurfaceStateSystem::ResetState`와 `ResetClock`을 호출한다. `PrepareSteps`는 auto substepping에서 `GetMaximumStableDeltaTime`을 사용하고 `TSimulationClock::Consume`이 반환한 step 목록을 보관한다. `RecordSimulation`은 목록을 순회하며 마지막 step에만 `bPrepareRenderHeight=true`를 넘긴다. 끝나면 `StepSerial`을 step 수만큼 증가시킨다.

| 증상·변경 | 추적할 지점 |
|---|---|
| pause 중 한 번만 실행 | `ConsumeSolverStepRequest` → `PrepareSteps` → `TSimulationClock::Consume`의 paused 분기 |
| 프레임 지연 뒤 simulation 시간 | `MainLoop`의 suspend flag → `BeginFrame`의 `LimitPendingSeconds`·`DroppedSeconds` |
| reset 후 첫 계산 | `BeginFrame`의 `ResetState`, `ResetClock` → 다음 프레임의 input/geometry 재구축 |
| 여러 substep의 측정 | `RecordSimulation`의 query offset과 `RendererCommands.cpp`의 query 영역 배치 |

`Window.cpp`는 GLFW와 framebuffer 크기를 관리하지만 Solver 시간은 `MainLoop`에서 측정한다. 설정 파일과 CLI는 초기 옵션의 출처이며 실제 실행값은 UI와 Scene 설정 적용 뒤의 값을 확인해야 한다.

## C++ 실제 호출과 시간 경계 — 확인됨

1. [TApplication::MainLoop](../../../../../Source/Application/Application.cpp)에서 event/UI/input을 처리하고 `TRenderer::RenderFrame`을 호출한다. 프레임마다 고정 개수의 Solver step을 수행한다고 가정하면 안 된다.
2. [BeginFrame](../../../../../Source/Application/FrameCoordinator.cpp)은 `PrepareScratchStorage` → UI reset 소비 → 필요 시 `ResetState`·`ResetClock` → `Clock.Accumulate`·`LimitPendingSeconds` 순서다. clock suspend 때는 `PrepareTransferWeightCachesForSettingChange`를 호출한다.
3. 같은 파일의 `PrepareSteps`는 `GetMaximumStableDeltaTime`을 참고해 `Clock.Consume(...)`를 호출하며, `RecordSimulation`은 반환된 step 수만큼 `TSurfaceStateSystem::RecordStep`을 반복한다. 마지막 반복에만 `bPrepareRenderHeight = true`를 넘긴다.
4. [TSimulationClock](../../../../../Source/SurfaceState/Solver/SimulationClock.h)의 상수는 `FixedSimulationStepSeconds = 1/60`, `MaxSimulationStepsPerFrame = 8`, `MaxRealtimePendingTicks = 4`. auto substepping은 fixed tick을 여러 작은 step으로 나눌 수 있다. UI pause에서 단일 step 요청은 pending time 소비와 구분된다.
5. Scene reload/reset 시 `RestartState`, `ResetClock`, 렌더링 자원 교체를 구분해 읽는다. `BeginFrame`의 리셋은 해당 프레임의 `Steps`를 비운다.

**대조 결과:** `TSimulationClock::Accumulate/Consume`는 클래스 내부 인라인 정의이므로 `.cpp`의 qualified symbol로 검색되지 않는다. 함수가 없다는 뜻은 아니다.
