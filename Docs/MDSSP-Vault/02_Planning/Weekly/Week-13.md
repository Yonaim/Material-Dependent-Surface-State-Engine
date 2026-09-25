# Week-13 — GPU 연산 최적화

상태: **계획** · 상위 계획: [[02_Planning/0000_Project-Plan|Project Plan]], [[02_Planning/0001_Roadmap|Roadmap]]

## 목표

측정으로 확인된 병목에 한해 Compute pass, memory 접근과 불필요한 계산을 개선한다.

## 작업

- 기준 workload와 성능 측정 방법을 고정한다.
- Solver pass 및 Barrier 비용, memory traffic, dispatch 크기를 확인한다.
- 공유 Geometry와 instance State 접근의 중복·불필요한 읽기/쓰기를 분석한다.
- 변경 전·후 결과의 정확성을 같은 test로 비교한다.
- 현재 설계인 네 상태 채널과 dense InputDelta를 기준으로 최적화한다. Sparse 입력으로 표현을 바꾸지 않는다.

## 산출물

- 병목 분석과 GPU 시간 측정.
- 최적화 전·후 비교 및 정확성 확인 결과.

## 참고

- [[05_Development/Notes/0003_Surface-State-GPU-Resource|Surface State GPU Resource]]
- [[05_Development/Experiments/0000_Solver-Pass-Comparison|Solver Pass 비교]]
