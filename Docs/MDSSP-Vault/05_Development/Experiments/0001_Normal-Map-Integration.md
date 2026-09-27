# 실험 — Normal Map Integration

- 상태: **NormalWeight 연결 완료 · Meso height 복원 실험 대기**
- 근거: [[07_Assets/Documents/0006_Geometry-Integration.pdf|형상 정보 반영]]
- 일정: Solver `NormalWeight`가 Normal Map 방향을 쓰는 연결은 Week-05 Branch 2.2에서 다룬다. MesoVirtualHeight/Curvature 복원과 Non-Integrable fallback 후보 비교·구현은 Week-05 Branch 2.3으로 옮긴다. Week-08에서는 이 결과를 중간 Demo에서 평가하고 후속 보정 범위를 정한다.

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

### Week-05 Branch 2.2 — NormalWeight

Simulation texel mapping의 triangle/barycentric 좌표로 Normal Map을 샘플링하고, tangent frame에서 mesh-local transfer normal을 복원해 NormalWeight cache에 연결했다. 평탄/기울어진 Map, UV 보간과 chart 경계, repeat 주소 지정, tangent handedness 및 invalid tangent fallback CPU fixture를 추가했다. GPU fixture는 이 normal이 TransferWeight와 State flux에 반영되고, non-uniform instance scale에서 inverse-transpose를 거치며, map 부재 시 geometric normal fallback을 사용하는 것을 확인한다. 2026-09-27 로컬 Apple M1에서 전체 CTest 5/5 통과, 데모 1 frame 실행, Vulkan validation 오류 없음.

### Week-05 Branch 2.3 — Meso geometry

미실시. Normal Map의 적분 가능성, 높이 복원, Curvature/Concavity 생성과 fallback 실험은 Branch 2.3 범위다.
