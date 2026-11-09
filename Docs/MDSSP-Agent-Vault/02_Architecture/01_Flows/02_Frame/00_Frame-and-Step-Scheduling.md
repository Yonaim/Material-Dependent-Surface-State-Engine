# Frame / Step Scheduling

App 프레임과 Simulation Step은 다르다. Clock, pause·reset, 누락된 시간, fixed/variable step, auto substep, 마지막 step의 렌더 높이 준비까지의 제어 경로를 다룬다.

**연결 문서**
- [[02_Architecture/01_Flows/02_Frame/01_Contact-to-InputDelta|Contact]]
- [[02_Architecture/01_Flows/03_Simulation/01_PreStep-and-Sparse-Scheduling|Pre-Step]]
- [[02_Architecture/03_Source/Src_Cpp/01_Application-and-Frame|Frame C++]]

## MainLoop에서 Step 목록까지

[`TApplication::MainLoop`](../../../../../Source/Application/Application.cpp)는 UI 프레임과 raycast 입력을 처리해 Contact를 `SubmitContact`에 전달한 뒤 `TRenderer::RenderFrame`을 호출한다. [`TFrameCoordinator::BeginFrame`](../../../../../Source/Application/FrameCoordinator.cpp)은 scratch 형식과 reset을 처리하고, pause·time scale·프레임 시간 중단 여부에 따라 시간을 누적한다. `LimitPendingSeconds`는 프레임 작업 예산을 넘는 시간을 버린다. `PrepareSteps`는 auto substepping 때 `GetMaximumStableDeltaTime`을 물어보고 [`TSimulationClock::Consume`](../../../../../Source/SurfaceState/Solver/SimulationClock.h)에 fixed/variable step, pause, 단일 step 요청을 전달한다. 기본 fixed tick은 `1/60 s`, 프레임당 최대 step 수는 `MaxSimulationStepsPerFrame`이다.

| 상태 | 기록되는 Solver step |
|---|---|
| pause·단일 step 없음 | 없음 |
| pause·단일 step 요청 | 하나 |
| reset | State와 clock을 재설정하고 해당 프레임 step 목록을 비움 |
| 여러 substep | `RecordSimulation`이 순서대로 기록하며 마지막 step이 렌더 높이 준비를 요청 |

## 실제 clock 상수와 누락 가능성

[SimulationClock.h](../../../../../Source/SurfaceState/Solver/SimulationClock.h)에서 `FixedSimulationStepSeconds = 1/60`, `MaxSimulationStepsPerFrame = 8`, realtime pending tick 기본 budget = 4. UI time scale이 클 경우 `BeginFrame`은 `ceil(timeScale)`도 고려하여 pending budget을 정하고, 초과 시간은 `DroppedSeconds`로 기록한다. fixed+auto mode에서는 하나의 fixed tick이 더 작은 transport-safe substep으로 쪼개질 수 있으며, 최대 8 step까지만 소비한다. `RecordSimulation`은 기록한 step 수를 `StepSerial`에 누적한다.
