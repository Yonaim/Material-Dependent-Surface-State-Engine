# Completed Plan History

> **한 줄 요약:** 완료된 주차별·Branch별 계획 문서를 짧은 이력으로 압축한 기록이다.

이 문서는 과거 계획의 **요약 기록**이다. 현재 프로젝트 방향은 [[Roadmap|Roadmap]], 현재 설계는 [[03_Architecture/0000_Overview|Architecture]]를 기준으로 한다.

## 주차별 목표

| Week | 당시 목표 |
|---:|---|
| 1 | Vulkan 렌더링 엔진과 Surface State System의 책임·연결 관계를 정하고, 첫 구현 범위를 Static Mesh로 한정한다. |
| 2 | 표면 상태·재질 반응·형상 데이터를 정의하고, State가 입력을 받아 다음 상태로 갱신되는 규칙을 정리한다. |
| 3 | Vulkan 기반 실행 경로에서 Mesh와 Material을 로드하고 Static Mesh를 렌더링한다. |
| 4 | 준비된 Simulation UV를 가진 테스트 Mesh에서 Surface data 생성부터 GPU Solver와 접촉 입력까지 최소 end-to-end 경로를 연결한다. |
| 5 | Input, Transport, Decay를 통해 이웃 texel 사이 State가 finite·비음수를 유지하고 Capacity 초과량을 보존하며 갱신되는지 검증한다. |
| 6 | 재질 내부에 흡수된 수분인 Wetness의 입력·전파·감쇠와 렌더링 반응을 구현한다. |
| 7 | Mud의 전파·잔류·적층을 Wetness와 결합해 중간 시연 가능한 결과를 만든다. |
| 8 | 중간 Demo를 기준으로 구현 품질과 위험을 평가하고 후반 작업 범위를 조정한다. |
| 9 | Heat 입력·전파·Cooling을 구현하고 재질별 열 반응을 확인한다. |
| 10 | Heat Saturation 조건을 만족할 때 Burn이 증가하고, Heat 냉각 뒤에도 Burn 흔적이 남도록 한다. |
| 11 | 표면의 높이·방향·곡률이 이동과 잔류에 미치는 영향을 조정하고 경계 사례를 개선한다. |
| 12 | 기본 데모에 필요한 State들을 하나의 Registry 기반 Surface State System과 공통 update path에서 처리한다. |
| 13 | 측정으로 확인된 병목에 한해 Compute pass, memory 접근과 불필요한 계산을 개선한다. |
| 14 | 대표 workload에서 해상도·Instance·갱신 조건에 따른 FPS, GPU 시간과 메모리 변화를 측정한다. |
| 15 | 기능·형상·해상도 설정에 따른 차이와 실패 조건을 재현 가능한 자료로 정리한다. |
| 16 | 구현물과 증거 자료를 정리해 재현 가능한 최종 Repository, 보고서, Demo와 발표 자료를 완성한다. |

## 세부 구현 계획 이력

| 시점 | 계획 | 요약 |
|---|---|---|
| Week-04 | 4주차 구현 브랜치 계획 | 이 문서들은 4주차 구현을 작은 검증 단위로 나누기 위한 임시 상세 계획이다. |
| Week-04 | Branch 1 — Surface Data Contract | CPU 자료형, State Registry, ID와 소유권 계약을 정해 Mapping·GPU·Solver 구현의 공통 기반을 만든다. |
| Week-04 | Branch 2 — Simulation Mapping | 준비된 Simulation UV를 검증하고 rasterization, texel 유효성, 이웃 graph와 seam 연결을 구현한다. |
| Week-04 | Branch 3 — Shared Geometry Build | Simulation Mapping 결과에서 공유 texel 위치·normal·neighbor와 Geometry 데이터를 생성한다. |
| Week-04 | Branch 4 — Surface GPU Resources | 공유 Geometry, Profile, 인스턴스 State와 입력 데이터를 Vulkan buffer 및 descriptor로 구성한다. |
| Week-04 | Branch 5 — Surface Solver 2-Pass | GPU의 두 Compute pass로 표면 State를 갱신하고 flux와 Capacity 동작을 검증한다. |
| Week-04 | Branch 6 — Surface Input Integration | 접촉 이벤트를 texel별 InputDelta로 누적해 Surface Solver 입력 경로에 연결한다. |
| Week-05 | 5주차 구현 상세 계획 — Solver 확장과 검증 | 4주차에 구축한 2-Pass Solver 경로를 바탕으로 `GeometryDrive`, Normal Map 기반 표면 방향, `TransferWeight`를 추가하고, 계산 중간값과 실행 상태를 Debug UI에서 확인한다. |
| Week-05 | Branch 1 — Solver Geometry Drive | 높이차와 중력 방향을 반영해 GeometryDrive를 계산하고 GPU Solver의 전달에 연결한다. |
| Week-05 | Branch 2 — Solver Transfer Weights | 이웃 사이 거리·normal·곡률·Profile 경계에 따른 TransferWeight 계산을 정의하고 구현한다. |
| Week-05 | Branch 2.1 — Solver Transfer Cache | 반복되는 이웃 TransferWeight와 RawOutgoing 계산 결과를 재사용하도록 Solver 캐시를 추가한다. |
| Week-05 | Branch 2.2 — Solver Normal Map Weights | Simulation texel별 Normal Map 방향을 TransferNormal으로 변환해 NormalWeight에 사용한다. |
| Week-05 | Branch 2.3 — Solver Virtual Meso Geometry from Normal Map | Normal Map 방향을 적분해 MesoVirtualHeight와 곡률·오목도 파생 데이터를 생성한다. |
| Week-05 | Branch 3 — Solver Debug Tools | Solver의 State·Geometry·Flux 중간값과 실행 상태를 확인하는 Debug UI를 추가한다. |
| Week-05 | Branch 6 — Solver Week 5 통합 검증 | 5주차 Solver의 보존, source 보유량 제한·초과량 보존, 형상 가중치와 경계 동작을 테스트와 GPU 실행으로 검증한다. |
| Week-06 | 6주차 브랜치 계획 — Wetness 구현 | 완료된 generic Surface State Solver와 GeometryDrive 계약 위에서 `Wetness` State의 입력·전파·감쇠를 Profile별로 조정하고, Wetness가 재질 색과 roughness에 반응하도록 연결한다. |

> 세부 계획 원문은 정리 전 원본 볼트와 Git 이력에서 확인한다. 현재 문서에는 앞으로 참고할 가치가 있는 목표와 분할 기준만 남긴다.
