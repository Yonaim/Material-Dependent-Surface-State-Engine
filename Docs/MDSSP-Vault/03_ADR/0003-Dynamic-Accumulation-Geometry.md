# ADR 0003 — Accumulation Height의 동적 형상 반영

- 상태: **Accepted**

## Context

Mud·Snow·SurfaceWater처럼 적층이 생기면 보이는 형상뿐 아니라 다음 이동 경로도 바뀐다. 원래 형상만 계속 사용하면 홈이 이미 메워졌는데도 State가 계속 같은 홈으로 이동하는 문제가 생긴다.

## Decision

- 적층을 `Cavity Filling + Surface Following`으로 나눈다.
- `AccumulationHeight`를 계산한다.
- 적층으로 변한 Height를 기반으로 Normal / Distance / Curvature를 후속 Simulation에 다시 반영한다.
- Rendering의 최종 높이에도 Meso Virtual Height와 함께 반영한다.

## Consequences

Instance마다 State가 다르므로 동적 형상 데이터의 실제 GPU 소유·저장 방식이 필요하다. 이 부분은 후속 GPU Resource 설계에서 정한다. [[02_Architecture/0008_Accumulation|적층]], [[02_Architecture/0007_Surface-Geometry|형상 정보]].
