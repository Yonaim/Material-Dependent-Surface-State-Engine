# Decision 0005 — 비활성 source의 RawFlux 쓰기 생략

> **한 줄 요약:** 이동량이 없는 texel은 방향별 flux 값을 쓰지 않아 GPU 쓰기를 줄인다.

- 분류: **Simulation**
- Status: **Accepted · 구현 및 GPU 회귀 검증 완료**
- Date: 2026-09-28
- 관련 문서: Directional RawFlux Cache, Pass 1 source 재사용, RawFlux 캐시 비교 실험

## 쉽게 읽기

보낼 양이 없는 source는 방향별 RawFlux를 기록하지 않는다. 다음 단계는 alpha가 0인 source의 캐시를 읽지 않으므로, 불필요한 메모리 쓰기를 줄인다.

## Context — 왜 필요했나

초기 RawFlux 캐시는 매 step 모든 방향의 RawFlux 항목을 덮어썼다. source 재사용과 가용량 0 생략 이후에도 빈 source는 8개 방향의 RawFlux 항목을 0으로 기록했다. Pass 2는 이웃 source의 alpha가 0이면 RawFlux 읽기를 이미 생략하므로 해당 저장은 실제 전달 계산에 사용되지 않았다. 초기 ON/OFF 합성 비교에서 빈 영역의 저장 비용이 재계산 제거 이득보다 큰 경우가 관찰됐다.

## Decision — 무엇을 정했나

1. Pass 1의 unsupported/invalid, `max(Current - Decay, 0) <= 0`, dt=0 경로는 RawOutgoing·alpha만 0으로 기록하고 RawFlux의 8개 방향 인덱스에는 쓰지 않는다. 캐시 ON/OFF 모두 적용한다.
2. RawFlux의 유효성은 현재 step의 source texel·Registry channel별 alpha로 판단한다. Pass 2는 **alpha를 먼저 확인하고 0이면 읽지 않는다**. 미기록 방향 항목에는 이전 값 또는 미초기화 값이 남을 수 있으며, `RawFlux × 0` 계산으로 대체하면 NaN이 전파될 수 있으므로 분기를 유지한다.
3. 활성 source의 캐시 ON 경로는 계속 모든 8개 방향의 RawFlux 항목을 갱신한다. 유효하지 않은 이웃·가중치 0·rate 0 등 flux가 0인 간선도 0으로 기록한다. 이 경로는 RawOutgoing=0이면 alpha=1이므로 0 저장을 제거하지 않는다.
4. 빈 source가 유입·외부 입력을 받아 활성화되면 다음 Pass 1이 해당 source의 모든 방향의 RawFlux 항목을 다시 기록한다. 이번 step의 입력·유입 처리와 소비 시점은 유지한다.
5. 생성·Reset 시 RawFlux 초기화는 추가하지 않는다. 최초 step과 Reset 이후에도 동일한 alpha guard와 활성 source 갱신 규칙으로 안전하게 사용한다. 별도 mask·pass·buffer·barrier 변경은 없다.

## Alternatives Considered — 다른 방법

- 비활성 source의 8개 방향 인덱스에 0 기록 유지: 디버그 값이 단순하지만 사용하지 않는 값을 쓰는 비용이 남는다.
- Pass 2에서 stale 값에 alpha=0을 곱하기: 미초기화·NaN scratch에 안전하지 않으므로 읽기 전 alpha 분기를 유지한다.
- RawOutgoing=0인 활성 source의 방향별 RawFlux의 0 값 기록도 생략: 기존 alpha=1 경로에서 Pass 2가 읽을 수 있어 현재 변경에 포함하지 않는다.

## Consequences — 결정의 영향

- alpha=0인 source의 RawFlux를 디버그 도구나 후속 pass가 직접 읽어 현재 step의 값으로 해석하면 안 된다. RawOutgoing·alpha는 모든 source에서 매 step 갱신된다.
- 메모리 할당량과 descriptor 배치는 유지한다. 추가 GPU payload는 0 B다. 6 Surface×512×512, Registry 1채널, 1 instance가 모두 비활성이면 float32·원소당 4 B·원소 padding 없는 RawFlux 배열의 48 MiB 분량 쓰기를 step마다 생략한다. 이는 논리적 buffer 쓰기량이며 GPU memory transaction 수나 allocator overhead의 추정이 아니다.
- source 계산 생략은 outgoing에만 적용한다. 빈 target의 incoming과 InputDelta·Next 갱신은 계속 수행한다.
- 초기 전체 방향 RawFlux 항목 덮어쓰기 계약은 이 결정으로 대체한다. RawFlux ON/OFF의 초기 측정은 변경 전 기록으로 유지한다.

## Validation — 검증

- Shader 및 GPU resource test target을 빌드했다. Vulkan validation과 synchronization validation을 활성화한 GPU 회귀 검사를 통과했다.
- 빈 source, decay-depleted source, unsupported/invalid channel의 RawFlux에 NaN을 넣어도 올바른 Next가 생성되고 scratch가 보존되는지 확인했다. 외부 입력 소비와 다음 step 전달, 빈 target의 incoming, 재활성화된 source의 8개 방향 인덱스 갱신을 검증했다.
- dt=0에서는 모든 RawFlux가 그대로 남고 RawOutgoing·alpha가 0으로 갱신된다. 이후 rate=0인 활성 source는 stale flux를 0으로 덮어쓰는지 검사했다. ON/OFF와 AB/BA 연속 전환 및 여러 Profile·Capacity·Registry channel 회귀도 통과했다.
- 합성 비교에서는 변경 전 ON, 변경 후 ON, 변경 후 OFF의 세 pipeline 구성을 같은 GPU 리소스와 initial State로 평가했다. 변경 전 SPIR-V를 별도 경로에 보존하여 서로 다른 pipeline을 동일 process에서 준비하고 각 측정 전 A를 동일 값으로 업로드했다. 5회 warmup 후 30회 GPU timestamp 중앙값을 구했으며 두 번째 반복은 실행 순서를 뒤집었다.
- Apple M1, 6 Surface×512×512, 1채널·1 instance, identity transform, gravity=(0,0,-1), dt=1/60초다. grid 위치·법선·Profile·State 분포는 실험 0004와 같고 렌더링·입력·CPU 준비·pipeline 생성은 측정 밖이다. 전체 구간은 두 pass 사이의 barrier 시간을 포함한다.

시간 단위는 ms이며 각 열은 반복 1 / 반복 2다.

| State 분포 | 변경 전 ON Pass 1 | 변경 후 ON Pass 1 | 변경 전 ON 전체 | 변경 후 ON 전체 | 변경 후 OFF 전체 |
|---|---|---|---|---|---|
| 전체 양수 | 12.701 / 11.817 | 10.787 / 10.432 | 16.570 / 15.533 | 14.295 / 13.975 | 22.625 / 22.576 |
| 약 1.56% 양수 | 2.101 / 2.035 | 1.240 / 1.277 | 4.603 / 4.420 | 3.764 / 5.593 | 3.778 / 3.362 |
| 전체 0 | 1.875 / 2.126 | 1.263 / 1.126 | 4.162 / 4.737 | 4.273 / 3.472 | 6.248 / 3.518 |

약 1.56% 양수 조건의 Pass 1은 약 37–41%, 전체 0 조건은 약 33–47% 짧아졌다. 전체 양수 조건은 생략 분기를 실행하지 않으므로 해당 편차를 inactive 쓰기 제거의 직접적인 효과로 해석하지 않는다. Pass 2 및 전체 구간의 편차가 커서 전체 Solver·실제 Scene FPS 개선률은 확정하지 않는다. 변경 전/후 ON의 Next·RawOutgoing·alpha 배열은 모든 조건에서 최대 절대 차이 0이었다. 변경 후 ON/OFF는 전체 양수·sparse에서 최대 5.96×10⁻⁸, dry에서 0이었다.

## Related — 관련 문서

- 방향별 RawFlux 캐시
- Pass 1 source 재사용
- RawFlux 캐시 ON/OFF 비교
- [[03_Architecture/0006_Surface-State-Update|Simulation Optimization]]
