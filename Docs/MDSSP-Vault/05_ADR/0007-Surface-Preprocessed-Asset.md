# ADR 0007 — 정적 Surface 전처리 에셋

> **한 줄 요약:** Mesh당 단일 `.Surface` 캐시를 택했던 초기 결정이며, 현재 캐시 계약은 ADR 0026을 따른다.

- 분류: **Assets**
- Status: **Superseded — ADR 0008을 거쳐 ADR 0026으로 대체**
- Date: 2026-09-25
- 관련 문서: [[0008-Runtime-Surface-Preprocessing|ADR 0008 — Runtime Surface 전처리]], [[0026-Resolution-Surface-Cache|ADR 0026 — 해상도별 Surface 캐시]]

## Context

Mesh와 관련 입력에서 생성하는 정적 Simulation Geometry와 texel별 Profile 배치를 실행 간 재사용할지 결정해야 했다.

## Decision

초기 결정은 공유 Geometry와 Profile map을 Mesh별 단일 `.Surface` 바이너리 파일에 저장하는 것이었다. State와 Profile 반응 파라미터는 캐시에서 분리하고, 입력 fingerprint가 달라지면 같은 경로의 파일을 다시 생성하도록 했다. 이 결정의 세부 직렬화 형식과 단일 경로 정책은 더 이상 현재 구현 기준이 아니다.

## Alternatives Considered

- Runtime에서 매번 전처리하고 메모리에서만 공유한다. ADR 0008에서 이 방식을 채택했다.
- 해상도별 캐시를 사용한다. 전처리 비용을 줄이기 위해 ADR 0026에서 다시 채택했다.

## Consequences

단일 파일 캐시는 ADR 0008의 Runtime 전처리 전용 정책으로 대체됐고, 이후 ADR 0026에서 해상도별 `.Surface` 캐시로 재도입됐다. 현재 경로, fingerprint, 파일 형식과 검증 규칙은 [[0026-Resolution-Surface-Cache|ADR 0026]] 및 [[../04_Architecture/0004_Surface-Geometry|Surface Geometry]]를 기준으로 한다.

## Related

- [[0008-Runtime-Surface-Preprocessing|ADR 0008 — Runtime Surface 전처리]]
- [[0026-Resolution-Surface-Cache|ADR 0026 — 해상도별 Surface 캐시]]
- [[../04_Architecture/0003_Assets-and-Profiles|에셋과 프로필]]
