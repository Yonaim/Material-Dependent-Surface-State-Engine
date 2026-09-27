# Branch 5 — Solver Debug Statistics

브랜치: `feat/solver-debug-statistics`  
선행 조건: `feat/solver-debug-controls` 병합  
관련 설계: [[03_Architecture/0009_UI-Interface|UI Interface]], [[03_Architecture/0007_Surface-GPU-Data-Layout|Surface GPU Data Layout]]

## 목표

현재 Solver 실행 범위와 최근 GPU Solver 시간을 UI에서 확인한다. 통계는 이미 CPU/GPU resource metadata에 있는 값을 활용하며, 전체 State를 CPU로 readback하지 않는다.

## 표시 항목

| 표시 | 정의 |
|---|---|
| Texel count | 현재 Scene에서 Solver가 관리하는 instance별 texel 수의 합. Shared Geometry라도 State가 instance별이므로 instance마다 센다. |
| Valid texel / valid ratio | Geometry mapping의 ValidMask 기준으로 valid 수와 `valid / total` 비율을 표시한다. |
| Current buffer | 다음 step이 읽을 A/B State buffer를 표시한다. |
| Recent GPU solver time | Pass 1 시작 전과 Pass 2 완료 뒤 GPU timestamp를 기록해 가장 최근 완료된 solver step의 경과 시간을 표시한다. |

## GPU timestamp 처리

- 선택한 compute queue family의 `timestampValidBits`와 장치 timestamp period를 확인한다.
- 지원되지 않으면 시간을 CPU 제출 시간으로 대체하지 않고 `N/A (timestamp unsupported)`로 표시한다.
- 완료되지 않은 query 결과를 기다리며 매 frame GPU/CPU를 stall하지 않는다. 완료된 최근 결과를 표시하고, 초기 상태는 `N/A`로 둔다.
- timestamp query pool의 생성·재사용·소멸을 GPU 작업 완료 fence 수명에 맞춘다.

## 구현 대상

- instance별 texel/valid count 조회 helper 또는 불변 metadata 집계.
- 현재 ping-pong 방향 노출.
- Solver pass 주변 Vulkan timestamp query 기록 및 완료 결과 수집.
- Debug UI `Solver` 섹션에 통계 라벨 추가.

## 검증

- 공유 Geometry를 사용하는 복수 instance에서 count가 instance scope로 집계된다.
- valid ratio가 모두-valid, 모두-invalid, 혼합 fixture에서 정확하다.
- Current A/B 표시가 실제 다음 descriptor 선택과 일치한다.
- timestamp 지원 GPU에서 값이 유한하고 0 이상이며, query를 기다리느라 frame이 stall하지 않는다.
- timestamp를 지원하지 않는 queue/device에서 N/A가 표시되고 validation 오류가 없다.

## 완료 조건

UI가 현재 Solver 상태를 정확히 보여주고, timestamp 지원 여부에 관계없이 동작한다.

## 제외 범위

프레임 전체 성능 분석기, Raycast/Contact CPU 시간, GPU readback을 이용한 상세 per-texel 통계.
