# Solver

`TSimulationClock`가 frame elapsed를 누적하고 `TFrameCoordinator`가 실행할 fixed step 수를 결정한다. 각 step의 instance 입력은 `TSurfaceSolverInstanceStep`에 모여 `TSurfaceStateSolver`로 전달된다. Solver는 accumulation geometry 준비, 두 transport compute pass와 활성 규칙이 있을 때의 Reaction Pass를 command buffer에 기록한다. 계산식은 [[02_Architecture/01_Contracts/04_Solver|Solver Contract]], 순서는 [[02_Architecture/00_Flows/00_Main-Flow|Simulation Pipeline]]이 기준이다.

| 파일 | 주요 코드 | 역할·데이터 흐름 |
|---|---|---|
| [SimulationClock.h](../../../../../Source/SurfaceState/Solver/SimulationClock.h) | `TSimulationClock::Accumulate`, `LimitPendingSeconds`, `Reset` | pause·배속·backlog 제한을 반영해 simulation time 잔여량 유지 |
| [SurfaceTimestep.cpp](../../../../../Source/SurfaceState/Solver/SurfaceTimestep.cpp) | next timestep bound 계산, transfer-weight/geometry 입력 준비 | 현재 Solver 옵션·profile·surface geometry에 안전한 timestep 제한을 계산 |
| [SurfaceStateSolver.h](../../../../../Source/SurfaceState/Solver/SurfaceStateSolver.h) | solver term/debug 설정, `TSurfaceSolverInstanceStep`, `RecordStep(s)` | step 구성과 GPU command 기록 API |
| [SurfaceStateSolver.cpp](../../../../../Source/SurfaceState/Solver/SurfaceStateSolver.cpp) | variant 선택, push constants, `RecordBufferBarriers`, pass dispatch | buffer 상태 전이와 sparse/regular dispatch를 기록하고 Pass 1→Pass 2→선택적 Reaction 의존성을 동기화 |
| [SurfaceStepRecording.cpp](../../../../../Source/SurfaceState/Solver/SurfaceStepRecording.cpp) | step command orchestration helper | instance state, InputDelta, geometry update와 solver step 호출의 순서를 묶음 |

`RecordStep` 호출 위치는 [FrameCoordinator](01_Application-and-Frame.md)를 본다. Pass가 참조하는 C++ descriptors와 buffers는 [[02_Architecture/02_Source/00_CPP/01_GPU/00_Index|GPU Resources]], 진입 셰이더는 [[02_Architecture/02_Source/01_Shaders/00_Simulation/00_Index|Simulation Shaders]]를 본다. 관련 회귀 테스트는 `Tests/SimulationTransportTests.cpp`와 `Tests/SurfaceGPUResourceTests.cpp`에 있다. 실행 결과는 [[05_Experiments/01_Validation-and-Tests|Validation]]에서 확인한다.

## `RecordSteps`를 읽는 순서

[`SurfaceStepRecording.cpp`](../../../../../Source/SurfaceState/Solver/SurfaceStepRecording.cpp)는 `ApplyPendingContacts` 후 Scene instance를 순회한다. descriptor가 있는 instance만 step으로 묶고, 현재 A/B 방향·texel/channel 수·DeltaTime·model matrix·gravity·입력 활성화와 Solver flags를 채운다. Transform이나 설정으로 정적 weight가 낡았으면 `UpdateTransferWeightCache`를 먼저 실행한다. Solver 호출 후 `AdvanceCurrentState`로 방향을 바꾸므로 renderer는 방금 계산한 Next State를 current로 본다.

[`SurfaceStateSolver.cpp`](../../../../../Source/SurfaceState/Solver/SurfaceStateSolver.cpp)의 `RecordSteps`는 입력 크기와 DeltaTime을 검증하고 각 instance의 Current/Next descriptor와 workgroup 수, sparse indirect offset을 준비한다. 강제 full geometry가 필요하면 Current State의 높이·geometry·weight를 Pass 1 전에 재생성한다. 다음으로 fused sparse reset, Pass 1, compute barrier, Pass 2, Scene에 활성 규칙이 하나라도 있으면 Reaction Pass, 최종 Next State의 높이, 필요할 때 dynamic geometry와 weight를 기록한다. 같은 단계의 여러 instance를 묶어 dispatch하며 Pass 1/2의 shader variant는 raw flux cache와 weight 형식 flags에서 선택한다.

| 코드 지점 | 이유 | 다음 소비자 |
|---|---|---|
| `CurrentDescriptorSet`/`NextDescriptorSet` | ping-pong 상태의 읽기와 쓰기를 분리 | 다음 step과 renderer의 current 선택 |
| `SparseLayout` indirect offset | A/B별 solver list와 height/geometry worklist 주소 선택 | `vkCmdDispatchIndirect` |
| `bNeedsPreSolverGeometryBuild` | reset·transform/profile 변경 뒤 Current geometry를 먼저 유효하게 만듦 | Pass 1 geometry/weight read |
| Pass 1 buffer barriers | RawOutgoing·OutgoingFluxScale 쓰기를 Pass 2에 보이게 함 | Pass 2 neighbor gather |
| Pass 2 → Reaction barrier | ReactionScratch에 기록한 기본 State·유입 정보를 Reaction Pass에 보이게 함 | Reaction Pass가 있는 Scene |
| 최종 State buffer barriers | Pass 2 또는 Reaction Pass의 Next State·schedule 결과를 파생 pass에 보이게 함 | post height·다음 step |
| `bPrepareAccumulationHeight` | geometry OFF에서도 최종 render height를 준비할지 결정 | overlay preview |

`TSimulationClock`은 시간 예산만 관리하고 State 계산을 하지 않는다. `SurfaceTimestep.cpp`의 안정성 제한은 `PrepareSteps`에서 auto substepping을 선택했을 때 사용된다. 수치 계산의 식과 범위는 [[02_Architecture/01_Contracts/04_Solver|Solver 계약]], 실제 pass 입출력은 [[02_Architecture/02_Source/01_Shaders/00_Simulation/00_Index|Simulation shader 지도]]와 함께 읽는다.

## RecordSteps 실제 단계별 명령과 Barrier

[TSurfaceStateSystem::RecordStep](../../../../../Source/SurfaceState/Solver/SurfaceStepRecording.cpp) → [TSurfaceStateSolver::RecordSteps](../../../../../Source/SurfaceState/Solver/SurfaceStateSolver.cpp)의 호출을 따라가면:

1. Contact 적용 후 transform/weight 설정 변화를 감지해 CPU TransferWeight cache를 필요할 때 갱신한다. 각 instance의 CurrentAB, model matrix, gravity, flag, InputActive, 마지막 step의 height 준비 여부를 `TSurfaceSolverInstanceStep`에 담는다.
2. `RecordSteps` 시작은 이전 graphics/compute/indirect 사용에 대한 read completion Barrier를 기록한다. `ForceFullGeometry`인 state-driven geometry는 Current State를 **AccumulationHeight → DynamicGeometry → DynamicWeights** 순서로 전체 재구축한다.
3. sparse/active-mask/geometry height 조건이 있으면 `SurfaceSparseScheduleReset` compute를 한 번 기록하고 schedule metadata write/read·indirect Barrier를 둔다.
4. **Pass 1**: 선택된 `Pass1Pipelines[Variant]`, full `vkCmdDispatch` 또는 sparse `vkCmdDispatchIndirect`. 결과 `OutgoingFluxScale`, `RawOutgoing`의 compute write→read Barrier.
5. **Pass 2**: `Pass2Pipelines[Variant]`로 target gather 후 기본 State를 계산하고 InputDelta를 비운다. Scene에 활성 Reaction 규칙이 없으면 Next State와 activity mask도 여기서 기록한다.
6. Scene에 활성 Reaction 규칙이 하나라도 있으면 ReactionScratch barrier 뒤 **Reaction Pass**가 texel의 Profile별 규칙을 적용하고 최종 Next State와 activity/height schedule을 기록한다. 규칙이 0개인 Profile의 texel은 기본 State를 복사한다. 최종 출력의 compute/graphics/indirect access barrier를 둔다.
7. Next descriptor로 **후행 AccumulationHeight**를 계산한다. `bUpdateGeometry`이면 DynamicGeometry, DynamicWeights를 순서대로 실행한다. 후행 height만 필요한 경우 dynamic geometry 연산은 생략될 수 있다.
8. 호출자가 각 active scene instance에 `AdvanceCurrentState`를 적용하고 pending input activation을 내린다.

**구현상 중요한 조건:** `RawModeFromFlags`는 cache-off, FP32/FP16 및 coalesced 조합으로 5개, `WeightModeFromFlags`는 FP32/FP16 2개, 별도 runtime-flags variant 1개이므로 Pass 1/2 각각 **총 11개 pipeline**이다. 이는 성능 숫자나 정밀도가 모든 variant에서 동일하다는 주장과 다르며 실제 실행 테스트가 필요하다.

## Local Reaction 경로

`TSurfaceStateSolver`는 Scene 공유 Profile GPU table의 최대 활성 규칙 수가 0보다 크면 Reaction flag(bit 25)를 설정한다. 이 flag는 해당 texel의 Profile 규칙 수가 아닌 Scene 전체 조건이다. Pass 2가 binding 24에 기본 snapshot과 유입을 기록한 뒤 write/read barrier를 거쳐 `SurfaceReaction.comp`를 실행한다. 이 pass의 최종 Next State와 activity/Height schedule을 기존 post Geometry 경로가 소비한다. binding 23/24의 데이터 및 시간 적분 계약은 [[02_Architecture/01_Contracts/06_Reactions|Local Reactions]]를 따른다.
