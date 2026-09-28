# ADR 0024 — RawFlux 캐시 ON/OFF 비교

- Status: **Accepted · 구현 완료**
- Date: 2026-09-28

## Context

방향별 RawFlux 캐시는 Pass 2의 재계산을 줄이는 대신 Pass 1의 저장, Pass 2의 읽기, instance별 GPU 버퍼가 필요하다. source 재사용과 가용 State가 없는 source의 계산 생략도 적용되어 있으므로, 캐시의 효과를 비교하려면 이 최적화들을 동일하게 유지한 ON/OFF 경로가 필요하다.

## Decision

1. Simulation 탭에 기본 ON인 `RawFlux Cache` 체크박스를 둔다. INI 수정이나 재시작 없이 전환한다. State, 현재 A/B 방향, 입력, Profile 설정, 해상도는 보존한다.
2. ON은 Pass 1에서 제한 전 방향별 RawFlux를 저장하고 Pass 2에서 읽는다. OFF는 RawFlux 버퍼의 모든 저장·읽기를 생략하고, Pass 2에서 이웃 source→target flux를 재계산한다. Pass 1의 source 재사용·가용량 검사, RawOutgoing·alpha, Pass 2의 incoming·input·decay 처리는 동일하게 유지한다. 새로운 pass는 추가하지 않는다.
3. OFF도 실제 source의 reciprocal slot을 사용하여 source-side TransferWeight를 읽는다. 두 모드 모두 공유 reverse-slot 메타데이터를 사용하며 임의로 반대 방향 slot을 가정하지 않는다.
4. 두 pass 각각 캐시 ON/OFF pipeline을 shader specialization constant 0으로 생성한다. CPU의 SolverFlags bit 5는 OFF pipeline과 barrier 범위를 선택한다. 사용하지 않는 shader 경로가 ON pipeline의 비용에 영향을 주지 않도록 한다. 초기 pipeline 생성 작업은 늘지만 런타임 전환 시 재컴파일하지 않는다.
5. 전환 전 GPU 작업을 완료하고 이전 모드의 미수집 timestamp 및 UI 평균을 버린다. 화면의 Pass 1·Pass 2·Solver 시간과 함께 현재 ON/OFF 모드를 표시한다. 새 모드의 평균은 새 측정으로 수집한다.
6. 비교용 `Fixed timestep (1/60 s)` 옵션을 둔다. 활성화하면 frame elapsed time 대신 solver step당 1/60초를 사용하며 Time scale을 곱한다. 기본은 기존 가변 시간 간격이다. 고정 옵션에서는 실제 시간 대비 시뮬레이션 속도가 FPS에 따라 달라진다.
7. OFF에서도 버퍼 할당은 유지한다. 표시하는 메모리는 실제 RawFlux buffer 크기를 모든 instance에 대해 합하고, reverse-slot buffer 크기를 공유 Geometry마다 한 번 더한 값이다. allocator alignment·메모리 블록 overhead·pipeline 메모리는 포함하지 않는다. OFF는 연산과 버퍼 접근 비용을 비교하는 경로이며 메모리 절감 경로가 아니다.

## Alternatives Considered

- INI만 변경: 실행마다 재시작이 필요하고 State와 조건을 맞춰 비교하기 어렵다.
- ON/OFF를 uniform shader branch로 선택: 구현은 간단하지만 재계산 경로를 포함하는 shader가 ON 측정에도 영향을 줄 수 있어 specialization을 채택한다.
- 전환 시 RawFlux 버퍼 해제·재할당: 메모리 사용량 비교에는 적합하지만 전환 비용과 descriptor 재구성이 개입하므로 현재 비교 기능에서는 할당을 유지한다.

## Consequences

- 같은 해상도, Profile, transform, 다른 Solver 항목, 초기 State, 입력, 시간 간격으로 비교해야 한다. 실행 중 토글만 바꾸면 시뮬레이션이 진행되어 서로 다른 State를 측정하므로 통제된 A/B 실험이 아니다.
- 수동 비교는 고정 시간 간격과 동일한 Time scale을 사용하고, 각 모드에서 Reset State 후 동일한 입력을 재현하고, warmup 후 평균을 읽는다. UI는 자동 snapshot/replay benchmark를 제공하지 않는다.
- 캐시가 유리한 정도는 활성 source 비율과 GPU에 따라 달라진다. 전체 텍셀 dispatch와 Pass 2 갱신은 두 모드 모두 유지된다. 빈 영역에서도 ON은 cache zero store를 수행하지만 OFF는 생략한다.
- 캐시 메모리 payload는 `instance별 Σ(TexelCount × RegistryChannelCount × 8 × 4 B) + 공유 Geometry별 Σ(TexelCount × 4 B)`다. RawFlux는 float32, reverse slots는 uint32이며 두 배열 모두 원소당 4 B, 추가 원소 padding은 없다. Registry channel 수는 로드한 `.SRProfile`들의 State 키로 결정된다.
- 예를 들어 6 Surface, 256×256, 1채널, 4 instance, 2 공유 Geometry 조합이면 RawFlux 48 MiB + reverse slots 3 MiB = 51 MiB다. 같은 구성의 128×128은 12.75 MiB, 512×512는 204 MiB다. State·Input·형상·TransferWeight 등 기존 버퍼는 이 합계에 포함하지 않는다.

## Validation

- 전체 build 및 5개 CTest를 통과했다. Vulkan validation과 synchronization validation을 활성화하여 검사했다.
- 서로 다른 reciprocal slot·Profile·Capacity, 여러 Registry channel, unsupported/invalid texel, overcapacity, decay, InputDelta 조건에서 ON/OFF의 Next·RawOutgoing·alpha를 비교했다. OFF에서 poisoned RawFlux buffer가 그대로 유지되면서 결과가 ON과 일치함을 확인했다.
- 동일 submission에서 ON/OFF와 AB/BA를 연속 전환하고 dt=0을 포함하여 이전 cache를 재사용하지 않는지 확인했다. GeometryDrive, Meso/Macro 방향, 비균일 scale·회전, singular transform·영/비유한 중력, 빈/감쇠로 비워진 source를 두 모드에서 기존 예상값과 비교했다.
- 실제 Renderer/API 통합 구동에서 기본 ON, 전환 시 descriptor·현재 A/B·입력 보존, 이전 timestamp 무효화, 양쪽 모드의 timestamp 재수집, OFF 입력 소비, 256→128 해상도 재구성 후 OFF 유지와 메모리 집계를 검증했다. 4 instance·2 공유 Geometry·1채널 구성의 집계는 256에서 RawFlux 48 MiB + reverse slots 3 MiB, 128에서 12 MiB + 0.75 MiB였다. UI 클릭을 통한 시각적 검증은 수행하지 않았다.
- [[../../06_Development/Experiments/0004_RawFlux-Cache-Comparison|RawFlux 캐시 비교 실험]]에서 동일 initial State·dt의 합성 fixture를 두 모드로 측정했다. 전체 양수 조건은 ON이 전체 GPU 구간을 약 43–52% 줄였고, 약 1.56% 양수 및 전체 0 조건은 OFF가 빨랐다. 실제 demo Scene FPS 개선률로 환산하지 않는다.

## Related

- [[0021-Directional-RawFlux-Cache|ADR 0021 — 방향별 RawFlux 캐시]]
- [[0022-Pass1-Source-Reuse|ADR 0022 — Pass 1 source 재사용]]
- [[0023-Simulation-Resolution-Presets|ADR 0023 — 시뮬레이션 해상도]]
- [[../../06_Development/Experiments/0004_RawFlux-Cache-Comparison|RawFlux 캐시 비교 실험]]
- [[../../01_Project-Policy/0003_Commit-Message-Style|커밋 메시지 규칙]]
