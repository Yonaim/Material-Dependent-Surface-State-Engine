# 렌더링 구현 검토

상태: **구현 방식 미확정 / 실험 필요** · 관련 문서: [[03_Architecture/0006_Rendering|Surface State Rendering]], [[03_Architecture/0005_Surface-Geometry|적층과 Accumulation Height]]

Architecture 문서는 렌더링 결과의 의미와 요구사항을 정의한다. 이 문서는 해당 결과를 실제 렌더링 경로에 연결할 때 선택·검증할 구현 항목을 기록한다.

## 검토 중인 적용 방식

| 방식 | 적용 단계 | 실제 Geometry 변화 |
|---|---|---|
| Height 기반 Shading / Parallax | Fragment Shader | 없음 |
| Geometry Displacement | Vertex / Tessellation / Mesh 단계 | 있음 |

적층의 두께 변화가 중요한 경우 Geometry Displacement 계열이 필요할 수 있다. 현재 표는 검토 대상이며 확정된 구현 선택을 뜻하지 않는다.

## 검증 항목

- `Wetness`, `Burn`, `Mud` 등 상태별 외관 변화가 목표 데모에서 충분히 표현되는지 확인한다.
- Mud·Snow 등 적층 상태의 높이 변화가 Shading / Parallax 방식으로 충분한지 확인한다.
- Geometry Displacement가 필요한 경우 지원 방식과 비용을 실험으로 비교한다.
- 성능 기준은 구현 후 대표 Mesh와 해상도에서 측정해 기록한다.

구현 및 실험 결과에 따라 선택한 경로가 확정되면 Architecture의 렌더링 요구사항은 유지하고, 확정 근거가 중요한 설계 선택은 ADR에 기록한다.
