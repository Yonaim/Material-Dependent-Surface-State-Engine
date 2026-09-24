# 형상 정보 전처리 메모

상태: **설계 / 알고리즘 검증 필요** · 근거: [[05_Assets/Documents/0006_Geometry-Integration.pdf|형상 정보 반영]]

정적 형상 전처리의 목표는 Mesh + Normal/Height detail에서 Solver가 읽을 texel별 Geometry Data를 만드는 것이다.

```text
Mesh Geometry + Normal / Height Detail
→ Normal
→ Distance
→ Meso Virtual Height
→ Curvature / ConcavityWeight
→ SharedSurfaceGeometryData
```

Normal Map의 모든 경우가 integrable하지는 않으므로 Meso Virtual Height 복원 알고리즘과 Non-Integrable fallback을 검증해야 한다. [[04_Development/Experiments/0001_Normal-Map-Integration|실험]]

Simulation UV 생성, Mesh→Texel mapping, Valid Mask, UV Seam, Neighbor Index는 [[04_Development/Notes/0000_Surface-Simulation-Mapping|Surface Simulation Mapping]]에서 정의한다. 이 문서는 그 결과 위에서 Meso Virtual Height와 Curvature/Concavity를 만드는 단계에 집중한다.
