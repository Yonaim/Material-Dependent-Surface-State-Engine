# 렌더링 구현 검토

> **한 줄 요약:** State 외관 변화와 Accumulation 형상 변화를 렌더링 경로에 연결할 구현 선택지를 기록하고 검증한다.

상태: **Lit 연결 방식 미확정 / 디버그 연결면 구현** · 관련 문서: [[04_Architecture/0009_Rendering|Surface State Rendering]], [[04_Architecture/0004_Surface-Geometry|적층과 Accumulation Height]]

Architecture 문서는 렌더링 결과의 의미와 요구사항을 정의한다. 이 문서는 해당 결과를 실제 렌더링 경로에 연결할 때 선택·검증할 구현 항목을 기록한다.

현재 높이 디버그는 compute에서 texel별 위치·normal을 계산하고 같은 UV chart 안에서 연결한 삼각형으로 표시한다 ([[../../05_ADR/0036-Texel-Geometry-Preview|ADR 0036]]). 독립 surfel 패치는 채택하지 않는다. 원본 메시 정점 밀도로 제한되던 초기 미리보기와 구분하며, Lit 재질 반응·물리적 다중 layer·Solver 형상 피드백의 방식 확정을 뜻하지 않는다.

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
