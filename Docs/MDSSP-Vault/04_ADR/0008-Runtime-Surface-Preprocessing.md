# ADR 0008 — Runtime Surface 전처리

- 상태: **Accepted**
- 날짜: 2026-09-25
- 대체: [[0007-Surface-Preprocessed-Asset]]

## Context

Surface simulation에는 Mesh의 Simulation UV를 texel graph로 변환한 Mapping, texel 위치·Normal·이웃 관계와 texel별 Profile 배치가 필요하다. 이전 결정은 이 정적 결과를 `.Surface` 바이너리 캐시에 저장하고 입력 fingerprint가 바뀌면 재생성하는 방식이었다.

현재는 별도 캐시 포맷, 버전 호환, fingerprint 및 stale 판정을 유지하기보다 애플리케이션 Runtime에서 입력으로부터 결과를 생성하는 방식으로 단순화한다. 다만 같은 Mesh와 Profile Distribution 조합을 쓰는 여러 instance가 있다면 전처리 결과를 Runtime 메모리에서 공유해 중복 계산하지 않는다.

## Decision

- `.Surface` 파일을 생성하거나 읽지 않는다. Persistent binary cache, cache path, input fingerprint와 cache invalidation을 사용하지 않는다.
- Asset/Scene 로딩 중 각 고유 Mesh와 Profile Distribution 조합에 대해 CPU 전처리를 수행한다. Scene object가 Profile Distribution을 선택하며, 결과는 해당 Runtime 세션 동안 메모리에서 유지한다.
- 전처리 입력은 Mesh topology/Simulation UV, 필요한 Normal Map 데이터, Profile Distribution이다. 출력은 texel Mapping 및 `TSharedSurfaceGeometryData`와 texel별 Profile 연결 데이터다.
- 같은 입력을 사용하는 Mesh instance들은 하나의 Runtime 전처리 결과를 공유한다. 서로 다른 Profile Distribution을 쓰는 instance는 별도의 Runtime 조합을 사용한다. Instance별 State는 별도의 `TSurfaceInstanceStateData`에 둔다. State는 `stateCapacity`를 넘지 않으며 초과량은 저장하지 않는다. Solver 임시값은 `TempState`이며 초과량 저장소가 아니다.
- 전처리는 Asset/Scene load 시점에만 실행한다. 매 frame 또는 instance마다 반복하지 않는다. 입력 Asset이 Runtime 중 교체되면 해당 조합의 결과를 다시 만든다.
- Profile Distribution은 원본 authoring 입력 `.SurfaceProfileMap`으로 유지하며, `.Scene` object가 사용할 map 경로를 지정한다. 전처리 결과는 `.Surface` 파일로 저장하지 않는다. Scene 경로 참조와 Runtime cache key는 [[0012-Scene-Profile-Distribution-Reference|ADR 0012]]를 따른다.
- 전처리 결과에 대한 disk serialization 및 사전 Asset Build 단계는 이번 설계에서 다루지 않는다.

| 데이터 | 소유 위치 | 수명 및 범위 |
|---|---|---|
| Mapping 및 texel 관계 | Runtime `TSurfaceMappingData` | 고유 Mesh의 전처리/geometry build 동안 또는 필요 기간 |
| Geometry와 Texel → Profile 연결 | Runtime `TSharedSurfaceGeometryData` | 같은 입력을 사용하는 instance 간 공유 |
| State 반응 파라미터와 Transition | `.SRProfile` | Profile별 설정 |
| State | `TSurfaceInstanceStateData` | 시간에 따라 변하는 instance별 데이터. `stateCapacity`로 제한 |

## Alternatives Considered

### 1. `.Surface` 바이너리 캐시 유지

실행 간 정적 결과를 재사용해 시작 시 전처리 시간을 줄일 수 있다. 대신 직렬화 포맷, 버전, 입력 fingerprint, stale 처리 및 cache 경로를 유지하고 검증해야 한다.

### 2. Asset Build 단계에서 전처리하고 Runtime은 결과만 로드

Runtime 작업량을 줄일 수 있다. 반면 별도 빌드 도구와 산출물 전달 절차가 필요하고, Asset 또는 전처리 설정 변경 시 build pipeline을 다시 실행해야 한다.

### 3. Runtime에서 전처리하고 메모리에서 결과 공유 — 선택

파일 포맷과 cache invalidation을 없애 구현·디버깅 경로를 단순화한다. Runtime 시작 또는 Asset load 때 CPU 비용이 발생하므로 이후 실제 로딩 시간 측정에 따라 캐시를 다시 검토할 수 있다.

## Consequences

- 모든 실행에서 전처리 비용이 발생하지만, 같은 입력을 쓰는 instance끼리는 결과를 공유하여 Mesh별 한 번만 계산한다.
- `.Surface` binary Save/Load, metadata/fingerprint 자료형과 cache round-trip 테스트를 코드에서 제거한다. Runtime preprocessing API만 남긴다.
- Missing/stale cache 경로는 없어지고, 전처리 입력 오류는 Asset/Scene load 실패로 보고한다.
- Normal Map 기반 Meso Virtual Height 및 Curvature/Concavity 생성 알고리즘은 별도 설계 대상이며, Runtime 실행 시점만 정한 이 ADR이 해당 알고리즘을 확정하지는 않는다.
- 전처리 비용이 허용되지 않을 정도로 커지는지 실제 Asset 집합에서 측정한다. 그 결과가 나오기 전에는 persistent cache를 다시 도입하지 않는다.

## Related

- [[0006-Dynamic-State-Registry]]
- [[0012-Scene-Profile-Distribution-Reference]]
- [[../03_Architecture/0001_Engine-Structure|엔진 구조와 데이터 흐름]]
- [[../03_Architecture/0003_Assets-and-Profiles|에셋과 프로필]]
- [[../05_Development/Notes/0000_Surface-Simulation-Mapping|Surface Simulation Mapping]]
- [[../05_Development/Code-Structure/0001_Asset-and-Surface-Data-Flow|Asset과 Surface 데이터 흐름]]
