# Week-05 — Solver 가중치와 형상 입력 검증

> **한 줄 요약:** Input, Transport, Decay를 통해 이웃 texel 사이 State가 finite·비음수를 유지하고 Capacity 초과량을 보존하며 갱신되는지 검증한다.

상태: **구현 중** · 상위 계획: [[03_Planning/00_Project-Overview/0000_Project-Plan|Project Plan]], [[03_Planning/00_Project-Overview/0001_Roadmap|Roadmap]]

[[05_ADR/Simulation/0020-State-Overcapacity-Transport|ADR 0020]]에서 Capacity는 포화 기준량으로 변경했다. Shader의 Saturation·Next 상한 clamp 제거는 구현했으며 빌드는 통과했다. 이전 상한 계약의 실행 결과와 대기 중인 새 계약 GPU 검증을 구분한다.

## 목표

Input, Transport, Decay를 통해 이웃 texel 사이 State가 finite·비음수를 유지하고 Capacity 초과량을 보존하며 갱신되는지 검증한다.

## 작업

- Saturation **차이에** 따른 SaturationDrive와 형상 기반 GeometryDrive를 계산한다.
- Distance, Normal, Curvature, Profile Boundary 가중치를 적용한다.
- Normal Map의 texel별 tangent-space normal을 Solver `NormalWeight` 입력으로 연결하고, 맵이 없을 때의 fallback을 검증한다.
- Normal Map에서 `MesoVirtualHeight`를 복원하고 높이장으로부터 Curvature/Concavity를 생성하는 별도 브랜치를 수행한다.
- Decay 후 가용량을 기준으로 alpha를 계산해 Outgoing이 보유량을 넘지 않게 한다.
- 2-Pass gather update, State A/B ping-pong, barrier 순서를 확인한다.
- 균일 상태, 단일 source, Capacity 차이, invalid texel, seam 전파와 큰 rate 조건을 테스트한다.
- 동일한 총 시간에서 서로 다른 timestep 결과 차이를 허용 오차로 비교한다.
- 검증을 돕는 Debug UI를 추가한다: TransferWeight 및 구성 가중치 히트맵, Solver pause/step, State 초기화, 전체 texel 수와 valid 비율, 현재 ping-pong buffer, 최근 GPU solver 시간.
- `OutgoingFluxScale` 디버그 뷰를 추가해 Solver가 계산한 outgoing flux 제한값을 확인한다.

위 관측·제어 기능은 서로 같은 Solver 상태와 GPU 리소스 수명에 의존하므로 상세 계획의 Branch 3 통합 브랜치 `feat/solver-debug-tools`에서 함께 구현한다.

## 산출물

- 재현 가능한 기본 Transport / Decay Solver, Normal Map 기반 표면 방향 가중치와 Virtual Meso Geometry 파생값.
- 상태 전파 테스트 결과와 수식·구현 차이 기록.
- Solver 동작과 상태를 확인할 수 있는 최소 Debug UI 및 `OutgoingFluxScale` 시각화.

## 일정 경계

4주차에서 확인한 최소 수직 경로를 확장해 Solver의 계산 정확성과 경계 사례를 검증한다. 4주차 목표를 반복해 새로 구현하는 것이 아니다.

## 구현 상세

- [[03_Planning/02_Weekly-Details/Week-05/0000_Week5-Branch-Plan|5주차 구현 상세 계획]]

## 참고

- [[04_Architecture/0006_Surface-State-Update|Surface State Update]]
- [[06_Development/Notes/0002_Next-State-Calculation|Next State 계산 메모]]
- [[06_Development/Experiments/0000_Solver-Pass-Comparison|Solver Pass 비교]]
