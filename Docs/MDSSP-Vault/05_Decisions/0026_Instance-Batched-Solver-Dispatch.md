# Decision 0026 — 인스턴스별 Solver를 단계별로 묶어 기록

> **한 줄 요약:** 독립된 오브젝트 인스턴스의 같은 Solver 단계를 모아 dispatch하고, 단계 사이에서만 동기화한다.

- 분류: **Simulation**
- Status: **구현 반영 · 실제 GPU 병렬 실행과 성능 검증 대기**
- Date: 2026-10-05

## Context

초기 구현은 각 인스턴스마다 높이·형상·TransferWeight 갱신, Pass 1, Pass 2를 연속 기록했다. 각 Solver 호출의 시작과 끝에는 compute stage barrier가 있어 한 인스턴스의 compute 작업이 끝난 뒤 다음 인스턴스 작업이 시작하는 순서였다.

각 인스턴스는 State, 동적 형상, flux scratch를 별도로 갖는다. Solver 단계 사이에는 데이터 의존성이 있지만 같은 단계의 서로 다른 인스턴스 사이에는 State 의존성이 없다. 정적 Surface 데이터는 읽기 전용으로 공유할 수 있다.

## Decision

1. 한 simulation step에서 활성 인스턴스의 작업 정보를 먼저 모은다.
2. Accumulation Height 계산을 모든 해당 인스턴스에서 기록한 뒤, height/dirty 데이터 의존성을 동기화한다.
3. Dirty Dispatch, Dynamic Geometry Update, Dynamic Transfer Weight Update를 각각 인스턴스 전체에 기록하고 각 의존 단계 사이에서 barrier를 실행한다.
4. 모든 인스턴스의 Pass 1을 기록한 뒤 `RawOutgoing`과 `OutgoingFluxScale`을 동기화한다. 이어서 모든 인스턴스의 Pass 2를 기록한다.
5. Pass 1→Pass 2의 데이터 의존성 및 State 결과의 다음 step·render 가시성은 유지한다. 서로 다른 인스턴스의 같은 단계 dispatch 사이에는 barrier를 넣지 않는다.
6. Solver GPU timestamp는 인스턴스별 구간을 더하지 않고 각 단계의 인스턴스 묶음 전체를 측정한다. `SimulationInstances`는 활성 인스턴스 수를 계속 표시한다.

## Alternatives Considered

- **인스턴스별 전체 Solver 순차 기록:** 기존 구현이다. 데이터 의존성이 없는 인스턴스 사이에도 compute stage barrier가 들어가 같은 단계 dispatch가 겹쳐 실행될 여지를 제한한다.

## Consequences

- 독립 인스턴스의 같은 단계 dispatch 사이 barrier가 제거되어 GPU가 작업을 함께 스케줄할 수 있다. 실제 동시 실행과 성능 이득은 GPU와 workload에 따라 달라지며 timestamp 측정으로 확인한다.
- 여러 인스턴스 dispatch가 같은 단계에서 실행될 수 있으므로 timestamp는 단계 묶음의 GPU 경과 시간을 나타낸다. 기존 인스턴스별 시간 합계와 직접 비교하지 않는다.
- 인스턴스 내부의 Accumulation Height → Dirty Dispatch → Geometry → Transfer Weight → Pass 1 → Pass 2 데이터 의존성은 유지한다.

## Related

- [[04_Development/0003_Solver-Performance|Solver Performance]]
- [[03_Architecture/0007_Surface-GPU-Data-Layout|Surface GPU Data Layout]]
