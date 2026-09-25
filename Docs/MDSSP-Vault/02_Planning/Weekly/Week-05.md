# Week-05 — 기본 Solver 구현·검증

상태: **계획** · 상위 계획: [[02_Planning/0000_Project-Plan|Project Plan]], [[02_Planning/0001_Roadmap|Roadmap]]

## 목표

Input, Transport, Decay를 통해 이웃 texel 사이 State가 Capacity 범위 안에서 안정적으로 갱신되는지 검증한다.

## 작업

- Saturation 차이에 따른 SaturationDrive와 형상 기반 GeometryDrive를 계산한다.
- Distance, Normal, Curvature, Profile Boundary 가중치를 적용한다.
- Decay 후 가용량을 기준으로 alpha를 계산해 Outgoing이 보유량을 넘지 않게 한다.
- 2-Pass gather update, State A/B ping-pong, barrier 순서를 확인한다.
- 균일 상태, 단일 source, Capacity 차이, invalid texel, seam 전파와 큰 rate 조건을 테스트한다.
- 동일한 총 시간에서 서로 다른 timestep 결과 차이를 허용 오차로 비교한다.

## 산출물

- 재현 가능한 기본 Transport / Decay Solver.
- 상태 전파 테스트 결과와 수식·구현 차이 기록.

## 일정 경계

4주차에서 확인한 최소 수직 경로를 확장해 Solver의 계산 정확성과 경계 사례를 검증한다. 4주차 목표를 반복해 새로 구현하는 것이 아니다.

## 참고

- [[03_Architecture/0004_Surface-State-Update|Surface State Update]]
- [[05_Development/Notes/0002_Next-State-Calculation|Next State 계산 메모]]
- [[05_Development/Experiments/0000_Solver-Pass-Comparison|Solver Pass 비교]]
