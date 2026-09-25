# Branch 3 — Shared Geometry Build

브랜치: `feat/shared-geometry-build`  
선행 조건: `feat/simulation-mapping` 병합  
관련 설계: [[03_Architecture/0005_Surface-Geometry|Surface Geometry]], [[05_Development/Notes/0001_Geometry-Preprocessing|Geometry Preprocessing]]

## 목표

Mapping의 TriangleID와 barycentric coordinate로 Solver가 읽을 정적 `SharedSurfaceGeometryData`를 만든다. 이 브랜치도 CPU 결과까지 완성하고 Vulkan upload는 다음 브랜치로 넘긴다.

또한 Branch 2에서 준비된 `.Surface` cache API를 Asset/Scene 생성 흐름에 연결한다. 이 브랜치는 Profile Distribution 입력을 `SurfaceProfileMap`으로 변환하고 cache miss/stale 시 필요한 CPU preprocessing을 수행해 versioned `.Surface`를 저장하는 end-to-end 경로를 소유한다.

## 4주차 구현 범위

반드시 구현:

- `ValidMask`
- `TexelSurfaceIndex`
- `SurfacePosition`
- `SurfaceNormal`
- `NeighborIndex[8]`
- 기본 `MesoVirtualHeight = 0`
- 기본 `ConcavityWeight = 0`

모든 Surface의 texel 해상도는 `512 × 512`로 고정한다. 해상도 상수는 한 곳에서 관리하고 UI·씬별 설정은 이번 구현에 넣지 않는다. `.Surface` header에는 실제 적용한 Surface별 width/height를 기록한다.

후속으로 미룸:

- Normal Map 적분
- non-integrable fallback
- 정밀 Macro Curvature
- geodesic distance
- Accumulation에 따른 동적 갱신

이렇게 하면 Solver 구조를 먼저 검증하면서도 나중에 Meso/동적 geometry 필드를 교체할 수 있다.

## CPU 자료구조

```cpp
struct SharedSurfaceGeometryCPU
{
    std::uint32_t Width = 0;
    std::uint32_t Height = 0;

    std::vector<std::uint32_t> ValidMask;
    std::vector<SurfaceLocalID> TexelSurfaceIndices;
    std::vector<glm::vec3> Positions;
    std::vector<glm::vec3> Normals;
    std::vector<float> MesoVirtualHeights;
    std::vector<float> ConcavityWeights;
    std::vector<std::array<LocalTexelIndex, 8>> NeighborIndices;
};
```

모든 vector 길이는 `width × height`로 동일해야 한다. invalid texel도 배열 자리를 유지해 index 계산을 단순하게 한다.

## Position과 Normal 복원

mapping texel의 triangle vertex를 `P0..P2`, `N0..N2`, barycentric을 `b0..b2`라 한다.

```text
Position = b0*P0 + b1*P1 + b2*P2
Normal   = normalize(b0*N0 + b1*N1 + b2*N2)
```

- `b2 = 1-b0-b1`
- Normal 길이가 epsilon 이하면 triangle face normal을 fallback으로 사용
- Position과 Normal은 Mesh local space
- instance transform은 이 데이터에 bake하지 않음

CPU 단계에서도 이웃 거리 배열을 보유하거나 `.Surface` payload에 직렬화하지 않는다. 거리와 전달 방향은 Solver가 `Position[j] - Position[i]`에서 필요할 때 계산한다. GPU는 ADR 기준에 따라 invalid `SurfaceID` sentinel을 사용한다.

## Distance

4주차 기준:

```text
Distance(i, j) = max(length(Position[j] - Position[i]), DistanceEpsilon)
```

이 식은 저장 데이터 정의가 아니라 Solver에서 이웃을 처리할 때 계산하는 값이다. CPU 검증 코드가 거리를 확인해야 하면 두 Position에서 임시로 계산하며 배열로 보관하지 않는다.

여기서 `j = NeighborIndex[i][k]`다.

- invalid neighbor의 Distance는 0
- 양방향 edge의 Distance는 epsilon 안에서 동일해야 함
- instance scale은 runtime에서 고려하거나 non-uniform scale을 제한해야 함

초기 데모에서 non-uniform scale을 허용하지 않는 편이 안전하다. 허용하려면 world-space distance를 instance별로 다시 계산해야 해 Shared 데이터 의미가 달라진다.

## Surface와 Profile 경계

Geometry에는 Profile index를 직접 저장하지 않는다.

- `TexelSurfaceIndex`: 공유 가능
- `Surface→ProfileIndex`: instance/scene 연결이므로 별도 데이터

서로 다른 Surface의 texel이 topology상 이웃일 수 있다. Geometry는 연결을 유지하고 Solver가 `ProfileBoundaryWeight`를 적용한다.

## Cache

`.Surface` cache API는 선행 Branch 2에서 구현되었다. Distance 직렬화를 제거하면서 format version을 2로 올렸고, version 1 cache는 stale/unsupported로 보고 재생성한다. 이 브랜치에서는 해당 API를 호출자 흐름에 연결한다.

```text
CacheFingerprint
= mesh content hash
+ normal map content hash
+ simulation UV set/hash
+ canonical texel Profile map hash
+ fixed state resolution (512 × 512 per Surface)
+ cache format and preprocessing version
```

`.Surface` 경로는 해상도나 content hash가 아닌 Mesh 자산의 안정적인 asset ID를 기준으로 정한다. Asset ID가 없는 현재 경로 기반 구현에서는 프로젝트 내 Mesh 상대 경로를 보존하고 확장자만 `.Surface`로 바꾼 경로를 cache root 아래에 사용한다. 예: `Assets/Meshes/Crate.obj` → `Cache/Surface/Assets/Meshes/Crate.Surface`. 이 규칙은 같은 파일명 Mesh의 경로 충돌을 방지한다.

Mesh 경로가 같은 입력 조합에서 해상도 또는 다른 fingerprint 항목이 달라지면 기존 `.Surface`를 stale로 판정하고 다시 빌드해 같은 경로에 덮어쓴다. 서로 다른 해상도별 cache 파일을 보존하지 않는다. 따라서 과거 설정으로 돌아갈 경우 다시 빌드한다. Scene/AssetManager는 cache load와 metadata 검사를 먼저 수행하고, missing/stale이면 Mapping 및 Shared Geometry/`SurfaceProfileMap`을 build한 뒤 이 경로에 저장하고 Asset으로 등록한다. 기존 binary 형식을 다시 만들거나 version 없는 dump를 추가하지 않는다.

## Profile Distribution 입력 계약

- Mesh와 같은 stem의 `<MeshStem>.SurfaceProfileMap` JSON sidecar를 사용한다. 예: `DemoCube.obj` 옆의 `DemoCube.SurfaceProfileMap`.
- `profiles` 배열의 순서가 이 Mesh의 local Profile table 순서다. 경로는 sidecar 디렉토리 기준 상대 `.SRProfile` 경로로 기록한다.
- `surfaces` 배열은 각 dense `surfaceId`를 `profileIndex`에 연결한다. 한 Surface의 모든 valid texel은 같은 Profile을 사용한다.
- 로더가 JSON type/version, 자료형, 중복·누락 Surface ID, Profile 경로와 범위를 검증한다.
- valid texel의 Profile index 범위, invalid texel sentinel, 미등록 Profile 참조를 검증한다.
- `.Surface` cache fingerprint에 반영되는 canonical Profile Map hash를 생성한다.
- Cache miss/stale 시 `.Surface`를 같은 stable path에 재생성해 덮어쓴다. 이전 해상도 변형 파일은 보존하지 않는다.

```json
{
  "type": "SurfaceProfileMap",
  "version": 1,
  "profiles": ["../SurfaceProfiles/Fabric.SRProfile"],
  "surfaces": [
    {"surfaceId": 0, "profileIndex": 0}
  ]
}
```

## 구현 대상

- `Source/SurfaceStateSystem/Geometry/SharedSurfaceGeometryData.h/.cpp`
- `Source/SurfaceStateSystem/Geometry/SurfaceGeometryBuilder.h/.cpp`
- 필요 시 `Source/AssetManager/Assets/MeshAsset.h/.cpp`
- `Source/AssetManager`의 Surface/Profile Distribution asset loading 또는 preprocessing orchestration
- 필요한 경우 Scene load path의 `.Surface` cache 연결
- `Tests/SharedSurfaceGeometryTests.cpp`

## 작업 순서

1. 모든 출력 배열 크기와 기본값 초기화
2. valid texel의 Position 복원
3. Normal 복원과 fallback
4. Mapping neighbor 복사
5. Distance 계산식의 Position 기반 결과 검증 (영구 배열은 만들지 않음)
6. Surface ID 복사
7. 전체 validation
8. Debug summary 로그

## Validation

- 모든 배열 크기가 texel count와 동일
- valid texel의 Position/Normal이 finite
- Normal 길이가 1에 가까움
- invalid texel은 기본값 유지
- neighbor index가 유효 범위 또는 invalid sentinel 중 하나
- 검증 시 즉석 계산한 이웃 거리가 양수이고 finite
- 양방향 edge가 같은 두 Position을 참조
- Surface ID가 SurfaceRange 범위 안

## 테스트

- 단일 triangle의 barycentric Position이 예상값과 일치
- 평면 quad의 모든 Normal이 동일
- 대각선 neighbor distance가 축 neighbor보다 큼
- UV seam edge에서도 거리 연속성 유지
- instance 두 개가 같은 CPU Shared Geometry 객체를 참조할 수 있음

## 권장 커밋 분할

1. `Feat: Texel별 Surface Geometry 복원`
2. `Feat: 이웃 거리의 on-demand 계산 연결`
3. `Test: Shared Surface Geometry 필드 검증`

## 완료 조건

- Mapping fixture에서 모든 필수 Geometry field가 생성된다.
- CPU validation/test가 통과한다.
- GPU upload에 필요한 배열과 명확한 크기가 준비된다.
- Normal Map이 없어도 Solver를 실행할 수 있는 기본값이 존재한다.
- Profile Distribution을 파싱해 유효한 texel Profile map을 만들 수 있다.
- cache hit는 기존 `.Surface`를 재사용하고, missing/stale는 build-save 후 재사용 가능한 Asset으로 등록된다.
