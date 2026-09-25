# ADR 0007 — 정적 Surface 전처리 에셋

- 상태: **Superseded by [[0008-Runtime-Surface-Preprocessing]]**
- 날짜: 2026-09-25

> 이 ADR은 정적 `.Surface` 바이너리 캐시를 선택했던 이전 결정을 기록한다. 현재 선택은 ADR 0008의 Runtime 전처리이며, 아래 내용은 결정 이력으로 보존한다.

## Context

Surface simulation은 Mesh UV에 대응하는 Texel 관계, Geometry 정보와 Texel별 Profile 배치를 필요로 한다. 이를 매 실행마다 다시 계산하면 로딩 비용이 발생하고, Render Material과 반응 Profile을 일대일로 묶으면 동일 Material 안의 위치별 반응 차이를 표현할 수 없다.

Mesh, Normal Map과 Profile 배치에서 생성되는 정적 데이터를 재사용할 수 있는 전처리 에셋으로 저장할 필요가 있다. 한편 시간에 따라 변하고 Surface instance마다 다른 State 및 Overflow는 공유 정적 에셋과 분리해야 한다.

## Decision

- 전처리 결과의 확장자는 `.Surface`로 표기한다.
- `.Surface`는 사용자가 직접 편집하는 파일이 아니라, Mesh 및 관련 입력에서 생성하는 정적 바이너리 에셋이자 전처리 캐시다.
- 전처리 입력은 Mesh, Normal Map, Profile Distribution이며, 출력에는 Shared Surface Geometry Data와 texel별 `SurfaceProfileMap`을 포함한다.
- Shared Surface Geometry Data는 유효성, Normal, Meso Virtual Height, Curvature/Concavity 등 정적 Geometry 값과 Neighbor index, Height Difference, Boundary, UV seam 연결 등 texel 관계 정보를 담는다. 이웃 Distance는 저장하지 않으며 Position 차이에서 계산한다.
- `SurfaceProfileMap`은 UV Texel을 `ProfileIndex`에 매핑한다. Render Material과 SRProfile은 별개이며, 동일 Render Material 영역 안에서도 texel별 Profile을 지정할 수 있다.
- State, Overflow, `inputFactor`, `transferRate`, `decayRate` 등의 Profile 반응 파라미터는 `.Surface`에 저장하지 않는다. 반응 파라미터는 `.SRProfile`, 동적 State/Overflow는 `TSurfaceInstanceStateData`가 소유한다.
- `.Surface` metadata에는 입력 변경과 캐시 유효성을 판별할 정보로 Mesh hash, Normal Map hash, Profile Map hash, grid resolution, UV set, preprocess version 등을 둔다.
- 캐시가 없거나 metadata가 현재 입력과 맞지 않으면 재생성한다. 개발 환경에서는 자동 전처리를 허용하고, 최종 Asset Build에서는 미리 생성하여 Runtime 로딩만 할 수 있도록 한다.
- Mesh별 `.Surface` 경로는 안정적인 Mesh Asset ID를 기준으로 하나만 둔다. ID가 준비되기 전에는 프로젝트 상대 Mesh 경로를 보존하고 확장자만 `.Surface`로 바꾼 경로를 cache root 아래에 사용한다. 파일명에는 해상도나 content hash를 넣지 않는다.
- 같은 Mesh의 입력 fingerprint가 바뀌면 같은 경로의 캐시를 재생성해 덮어쓴다. 해상도별 변형 파일은 보존하지 않으며, 이전 해상도가 다시 필요하면 다시 빌드한다.

| 데이터 | 소유 위치 | 수명 및 범위 |
|---|---|---|
| Geometry 및 Texel 관계 | `.Surface` | Mesh 입력에 종속된 정적·공유 데이터 |
| Texel → ProfileIndex | `.Surface` | UV Texel별 정적 Profile 배치 |
| State 반응 파라미터와 Transition | `.SRProfile` | Profile별 정적 설정 |
| State와 Overflow | `TSurfaceInstanceStateData` | 시간에 따라 변하는 instance별 데이터 |

`.Surface`는 version 2 binary cache로 직렬화하며, 필드는 명시적인 little-endian 정수·float encoding으로 기록한다. Version 2는 이웃 Distance를 더 이상 직렬화하지 않는다. Version 1 cache는 호환 로드하지 않고 stale로 처리해 재생성한다. 입력 fingerprint는 cache invalidation 목적의 64-bit FNV-1a이며 보안용 hash가 아니다. GPU 업로드 표현은 별도 설계에서 정한다.

## Alternatives Considered

### 1. 매번 Runtime에서 전처리

캐시 형식과 버전 관리를 피할 수 있고 입력 변경을 항상 반영한다. 하지만 매 실행마다 Mapping과 Geometry 계산 비용이 발생한다.

### 2. 정적 데이터를 Mesh와 별도 캐시 없이 Runtime 객체에서만 보유

초기 구현은 단순하다. 그러나 실행 간 결과 재사용과 Asset Build 단계의 전처리가 어렵고, 재생성 여부를 판단할 자산 계약도 없다.

### 3. `.Surface` 바이너리 전처리 캐시 — 선택

무거운 정적 결과를 재사용하고 Runtime 로딩과 전처리를 분리할 수 있다. 반면 파일 버전, 입력 fingerprint, 직렬화 검증과 cache invalidation을 구현해야 한다.

### 4. Render Material당 단일 SRProfile

연결 구조가 단순하다. 그러나 같은 Material 안의 부분 영역별 Profile 차이를 표현하지 못하므로 texel별 Profile map을 선택한다.

## Consequences

- 동일 Mesh 전처리 결과를 여러 Surface instance가 공유할 수 있다.
- 같은 Material 내부의 서로 다른 반응 Profile을 texel 단위로 지정할 수 있다.
- `.Surface`의 입력 fingerprint와 preprocess version 변경 시 캐시를 재생성해야 한다.
- cache path는 Mesh Asset을 기준으로 안정적으로 유지되며, stale/missing 결과는 해당 파일을 재생성해 대체한다.
- Profile Distribution의 authoring 형식 및 전처리 파이프라인 연결은 미완료다.
- Normal Map 기반 Meso Virtual Height 및 Curvature/Concavity 생성 알고리즘은 미완료다. Cache miss 자동 재생성 orchestration도 미구현이다.
- Profile Distribution 형식/loader 및 cache miss/stale의 Mapping→Build→Save 자동 연결은 `feat/shared-geometry-build`에 배정한다. Normal Map 기반 형상 복원은 Week-08 experiment에서 후보와 품질·비용을 검증한 뒤 별도 구현 branch를 결정한다.
- State/Overflow의 크기와 갱신은 `.Surface`의 정적 캐시 수명에 영향을 받지 않는다.

## Related

- [[0004-Asset-Profile-Mapping]]
- [[../03_Architecture/0003_Assets-and-Profiles|에셋과 프로필]]
- [[../05_Development/Notes/0000_Surface-Simulation-Mapping|Surface Simulation Mapping]]
- [[../05_Development/Notes/0003_Surface-State-GPU-Resource|Surface State GPU Resource]]
