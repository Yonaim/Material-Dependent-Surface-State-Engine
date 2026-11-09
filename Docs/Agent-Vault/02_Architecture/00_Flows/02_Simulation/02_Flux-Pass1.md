# Solver Pass 1 — Raw Flux / Outgoing Scale

입력은 Current State·Profile·Geometry·Transfer Weight, 출력은 RawOutgoing·OutgoingFluxScale 및 선택적 방향별 Flux cache다.

**연결 문서**
- [[02_Architecture/00_Flows/02_Simulation/03_Gather-Pass2|Pass 2]]
- [[02_Architecture/02_Source/01_Shaders/00_Simulation/01_Solver-Passes|Simulation Shader]]

## 실행

`TSurfaceStateSolver::RecordSteps`가 `SurfaceSolverPass1.comp`를 기록한다. Pass 1은 Decay를 고려한 source 보유량을 기준으로 방향별 Raw Flux와 전체 Outgoing을 구하고, 가용량을 초과하는 유출을 제한할 scale을 기록한다.

## 의존성

- **읽기:** Current State A/B, Profile parameter, neighbor geometry, directional weights, sparse metadata(활성화 시).
- **쓰기:** RawOutgoing, OutgoingFluxScale, 선택적 cache/scratch.
- **후속:** Compute Write → Read barrier 이후 Pass 2가 source의 scale/Flux를 읽는다. Cache OFF일 때 Pass 2는 필요한 방향 Flux를 다시 계산할 수 있다.

수식은 [[02_Architecture/01_Contracts/04_Solver|Solver 계약]], 정확한 descriptor/packing은 [[02_Architecture/01_Contracts/05_GPU-Layout|GPU Layout]]을 기준으로 한다.

## C++ barrier/dispatch 연결

`TSurfaceStateSolver::RecordSteps`는 Pass1 variant를 선택하고 full 또는 `Item.HeightBuffer`의 solver indirect offset으로 dispatch한다. 종료 후 `OutgoingFluxScale`, `RawOutgoing`에 compute write → compute read buffer barrier를 둔다. shader에서 총 raw outgoing이 계산한 available amount보다 클 때 scaling해 Pass2가 읽도록 한다. Raw flux cache를 켰을 때에만 방향별 flux cache가 채워진다.
