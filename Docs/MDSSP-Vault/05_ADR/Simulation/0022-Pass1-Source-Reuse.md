# ADR 0022 — Pass 1 source 재사용과 가용량 0 생략

- Status: **Accepted · 구현 및 GPU 회귀 검증 완료**
- Date: 2026-09-28

## Context

방향별 RawFlux 캐시는 Pass 2의 flux 재평가를 제거했지만 Pass 1은 source의 법선·중력 투영·위치를 이웃마다 계산했다. source 포화도·Profile 조회·지원 검사도 반복되었고 instance 공통 inverse-transpose는 텍셀마다 준비했다. State가 비어 있어도 형상 계산 후 alpha로 실제 유출을 0으로 제한했다.

## Decision

1. Pass 1은 texel·Registry channel별 가용량 `max(Current - Decay, 0)`을 먼저 계산한다. unsupported/invalid, 가용량=0 또는 dt=0이면 방향별 RawFlux·RawOutgoing·alpha를 모두 0으로 덮어쓰고 평가를 생략한다. 별도 활동 마스크는 두지 않는다.
2. Pass 2의 Incoming gather·InputDelta 소비·Decay·Next 갱신은 유지한다. 빈 텍셀도 유입을 받으며 이번 step의 유입·입력은 다음 step부터 outgoing 대상이다.
3. source 지원 확인·프로파일 파라미터·포화도는 channel당 준비하여 이웃 평가가 재사용한다. target의 지원 여부·포화도와 edge 방향·길이는 이웃별로 평가한다.
4. source 법선의 world 변환·정규화, 중력 투영의 정규화된 방향, displaced source 위치는 geometry rate가 양수인 활성 채널이 있을 때 invocation당 한 번 준비한다. 여러 채널도 이 source 형상을 공유한다. 채널별 edge 배열은 추가하지 않는다.
5. CPU는 dispatch당 한 번 instance 선형 변환·inverse-transpose·gravity up을 준비한다. push constant는 128 byte로 Vulkan의 최소 보장 범위 안에 둔다. offset 0의 header 16 B, offset 16의 world gravity vec4 16 B, offset 32의 model 선형 변환 3×vec4 48 B, offset 80의 normal matrix와 up 3×vec4 48 B다. 마지막 column들의 xyz는 normal matrix, w는 up의 x/y/z다. Translation은 endpoint 차이에서 상쇄된다.
6. singular transform·영/비유한 중력·비유한 source normal의 GeometryDrive=0 처리를 유지한다. State 용량·Registry·2-pass·descriptor·기존 buffer/barrier 구조는 유지한다.

## Alternatives Considered

- source 형상만 재사용: 임시 후보에서 효과가 확인되었지만 빈 State의 불필요한 계산과 instance 행렬 계산이 남았다.
- 활동 비트 마스크로 전체 텍셀 갱신 생략: 자기 가용량만으로 incoming 여부를 알 수 없으며 이번 변경에 필요하지 않다.
- 모든 channel의 edge GeometryDrive를 배열로 저장: 과거 1채널 측정에서 안정적인 이득이 없었고 register 비용이 생기므로 미채택이다.
- Decay를 두 pass 사이에 저장: 반복 계산은 줄지만 추가 저장·읽기 비용이 생겨 별도 측정 전에는 유지한다.

## Consequences

- State가 빈 영역은 큰 형상·포화도 계산을 건너뛰며 slot-major cache zero store는 계속 수행한다. Dispatch 수와 Pass 2의 target 갱신은 그대로다.
- 추가 storage buffer payload는 0 B다. source 재사용 값은 invocation-local이며 push constant만 초기 96→128 B로 변경한다.
- 가용량=0 또는 dt=0 경로의 제한 전 RawOutgoing과 alpha 디버그 값은 초기 구현과 다를 수 있다. 실제 outgoing과 Next State 계약은 보존한다.
- source 공통 계산 재사용은 동일 해상도에서도 효과가 있으며 해상도 축소에 의한 작업량 감소와 구분한다.

## Validation

전체 build와 GPU 회귀 검사가 통과했다. 빈 source 입력의 다음 step 전달, 빈 target의 incoming, 감쇠로 가용량이 0인 source의 입력 보존과 poisoned flux zero overwrite를 추가 검증했다. 비균일 scale·회전·이동, singular transform, 영/비유한 중력, geometry toggle, geometry를 쓰지 않는 채널 뒤의 여러 활성 Registry channel과 서로 다른 rate·Capacity를 포함한다.

합성 비교는 Apple M1, 6 Surface×512×512, 1채널, 같은 grid·Profile·dt·초기 State를 사용했다. 각 dispatch는 같은 A를 읽고 B에 기록하며 5회 warmup 후 30회 timestamp 중앙값을 구했다. 두 번 반복하고 순서를 뒤집었다. 실제 Cube Scene·Normal Map·렌더링은 포함하지 않는다. CPU preparation/upload는 측정 밖이며 두 버전은 각자 일치하는 push constant ABI를 사용했다. 구체적 fixture는 [[../../06_Development/Experiments/0003_Pass1-Cost-Analysis|Pass 1 비용 분석]]과 같다.

| State | 이전 Pass 1 run 1 / 2 (ms) | 적용 후 Pass 1 run 1 / 2 (ms) | 전체 Solver run 1 / 2 (ms) |
|---|---|---|---|
| 전체 양수 | 13.012 / 13.023 | 10.027 / 8.931 | 16.203→13.500 / 16.351→12.415 |
| 약 1.56% 양수 | 13.167 / 13.234 | 2.019 / 2.081 | 15.416→4.362 / 15.474→4.693 |
| 전체 0 | 16.988 / 13.744 | 2.028 / 1.882 | 21.057→4.481 / 15.971→4.155 |

Next State 전체 배열의 최대 절대 오차는 모든 run에서 0이었다. inactive source의 RawOutgoing·alpha는 새 분기의 디버그 계약이 달라 비교 기준으로 사용하지 않았다. Pass 1 감소는 전체 양수 약 23–31%, 1.56% 양수 약 84%, 전체 0 약 86–88%다. run 편차가 있으며 실제 Scene FPS 개선률로 환산하지 않는다.

## Related

- [[0021-Directional-RawFlux-Cache|ADR 0021 — RawFlux 캐시]]
- [[0020-State-Overcapacity-Transport|ADR 0020 — State 초과량 보존]]
- [[0023-Simulation-Resolution-Presets|ADR 0023 — 해상도 프리셋]]
- [[../../04_Architecture/0007_Surface-Solver-Cache|Solver Cache]]
