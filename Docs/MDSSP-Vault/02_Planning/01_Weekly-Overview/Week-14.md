# Week-14 — 성능 실험

상태: **계획** · 상위 계획: [[02_Planning/00_Project-Overview/0000_Project-Plan|Project Plan]], [[02_Planning/00_Project-Overview/0001_Roadmap|Roadmap]]

## 목표

대표 workload에서 해상도·Instance·갱신 조건에 따른 FPS, GPU 시간과 메모리 변화를 측정한다.

## 작업

- 실험 Mesh, Scene, Profile, State field와 실행 환경을 고정한다.
- Texel 해상도, Instance 수, valid texel 비율, update frequency와 Solver 설정을 바꿔 측정한다.
- CPU 전처리·Input upload·Compute pass·Rendering 비용을 가능한 범위에서 분리한다.
- 같은 조건의 반복 측정과 로그를 보존한다.
- 각 성능 실험에서 사용할 Profile 집합과 Registry State 목록을 고정하고 기록한다. 실험 도중 State 집합을 바꾸지 않으며, 별도 실험 간에는 채널 수가 달라도 된다.

## 산출물

- 성능 그래프와 원시 측정 로그.
- 환경·조건·측정 절차 기록.
- 병목과 목표 시각 품질 사이의 trade-off 요약.

## 참고

- [[05_Development/Notes/0003_Surface-State-GPU-Resource|Surface State GPU Resource]]
- [[05_Development/Experiments/0000_Solver-Pass-Comparison|Solver Pass 비교]]
