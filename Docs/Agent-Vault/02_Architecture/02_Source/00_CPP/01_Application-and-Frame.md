# Application and Frame

`main.cpp`가 명령행 옵션을 파싱해 `FApplication`을 만들고 실행한다. `FApplication`은 설정·창·Vulkan·Scene·Surface State·Renderer·Debug UI 수명을 잇는다. 프레임 내부 시뮬레이션 준비와 step 기록은 `FFrameCoordinator`로 위임한다.

| 파일 | 코드에서 찾을 것 | 역할과 연결 |
|---|---|---|
| [main.cpp](../../../../../Source/main.cpp) | `main` | `ParseApplicationLaunchOptions` 후 `FApplication::Run`을 호출하는 프로세스 진입점 |
| [Application.h](../../../../../Source/Application/Application.h) | `FApplication` 선언 | 엔진 주요 객체 소유, 앱 lifecycle·main loop API |
| [Application.cpp](../../../../../Source/Application/Application.cpp) | 생성·초기화·`Run`·정리 경로 | 설정→window/Vulkan→asset/scene→surface/render/UI 초기화와 종료 순서를 연결 |
| [BenchmarkOptions.h](../../../../../Source/Application/BenchmarkOptions.h) | `FBenchmarkOptions`, `FApplicationLaunchOptions` | CLI에서 정해지는 benchmark·Scene·옵션 데이터 |
| [BenchmarkOptions.cpp](../../../../../Source/Application/BenchmarkOptions.cpp) | `ParseApplicationLaunchOptions` | `argv`에서 실행 옵션을 검증·구성해 `main`에 반환 |
| [EngineConfig.h](../../../../../Source/Application/EngineConfig.h) | 설정 구조와 read/write API | 사용자 설정값과 설정 파일 경로의 선언 |
| [EngineConfig.cpp](../../../../../Source/Application/EngineConfig.cpp) | 설정 읽기·파싱·기본값 처리 | 파일에서 runtime 설정을 읽고 잘못된 값에 기본 정책 적용 |
| [PerformanceSettings.h](../../../../../Source/Application/PerformanceSettings.h) | performance setting 구조·serialization | 성능 비교용 실행 옵션을 파일로 저장·복원 |
| [FrameCoordinator.h](../../../../../Source/Application/FrameCoordinator.h) | `FFrameCoordinator` API | frame begin, simulation 준비, command 기록, clock/state restart 경계 |
| [FrameCoordinator.cpp](../../../../../Source/Application/FrameCoordinator.cpp) | `BeginFrame`, `PrepareSteps`, `RecordSimulation`, `RestartState` | UI·pause·reset 상태를 반영해 누적 시간을 Solver step으로 바꾸고 surface simulation 기록 |
| [Window.h](../../../../../Source/Application/Window.h) | `FWindow` | GLFW 창 상태·framebuffer 크기·대기 API |
| [Window.cpp](../../../../../Source/Application/Window.cpp) | GLFW init/terminate, callbacks | 창 lifecycle과 크기 변경을 관리하고 resize를 renderer에 전달할 기반 제공 |
| [SceneFileDialog.h](../../../../../Source/Application/SceneFileDialog.h) | `FSceneFileDialog` | Scene 선택 창의 외부 API |
| [SceneFileDialog.cpp](../../../../../Source/Application/SceneFileDialog.cpp) | 파일 선택·확장자 처리 | UI에서 선택한 Scene 경로를 앱에 돌려줌 |

수정할 때 확인할 흐름은 frame clock이 suspend/pause/reset을 반영하는 지점, Scene 재시작 시 State와 renderer resource가 함께 갱신되는 지점이다. [프레임 순서](../../00_Flows/00_Main-Flow.md)와 [검증 문서](../../../05_Experiments/01_Validation-and-Tests.md)를 함께 본다.

## `MainLoop`에서 한 프레임 추적하기

[`FApplication::MainLoop`](../../../../../Source/Application/Application.cpp)는 window event를 poll하고 `FDebugUI::BeginFrame`을 호출한다. `FInputSystem::PollDebugContact`가 반환한 contact와 Scene animation event를 `FSurfaceStateSystem::SubmitContact`로 넘긴다. 그 다음 `FRenderer::RenderFrame`에서 simulation과 rendering command가 기록된다. benchmark mode에서는 고정 simulation step을 전달하고 CPU frame duration을 모은다. UI 설정·파일 대화상자가 오래 걸린 프레임은 `ConsumeFrameTimeResetRequest`로 simulation clock을 중단하고 GPU 완료 뒤 시간 기준을 다시 잡는다.

[`FFrameCoordinator::BeginFrame`](../../../../../Source/Application/FrameCoordinator.cpp)은 `PrepareScratchStorage` → reset 요청 처리 → clock 누적·pending 제한 순서다. reset이면 `FSurfaceStateSystem::ResetState`와 `ResetClock`을 호출한다. `PrepareSteps`는 auto substepping에서 `GetMaximumStableDeltaTime`을 사용하고 `FSimulationClock::Consume`이 반환한 step 목록을 보관한다. `RecordSimulation`은 목록을 순회하며 마지막 step에만 `bPrepareRenderHeight=true`를 넘긴다. 끝나면 `StepSerial`을 step 수만큼 증가시킨다.

| 증상·변경 | 추적할 지점 |
|---|---|
| pause 중 한 번만 실행 | `ConsumeSolverStepRequest` → `PrepareSteps` → `FSimulationClock::Consume`의 paused 분기 |
| 프레임 지연 뒤 simulation 시간 | `MainLoop`의 suspend flag → `BeginFrame`의 `LimitPendingSeconds`·`DroppedSeconds` |
| reset 후 첫 계산 | `BeginFrame`의 `ResetState`, `ResetClock` → 다음 프레임의 input/geometry 재구축 |
| 여러 substep의 측정 | `RecordSimulation`의 query offset과 `RendererCommands.cpp`의 query 영역 배치 |

`Window.cpp`는 GLFW와 framebuffer 크기를 관리하지만 Solver 시간은 `MainLoop`에서 측정한다. 설정 파일과 CLI는 초기 옵션의 출처이며 실제 실행값은 UI와 Scene 설정 적용 뒤의 값을 확인해야 한다.

## C++ 실제 호출과 시간 경계 — 확인됨

1. [FApplication::MainLoop](../../../../../Source/Application/Application.cpp)에서 event/UI/input을 처리하고 `FRenderer::RenderFrame`을 호출한다. 프레임마다 고정 개수의 Solver step을 수행한다고 가정하면 안 된다.
2. [BeginFrame](../../../../../Source/Application/FrameCoordinator.cpp)은 `PrepareScratchStorage` → UI reset 소비 → 필요 시 `ResetState`·`ResetClock` → `Clock.Accumulate`·`LimitPendingSeconds` 순서다. clock suspend 때는 `PrepareTransferWeightCachesForSettingChange`를 호출한다.
3. 같은 파일의 `PrepareSteps`는 `GetMaximumStableDeltaTime`을 참고해 `Clock.Consume(...)`를 호출하며, `RecordSimulation`은 반환된 step 수만큼 `FSurfaceStateSystem::RecordStep`을 반복한다. 마지막 반복에만 `bPrepareRenderHeight = true`를 넘긴다.
4. [FSimulationClock](../../../../../Source/SurfaceState/Solver/SimulationClock.h)의 상수는 `FixedSimulationStepSeconds = 1/60`, `MaxSimulationStepsPerFrame = 8`, `MaxRealtimePendingTicks = 4`. auto substepping은 fixed tick을 여러 작은 step으로 나눌 수 있다. UI pause에서 단일 step 요청은 pending time 소비와 구분된다.
5. Scene reload/reset 시 `RestartState`, `ResetClock`, 렌더링 자원 교체를 구분해 읽는다. `BeginFrame`의 리셋은 해당 프레임의 `Steps`를 비운다.

**대조 결과:** `FSimulationClock::Accumulate/Consume`는 클래스 내부 인라인 정의이므로 `.cpp`의 qualified symbol로 검색되지 않는다. 함수가 없다는 뜻은 아니다.


## MainLoop 책임 분리 — 2026-11-09

`MainLoop`는 event/UI → `SubmitInteractiveContact` → `UpdateFrameTiming` → `AdvanceDemoAndSubmitContacts` → RenderFrame → 필요 시 `WaitForSettingsAndResetClock` → CPU 측정 순서를 직접 보여준다. `FinalizeRun`은 기존 device-idle 대기와 benchmark 마무리를 담당한다. `FFrameTiming`은 한 프레임의 suspend 여부와 재생 시간만 전달하는 private 값이며 자원을 소유하지 않는다.

시간 sampling 위치·고정 benchmark step·UI 대기 제외·animation contact 변환·GPU 대기 순서 및 기존 실패 정책을 유지했다. 파일 쓰기 오류 보완과 종료 대기 반환값 처리는 별도 후속 작업이다. 검증 근거는 [Validation](../../../05_Experiments/01_Validation-and-Tests.md)의 C++ 상위 흐름 정리 기록을 따른다.

## 애플리케이션 최상위 실행

`FApplication::MainLoop`는 이벤트 폴링, UI 입력 처리, Contact 제출, 프레임 시간 갱신, Scene 애니메이션 및 렌더 프레임 실행을 조율한다. 프레임 내부 GPU 명령 순서는 `FRenderer::RecordCommandBuffer`에서 읽고 [[02_Architecture/04_Code-Reading-Guide|Code Reading Guide]]에서 관련 하위 함수로 내려간다.
