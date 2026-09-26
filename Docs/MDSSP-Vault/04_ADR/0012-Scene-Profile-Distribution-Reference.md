# ADR 0012 — Scene별 Surface Profile Map 참조

- 상태: **Accepted**
- 날짜: 2026-09-26
- 관련 결정: [[0008-Runtime-Surface-Preprocessing]], [[0009-Texel-Profile-Index-Map]]

## Context

`.SurfaceProfileMap`은 Mesh Surface에 SRProfile을 할당하는 authoring 입력이다. 이를 OBJ 옆의 고정된 경로에서 자동 탐색하면 하나의 Mesh가 사용하는 Profile 배치가 Mesh에 묶인다. 같은 OBJ를 여러 Scene에서 사용하면서 Scene별로 다른 Profile 배치를 선택하려면 Scene이 어떤 매핑 파일을 쓸지 결정해야 한다.

Runtime 전처리 결과에는 Geometry와 texel별 Profile map이 함께 들어간다. 그러므로 서로 다른 Profile Map을 선택한 Scene instance는 다른 Runtime Surface Data 조합을 사용해야 한다. 같은 Mesh와 같은 Map 경로를 선택한 instance는 같은 결과를 공유할 수 있다.

## Decision

- `.Scene`의 각 Mesh object는 `mesh` 경로와 선택적 `surfaceProfileMap` 경로를 가진다.
- 두 경로는 `.Scene` 파일이 있는 디렉터리를 기준으로 해석하며 상대 경로만 허용한다. `mesh`는 `.obj`, `surfaceProfileMap`은 `.SurfaceProfileMap` 확장자를 사용한다.
- `surfaceProfileMap`이 없으면 해당 object는 렌더링 Mesh만 로드하고 Surface simulation data는 생성하지 않는다.
- `surfaceProfileMap`이 지정되면 Scene loader가 해당 Mesh와 map으로 Runtime Surface Data를 로드/생성해 instance에 연결한다. map 안의 `.SRProfile` 경로는 기존 계약대로 map 파일 디렉터리 기준 상대 경로다.
- AssetManager는 정규화한 `(Mesh 경로, SurfaceProfileMap 경로)` 조합별로 Runtime Surface Data와 Profile table을 캐시한다. 서로 다른 map은 동일 Mesh라도 서로 다른 Runtime handle을 받는다.
- Scene instance의 동적 State는 instance별로 유지하고, 같은 Runtime Surface Data 조합을 가진 instance끼리는 정적 GPU Geometry와 Profile buffer를 공유한다.
- OBJ 로더는 `.SurfaceProfileMap`을 이름 규칙으로 자동 검색하지 않는다. Profile Distribution 선택은 `.Scene`이 소유한다.

## `.Scene` 예시

```json
{
  "type": "TScene",
  "version": 1,
  "objects": [
    {
      "mesh": "../Meshes/Rock.obj",
      "surfaceProfileMap": "../SurfaceProfiles/Rock_Wet.SurfaceProfileMap",
      "transform": {
        "position": [0.0, 0.0, 0.0],
        "rotationDegrees": [0.0, 30.0, 0.0],
        "scale": [1.0, 1.0, 1.0]
      }
    }
  ]
}
```

서로 다른 Scene은 같은 OBJ에 다른 map 경로를 지정할 수 있다. `.SurfaceProfileMap` 내부의 SRProfile 경로 해석과 Surface ID/Profile index 형식은 별도로 유지한다.

Scene이 지정하는 `.SurfaceProfileMap`은 기존 v1 형식을 그대로 사용한다. Profile 경로는 map 파일 기준 상대 경로이고, 각 Surface는 profiles 배열의 항목을 선택한다.

```json
{
  "type": "SurfaceProfileMap",
  "version": 1,
  "profiles": ["../Profiles/stone.SRProfile", "../Profiles/moss.SRProfile"],
  "surfaces": [
    { "surfaceId": 0, "profileIndex": 0 },
    { "surfaceId": 1, "profileIndex": 1 }
  ]
}
```

## Alternatives Considered

### 1. OBJ 옆의 고정 sidecar를 자동 탐색

OBJ와 매핑 입력을 한 쌍처럼 배포할 수 있고 Scene 형식이 간단하다. 하지만 Mesh마다 사실상 하나의 Profile 배치만 선택하게 되어, 같은 Mesh를 쓰는 Scene별 변형이 어렵다. 이 방식은 선택하지 않는다.

### 2. `.Scene` 안에 Surface/Profile 매핑 전체를 내장

Scene 단위 설정이 명확하고 별도 map 파일을 참조하지 않는다. 다만 기존 `.SurfaceProfileMap` 입력 형식과 중복되며, 재사용하거나 별도 편집하기 어렵다. Scene에는 매핑 데이터 자체 대신 사용할 map 경로를 둔다.

### 3. `.Scene`에서 `.SurfaceProfileMap` 경로 참조 — 선택

Scene별 선택권을 제공하면서 Profile Distribution 입력을 별도 파일로 재사용할 수 있다. 경로 해석 및 Mesh/Map 조합별 Runtime 캐시가 필요하지만, Scene 로딩 시 결정해 런타임 조회에는 경로 탐색 비용을 추가하지 않는다.

## Consequences

- Scene loader는 Mesh, 선택된 Profile Map, transform을 검증하고 각 Asset을 연결한다.
- AssetManager와 GPU resource manager는 Mesh handle 단독이 아니라 Runtime Surface Data handle을 기준으로 Profile map/Geometry 자원을 공유한다.
- 같은 Mesh를 서로 다른 map으로 쓰면 정적 Surface 결과가 별도로 생성될 수 있다. 같은 Mesh와 map 조합은 Runtime 메모리에서 공유한다.
- `.Scene`에 map이 빠진 object는 표면 solver resource를 받지 않는다.
- Scene에 명시된 파일이 없거나 형식이 잘못된 경우 로딩은 오류로 실패한다. 자동 sidecar fallback은 하지 않는다.

## Related

- [[03_Architecture/0003_Assets-and-Profiles|Assets and Profiles]]
- [[04_ADR/0008-Runtime-Surface-Preprocessing|ADR 0008 — Runtime Surface 전처리]]
- [[04_ADR/0009-Texel-Profile-Index-Map|ADR 0009 — Texel별 Profile Index Map]]
- [[05_Development/Code-Structure/0001_Asset-and-Surface-Data-Flow|Asset과 Surface 데이터 흐름]]
