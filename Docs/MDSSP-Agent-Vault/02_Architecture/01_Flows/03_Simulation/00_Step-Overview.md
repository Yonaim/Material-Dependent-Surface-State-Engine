# Simulation Step Overview

각 실행 step의 전체 순서만 정의한다. 수식·저장 ABI는 계약 문서에서, 셰이더 파일과 C++ 소유자는 Source에서 확인한다.

**연결 문서**
- [[02_Architecture/01_Flows/03_Simulation/01_PreStep-and-Sparse-Scheduling|Pre-Step]]
- [[02_Architecture/01_Flows/03_Simulation/02_Flux-Pass1|Pass 1]]
- [[02_Architecture/01_Flows/03_Simulation/03_Gather-Pass2|Pass 2]]
- [[02_Architecture/01_Flows/03_Simulation/04_Accumulation-and-Geometry-Feedback|Geometry]]

## 한 Solver Step과 Geometry Feedback


```mermaid
flowchart LR
  Contact[Debug / Demo contact] --> Delta[InputDelta]
  Current[Current State] --> Pass1[Pass 1: Decay + RawFlux + alpha]
  Geometry[Geometry + Profile + neighbors] --> Pass1
  Pass1 --> Barrier[Compute barrier]
  Barrier --> Pass2[Pass 2: incoming gather + Next]
  Current --> Pass2
  Delta --> Pass2
  Pass2 --> Height[Accumulation height]
  Height --> Dynamic[Dynamic position/normal/concavity/weights]
  Dynamic --> NextStep[Next Solver step]
  Pass2 --> Render[Debug + demo Lit]
  Height --> Render
```

`InputDelta`는 첫 Pass 2에서 한 번 소비한다. Pass 1은 가용량 이상을 보내지 않도록 outgoing을 제한하고 Pass 2가 초과 Capacity까지 포함한 Next State를 저장한다. State-driven geometry가 ON이면 파생 형상은 **다음** step의 수송·Decay에 들어간다. OFF이면 정적 형상과 CPU 가중치를 쓴다. Raw Flux cache와 sparse 실행은 결과식이 아닌 실행 경로의 선택이다. [[02_Architecture/02_Contracts/03_Solver|Solver]]


## 실제 명령 순서

```mermaid
flowchart TD
  Current["Current State A/B"] --> Pre{"Current geometry 무효?"}
  Pre -->|예| Build["Current 높이 → geometry → weight 전체 재구축"]
  Build --> Reset["sparse metadata reset·입력 group 병합"]
  Pre -->|아니오| Reset
  Reset --> Pass1["Pass 1: decay·raw flux·outgoing scale"]
  Pass1 --> Barrier["compute write → read barrier"]
  Barrier --> Pass2["Pass 2: incoming gather·InputDelta → Next State"]
  Pass2 --> Height["Next State → Accumulation Height"]
  Height --> Dynamic{"state driven geometry?"}
  Dynamic -->|예| Geo["Next geometry → dynamic weights"]
  Dynamic -->|아니오| Swap["높이만 준비한 뒤 A/B 전환"]
  Geo --> Swap
  Swap --> Consumer["다음 substep 또는 renderer"]
```

[`TSurfaceStateSystem::RecordStep`](../../../../../Source/SurfaceState/Solver/SurfaceStepRecording.cpp)는 active instance마다 현재 A/B, texel/channel 수, 모델 행렬, gravity, 입력 활성화 및 Solver flags를 `TSurfaceSolverInstanceStep`에 담는다. Transform·설정 변경으로 transfer weight cache가 무효화되면 CPU에서 다시 계산해 업로드한다. [`TSurfaceStateSolver::RecordSteps`](../../../../../Source/SurfaceState/Solver/SurfaceStateSolver.cpp)는 instance들을 단계별로 묶어 dispatch하며, 출력 버퍼가 다음 shader·indirect command에서 읽히기 전에 barrier를 기록한다.

## Pass별 입출력과 조건

| 단계 | shader와 주요 읽기 → 쓰기 | 조건 |
|---|---|---|
| 선행 재구축 | `SurfaceAccumulationHeight` → `SurfaceDynamicGeometryUpdate` → `SurfaceDynamicWeightsUpdate`: Current State → 현재 geometry·weight | reset, profile/transform 변경 등으로 현재 geometry가 무효화되었을 때 full dispatch |
| schedule reset | `SurfaceSparseScheduleReset`: persistent activity·CPU 입력 group → 현재/다음 schedule | sparse solver·active mask·geometry scheduling 사용 시 |
| Pass 1 | `SurfaceSolverPass1`: Current State·Profile·Geometry·Weight → RawOutgoing·OutgoingFluxScale·선택적 flux cache | source의 decay와 전송 가능량 계산 |
| Pass 2 | `SurfaceSolverPass2`: Current State·이웃 출력·InputDelta → Next State·다음 activity | target에서 incoming을 gather하고 입력을 한 번 반영 |
| 후행 파생값 | `SurfaceAccumulationHeight`, 필요한 경우 두 Dynamic shader: Next State → 높이·다음 geometry·weight | 다음 step 및 renderer가 새 결과를 읽도록 준비 |

## Step 종료, 다음 소비자

`RecordSteps` 이후 `TSurfaceGPUResourceManager::AdvanceCurrentState`가 A/B 방향을 뒤집고 입력 활성화 플래그를 정리한다. geometry OFF에서는 Solver용 동적 geometry 갱신을 생략하지만 마지막 substep에서 표시용 높이를 만들 수 있다. sparse dispatch는 metadata의 indirect command를 소비하고, full 경로는 texel 수에 따른 workgroup을 dispatch한다. 둘의 수식 의미와 buffer ABI는 [[02_Architecture/02_Contracts/03_Solver|Solver]]와 [[02_Architecture/02_Contracts/04_GPU-Layout|GPU Layout]]이 기준이다.


## 변경 시 함께 추적할 위치

| 변경 | 생산자 | 소비자 |
|---|---|---|
| 시간·substep | `FrameCoordinator.cpp`, `SimulationClock.h` | `SurfaceStepRecording.cpp` |
| Contact 범위·적용 | `SurfaceContactApplication.cpp` | `SurfaceSolverPass2.comp`, sparse 입력 group |
| Flux·Decay | `SurfaceSolverPass1.comp`, `SurfaceSolverPass2.comp` | scratch layout, `SurfaceStateSolver.cpp` barrier |
| State 기반 형상 | `SurfaceAccumulationHeight.comp`, 두 Dynamic shader | 다음 step과 렌더 overlay |
