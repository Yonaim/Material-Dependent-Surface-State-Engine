# ADR 0013 — InputDelta Host Upload 동기화

- 상태: **Accepted**
- 날짜: 2026-09-26
- 관련: [[0011-GPU-Resource-Initialization-and-ABI|ADR 0011]], [[../02_Planning/02_Weekly-Details/Week-04/0006_Branch-Surface-Input-Integration|Branch 6 — Surface Input Integration]]

## Context

`InputDelta`는 instance별 GPU buffer다. CPU가 접촉 event를 upload하고 Solver Pass 2가 같은 buffer를 읽은 뒤 0으로 clear한다. Buffer는 host-visible/coherent이지만, coherent memory는 CPU/GPU 동시 접근을 순서화하지 않는다.

Renderer는 두 frame slot을 사용한다. 현재 frame slot fence만 기다린 채 공유 `InputDelta`를 CPU에서 덮어쓰면, 다른 frame slot에서 제출한 Solver command가 이전 buffer 사용을 마치지 않았을 수 있다. Host write와 shader read/write의 경합은 입력 누락이나 불확정 결과를 만들 수 있다.

## Decision

- Branch 6 MVP는 기존 host-visible/coherent `InputDelta` buffer에 CPU가 직접 upload한다.
- 새 접촉 입력이 실제로 있는 경우에만, `InputDelta`를 사용하는 Solver queue가 idle이 될 때까지 기다린 뒤 CPU upload를 한다. 현재 Solver dispatch는 graphics queue에 제출되므로 해당 graphics queue를 기다린다.
- CPU는 대상 instance별 dense `InputDelta` 배열에 같은 frame의 event를 합산하고, 해당 배열을 upload한다. 입력이 없는 frame에는 queue idle 대기와 upload를 생략한다.
- 다음 frame의 Solver command는 upload 이후 queue에 제출한다. Pass 2는 입력을 한 번 State에 더하고 `InputDelta`를 0으로 clear한다.
- queue idle로 대기하는 것은 새 입력 upload 전의 이전 GPU 사용 완료를 보장하기 위한 MVP 동기화다. 전체 queue를 매 frame 기다리지는 않는다.
- Frame-scoped staging buffer와 GPU copy/barrier 방식은 후속 최적화로 미룬다. MVP 측정에서 click 입력 대기의 체감 비용이 확인되면 별도 작업으로 도입한다.

## Alternatives Considered

### 1. 입력 event마다 `InputDelta`를 직접 덮어쓰기

CPU에서 직접 쓰는 경로는 간단하지만 이전 frame의 GPU 접근 완료를 기다리지 않으면 CPU write와 shader read/write가 겹칠 수 있어 안전하지 않다. 동기화 없이 사용하는 방식은 선택하지 않는다.

### 2. 새 입력이 있을 때 Solver queue idle 후 직접 upload — 선택

기존 buffer와 `TGPUBuffer::Upload` 경로를 재사용하고 추가 staging 자원이나 copy 명령이 필요 없다. 입력이 있는 frame에는 해당 queue 작업이 끝날 때까지 기다리므로 잠깐 멈출 수 있다. 입력 event가 없는 frame에는 대기를 생략한다.

### 3. Frame-scoped staging buffer에서 GPU copy

CPU upload와 Solver가 접근하는 buffer를 분리해 전체 queue idle 대기를 피할 수 있다. 대신 frame slot별 staging resource, transfer usage, GPU copy 명령, transfer/compute memory barrier와 수명 관리를 추가해야 한다. 현재 MVP에는 구현 비용이 커 후속 최적화로 둔다.

## Consequences

- CPU upload가 이전 Solver의 read/clear와 겹치지 않는 순서를 갖는다.
- 기존 `InputDelta` buffer layout과 한 번 소비 후 clear하는 의미를 유지한다.
- 새 입력이 발생한 frame의 CPU 지연은 허용한다. 입력 빈도 또는 지연 문제가 확인되기 전까지 staging 경로는 도입하지 않는다.
- 이후 compute가 graphics queue와 다른 queue에서 실행되면 대기 대상과 queue 간 synchronization을 다시 결정해야 한다.

## Related

- [[0011-GPU-Resource-Initialization-and-ABI|ADR 0011 — GPU Resource Initialization, Descriptors, and CPU↔GPU ABI]]
- [[../03_Architecture/0004_Surface-State-Update|Surface State Update]]
- [[../05_Development/Notes/0003_Surface-State-GPU-Resource|Surface State GPU Resource]]
