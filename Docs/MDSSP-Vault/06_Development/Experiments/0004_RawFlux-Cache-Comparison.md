# RawFlux 캐시 ON/OFF 비교 실험

- Date: 2026-09-28
- Status: **초기 ON/OFF 경로의 합성 GPU 비교 완료 · ADR 0025 이전 측정 기록**
- 적용 범위: 아래 수치는 빈 source의 RawFlux를 0으로 덮어쓰던 초기 ON 구현이다. [[../../05_ADR/0025-Inactive-RawFlux-Write-Elision|ADR 0025]] 이후에는 해당 쓰기를 생략하므로 현재 성능 수치로 사용하지 않는다.
- 결정: [[../../05_ADR/0024-RawFlux-Cache-Comparison|ADR 0024 — RawFlux 캐시 ON/OFF 비교]]

## 비교 조건

- Apple M1, Vulkan/MoltenVK, 6 Surface × 512×512 = 1,572,864 texel, Registry 1채널, 1 instance.
- 같은 GPU 리소스와 Profile을 사용하며 ON/OFF 각각 specialization pipeline을 선택한다. source 재사용, 빈 source 생략, CPU instance 행렬 계산, 다른 Solver 항목은 양쪽에 유지한다. 메모리 할당은 바꾸지 않는다.
- 각 Surface의 texel `(x,y)` 위치는 `(0.01x, 0.01y, 0.0025x)`, 법선은 `normalize(-0.25,0,1)`이다. 8방향 grid 이웃이며 경계 밖 이웃은 invalid다. 모든 texel은 같은 Profile을 지원하며 Normal Map, 렌더링, 이벤트 입력은 제외한다.
- Capacity=1, saturation rate=0.2, geometry rate=50, decay rate=0.03, cavity retention factor=0.5, instance transform=identity, gravity=(0,0,-1), dt=1/60초.
- dense는 모든 source의 State가 `0.25 + 0.5 × (global index % 31)/30`이다. sparse는 `global index % 1024 < 16`에만 같은 양수를 넣고 나머지는 0이다. dry는 전체 0이다.
- 각 모드의 측정 전 동일한 initial State를 A에 업로드한다. 모든 dispatch는 같은 A를 읽고 B에 기록하여 시뮬레이션 진행에 따른 조건 변화를 제거한다. 5회 warmup 후 30회 GPU timestamp의 중앙값을 구한다. 두 번째 반복에서는 OFF→ON으로 순서를 뒤집는다.
- CPU 준비·업로드·pipeline 생성·download는 시간 측정 밖이다. 전체 구간은 Pass 1 시작부터 Pass 2 종료까지이며 중간 barrier 구간을 포함한다. UI의 Solver GPU 값은 두 pass 구간 합을 평균하므로 이 전체 중앙값과 집계 방식이 다르다.
- 비교 fixture의 버퍼 payload는 RawFlux float32 `1,572,864 × 1 × 8 × 4 B` = 48 MiB, 공유 reverse slots uint32 `1,572,864 × 4 B` = 6 MiB로 합계 54 MiB다. 두 배열은 원소당 4 B이며 추가 원소 padding이 없다. ON/OFF 모두 동일하게 할당한다. allocator overhead, pipeline 메모리 및 기존 State·형상 버퍼는 제외한다. 실제 Scene의 instance·공유 조합 수와 구분한다.

## 결과

시간 단위는 ms이며 ON/OFF 열은 각각 반복 1 / 반복 2다.

| State 분포 | ON Pass 1 | OFF Pass 1 | ON Pass 2 | OFF Pass 2 | ON 전체 | OFF 전체 |
|---|---|---|---|---|---|---|
| 전체 양수 | 8.937 / 9.255 | 11.708 / 8.758 | 3.308 / 3.404 | 13.878 / 13.400 | 12.321 / 12.615 | 25.535 / 22.290 |
| 약 1.56% 양수 | 1.952 / 1.978 | 1.188 / 1.105 | 2.170 / 2.195 | 2.438 / 2.336 | 4.181 / 4.241 | 3.675 / 3.592 |
| 전체 0 | 2.101 / 1.850 | 1.198 / 1.043 | 2.528 / 2.123 | 2.714 / 2.194 | 4.494 / 4.007 | 3.901 / 3.303 |

- 전체 양수에서는 ON의 전체 GPU 구간이 OFF보다 약 43–52% 짧았다. Pass 2의 방향별 geometry·saturation 재평가를 없애는 이득이 크다.
- 약 1.56% 양수에서는 ON의 전체 구간이 OFF보다 약 14–18% 길었다. 빈 source의 계산은 이미 생략되므로 재계산 제거 이득은 작고 ON의 방향별 cache zero store 비용은 남는다.
- 전체 0에서는 ON의 전체 구간이 OFF보다 약 15–21% 길었다. 두 모드 모두 incoming 재평가가 없어 Pass 1의 cache zero store 제거가 OFF에 유리했다.
- 모든 결과 배열 Next·RawOutgoing·alpha의 ON/OFF 최대 절대 차이는 전체 양수·sparse에서 5.96×10⁻⁸ 이하, dry에서 0이었다. specialization에 따른 부동소수점 반올림 수준이다.
- GPU clock·열 상태와 compiler 최적화에 따른 반복 편차가 있다. 캐시 사용이 항상 빠르다는 결론이나 실제 Cube Scene FPS 개선률로 환산하지 않는다. 반복 1의 dense Pass 1 편차도 유지하여 기록했다.

## 실제 Scene에서 비교

1. Solver 탭의 Cache Comparison을 펼쳐 RawFlux Cache를 선택한다. 기본값은 ON이다.
2. 동일한 해상도·다른 Solver 항목·Profile·transform을 유지하고 맨 오른쪽 Global Settings 탭에서 Fixed timestep ON·Auto substepping OFF를 사용한다. 양쪽에서 같은 Time scale을 사용한다. Solver step당 1/60초이며 배속은 누적 시간에 적용된다. frame당 Solver 반복 수와 backlog도 함께 기록한다.
3. 각 모드에서 Reset State 후 같은 입력을 재현한다. 같은 초기 State 없이 실행 중 토글한 숫자는 동등한 조건의 A/B 측정으로 해석하지 않는다. 입력 위치·강도·횟수와 경과 step을 맞춰야 한다.
4. warmup과 전환 직후 첫 평균을 지나서 Pass 1·Pass 2·Solver GPU를 읽는다. 화면의 모드 표시와 cache buffer MiB를 함께 기록한다. OFF에서도 할당은 유지되며 추가 VRAM 절감은 발생하지 않는다.

UI는 토글·고정 시간 간격·실제 buffer 크기·모드별 평균 초기화를 제공한다. 자동 State snapshot/replay와 정해진 step 수의 benchmark 실행은 현재 제공하지 않는다.
