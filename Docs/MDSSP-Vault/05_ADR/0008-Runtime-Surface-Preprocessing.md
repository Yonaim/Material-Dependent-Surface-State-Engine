# ADR 0008 — Runtime Surface 전처리

> **한 줄 요약:** 실행 중 전처리와 메모리 공유만 유지했던 중간 결정이며, 현재는 해상도별 `.Surface` 캐시를 사용한다.

- 분류: **Assets**
- Status: **Superseded by [[0026-Resolution-Surface-Cache|ADR 0026]]**
- Date: 2026-09-25
- 관련 문서: [[0007-Surface-Preprocessed-Asset|ADR 0007 — 정적 Surface 전처리 에셋]], [[0026-Resolution-Surface-Cache|ADR 0026 — 해상도별 Surface 캐시]]

## Context

ADR 0007의 단일 `.Surface` 캐시를 유지하면 직렬화 형식, 입력 fingerprint와 stale 판정을 관리해야 했다. 정적 결과를 실행 중 생성하고 같은 입력을 쓰는 instance끼리 메모리에서 공유하는 단순한 경로를 검토했다.

## Decision

중간 결정으로 persistent `.Surface` 파일을 없애고, Asset/Scene 로딩 중 고유 Mesh와 Profile Distribution 조합을 전처리해 Runtime 메모리에서 공유했다. Instance별 State는 별도 데이터로 유지했다. 이 ADR은 현재 cache 저장·로드 계약을 정의하지 않는다.

## Alternatives Considered

- 실행 간 재사용을 위해 `.Surface` 바이너리 캐시를 유지한다. 초기 ADR 0007과 이후 ADR 0026의 선택이다.
- Runtime 전처리와 메모리 공유만 사용한다. 당시 파일 형식 및 무효화 관리를 줄이기 위해 채택했으나, 매 실행 전처리 비용이 남았다.

## Consequences

ADR 0026은 해상도별 `.Surface` 캐시를 다시 도입해 유효 결과를 실행 간 재사용하고, cache miss·stale·손상 시 Runtime 전처리로 대체한다. 현재 계약은 [[0026-Resolution-Surface-Cache|ADR 0026]] 및 [[../04_Architecture/0004_Surface-Geometry|Surface Geometry]]를 기준으로 한다.

## Related

- [[0007-Surface-Preprocessed-Asset|ADR 0007 — 정적 Surface 전처리 에셋]]
- [[0026-Resolution-Surface-Cache|ADR 0026 — 해상도별 Surface 캐시]]
- [[../04_Architecture/0003_Assets-and-Profiles|에셋과 프로필]]
