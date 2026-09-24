# Surface State Rendering

상태: **방향 설계** · 근거: [[05_Assets/Documents/0006_Geometry-Integration.pdf|형상 정보 반영]], [[05_Assets/Documents/0007_Target-Demos.pdf|목표 데모]]

Rendering은 State에 따른 **외관 변화**와 Accumulation에 따른 **형상 높이 변화**를 구분한다.

## 최종 Meso Height

$$
FinalMesoHeight
=
MesoVirtualHeight
+
AccumulationHeight
$$

Simulation 관점의 전체 높이는 다음과 같다.

$$
DynamicFinalHeight
=
MacroHeight + MesoVirtualHeight + AccumulationHeight
$$

## 적용 방식

| 방식 | 적용 단계 | 실제 Geometry 변화 |
|---|---|---|
| Height 기반 Shading / Parallax | Fragment Shader | X |
| Geometry Displacement | Vertex / Tessellation / Mesh 단계 | O |

Mud·Snow처럼 실제 두께 변화가 중요한 적층은 Geometry Displacement 계열이 필요할 수 있다. 정확한 렌더링 경로와 성능 기준은 구현·실험 후 확정한다.

## 상태별 외관 예

- `Wetness`: 재질 내부 수분에 따른 색 / roughness 변화.
- `Burn`: 그을림·탄 정도 변화.
- `Mud`: 외관 변화 + `AccumulationHeight`.
- `SurfaceWater`(확장 예정): 표면 물기 / 고임 + 적층 높이 가능.

적층 계산은 [[02_Architecture/0008_Accumulation|적층과 Accumulation Height]]을 본다.
