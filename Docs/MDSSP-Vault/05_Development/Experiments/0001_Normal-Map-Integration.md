# 실험 — Normal Map Integration

- 상태: **계획**
- 근거: [[07_Assets/Documents/0006_Geometry-Integration.pdf|형상 정보 반영]]
- 일정: Week-08 중간 결과 검토에서 후보 기법을 비교한다. 알고리즘 확정 후 구현 branch를 Roadmap에 추가한다.

## 가설

Normal Map에서 Meso Virtual Height를 복원해 GeometryDrive / Curvature / Accumulation에 사용하려면 integrable 여부에 따라 처리 전략이 필요하다.

## 측정 조건과 방법

- 적분 가능한 평탄 / 볼록 / 오목 Normal Map.
- 경로에 따라 복원 결과가 달라지는 Non-Integrable Normal Map.
- 여러 texel scale에서 복원 결과 비교.

## 관찰 항목

- Height discontinuity.
- Curvature / Concavity의 안정성.
- Transport 방향 편향.
- Cavity Filling 결과.
- 전처리 비용.

## 결과

미실시.
