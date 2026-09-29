# 렌더링 구현 검토

> **한 줄 요약:** State 외관 변화와 Accumulation 형상 변화를 렌더링 경로에 연결할 구현 선택지를 기록하고 검증한다.

상태: **Wetness·Mud 데모 Lit / 디버그 연결면 구현** · 관련 문서: [[04_Architecture/0009_Rendering|Surface State Rendering]], [[04_Architecture/0004_Surface-Geometry|적층과 Accumulation Height]]

Architecture 문서는 렌더링 결과의 의미와 요구사항을 정의한다. 이 문서는 해당 결과를 실제 렌더링 경로에 연결할 때 선택·검증할 구현 항목을 기록한다.

현재 높이 디버그는 compute에서 texel별 위치·normal을 계산하고 같은 UV chart 안에서 연결한 삼각형으로 표시한다 ([[../../05_ADR/0036-Texel-Geometry-Preview|ADR 0036]]). 독립 surfel 패치는 채택하지 않는다. 원본 메시 정점 밀도로 제한되던 초기 미리보기와 구분하며, 현재 Mud 데모 Lit에서도 같은 표시 형상을 사용한다. 물리적 다중 layer·Solver 형상 피드백의 방식 확정을 뜻하지 않는다.

## 현재 데모 구현

Wetness·Mud 이름의 현재 Registry ID를 조회하는 adapter와 Profile-aware State 샘플링을 연결했다. Wetness는 darkening·roughness 감소, Mud는 갈색 피복·roughness와 computed texel 변위를 적용한다. 공통 GGX 조명은 카메라 위치를 frame별 uniform에서 읽는다. State·Solver 계약과 Profile 동적 등록은 유지한다. 높이 grid는 변위 면의 UV에서 texel 묶음 경계를 표시하며 삼각형 대각선을 표시하지 않는다. 상세 결정과 남은 범위는 [[../../05_ADR/0037-Texel-Grid-and-Demo-Lit-Effects|ADR 0037]]을 따른다.

## 최종 렌더링 검토 항목

| 방식 | 적용 단계 | 실제 Geometry 변화 |
|---|---|---|
| Height 기반 Shading / Parallax | Fragment Shader | 없음 |
| Geometry Displacement | Vertex / Tessellation / Mesh 단계 | 있음 |

적층의 두께 변화가 중요한 경우 Geometry Displacement 계열이 필요할 수 있다. 현재 표는 최종 렌더링의 검토 대상이다. 데모 Mud는 texel 연결면 변위를 사용하지만 최종 물리 layer의 방식은 미확정이다.

## 검증 항목

- `Wetness`, `Burn`, `Mud` 등 상태별 외관 변화가 목표 데모에서 충분히 표현되는지 확인한다.
- Mud·Snow 등 적층 상태의 높이 변화가 Shading / Parallax 방식으로 충분한지 확인한다.
- Geometry Displacement가 필요한 경우 지원 방식과 비용을 실험으로 비교한다.
- 성능 기준은 구현 후 대표 Mesh와 해상도에서 측정해 기록한다.

구현 및 실험 결과에 따라 선택한 경로가 확정되면 Architecture의 렌더링 요구사항은 유지하고, 확정 근거가 중요한 설계 선택은 ADR에 기록한다.
