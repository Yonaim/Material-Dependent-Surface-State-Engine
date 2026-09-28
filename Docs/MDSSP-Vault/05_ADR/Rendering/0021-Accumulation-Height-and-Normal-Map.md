# ADR 0021 — Accumulation Height와 Normal Map 렌더링

> **한 줄 요약:** Normal Map에서 복원한 Meso 형상과 동적 적층 높이를 한 번씩만 최종 표면 방향에 반영한다.

- Status: **Proposed**
- Date: 2026-09-28
- Related: [[../Simulation/0003-Dynamic-Accumulation-Geometry|ADR 0003]], [[../Simulation/0018-Normal-Map-Meso-Geometry|ADR 0018]], [[../../04_Architecture/0009_Rendering|Surface State Rendering]], [[../../06_Development/Notes/0004_Rendering-Implementation|Rendering 구현 검토]]

## Context

Normal Map에서 복원한 `MesoVirtualHeight`와 그 높이장에서 얻은 `MesoNormal`은 이미 원본 Normal Map의 표면 변화를 나타낸다. 동적 `AccumulationHeight`는 여기에 적층으로 생긴 높이를 더한다. 원본 Normal Map을 복원 Meso normal 위에 그대로 다시 적용하면 같은 세부 방향을 중복 반영할 수 있다.

현재 Architecture는 최종 높이를 `MesoVirtualHeight + AccumulationHeight`로 정의하지만, 일반 렌더링 경로에서 적층 형상과 그 normal을 어떻게 적용할지는 미정이다. 검토 중인 구현은 높이 기반 shading/parallax와 실제 Geometry Displacement다.

## Decision

1. 최종 Meso 높이는 `FinalMesoHeight = MesoVirtualHeight + AccumulationHeight`로 구성하고, 두 항은 같은 표면 기준과 길이 단위를 사용한다.
2. 최종 조명 normal은 이 최종 표면 방향을 한 번 반영한다. `MesoVirtualHeight`와 `MesoNormal`에 이미 포함된 원본 Normal Map 변화를 같은 주파수 대역에서 다시 더하지 않는다.
3. Geometry Displacement 경로에서는 `FinalMesoHeight`로 형상을 변위하고, 변위된 형상에서 normal을 구한다. 이후 원본 Normal Map은 Meso 복원에 포함되지 않은 잔여 미세 세부가 있을 때만 적용한다.
4. Height 기반 shading/parallax 경로에서는 `FinalMesoHeight`의 변화가 조명 방향에 반영되도록 한다. 원본 Normal Map을 추가로 사용할 경우 Meso 복원에 포함된 대역을 분리해 중복되지 않는 세부만 적용한다.
5. 어떤 경로를 최종 구현으로 채택할지는 대표 Mesh에서 실루엣 요구와 비용을 비교한 뒤 확정한다. 이 ADR은 높이와 normal의 결합 규칙을 제안하며 렌더링 경로의 구현 완료를 뜻하지 않는다.

## Alternatives Considered

| 대안 | 장점 | 한계 |
|---|---|---|
| Height 기반 shading/parallax | 메쉬 정점 밀도에 덜 의존하고 화면상 표면 방향 변화를 줄 수 있다. | 실루엣과 실제 가림을 바꾸지 않는다. |
| Geometry Displacement | 표면 위치와 실루엣을 바꿀 수 있다. | 충분한 정점 밀도 또는 tessellation이 필요하고 비용 검증이 필요하다. |
| 원본 Normal Map을 MesoNormal 위에 그대로 재적용 | 기존 material normal 경로를 재사용하기 쉽다. | MesoVirtualHeight가 이미 같은 Normal Map에서 복원된 세부를 포함하므로 그 대역이 중복 적용될 수 있다. |

## Consequences

- 구현은 `MesoVirtualHeight`, `AccumulationHeight`, 원본 Normal Map의 책임과 적용 순서를 분리해야 한다.
- 같은 재질 Normal Map을 복원 높이와 잔여 미세 세부로 나눌 경우 필터 범위와 세부 대역의 정의가 필요하다. 대역 분리는 이 ADR에서 수치로 정하지 않는다.
- 렌더링 선택을 확정할 때 평탄면, ramp, bowl, 적층 경계와 UV seam 사례에서 높이 연속성, normal 중복, 실루엣, 성능을 비교하고 ADR 상태를 갱신한다.

## Related

- [[../Simulation/0003-Dynamic-Accumulation-Geometry|ADR 0003 — Accumulation Height의 동적 형상 반영]]
- [[../Simulation/0018-Normal-Map-Meso-Geometry|ADR 0018 — Normal Map 기반 Meso Geometry 복원]]
- [[../../04_Architecture/0009_Rendering|Surface State Rendering]]
- [[../../04_Architecture/0004_Surface-Geometry|형상 정보와 적층]]
- [[../../06_Development/Notes/0004_Rendering-Implementation|Rendering 구현 검토]]
