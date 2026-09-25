# Branch 3 — Shared Geometry Build

브랜치: `feat/shared-geometry-build`  
선행 조건: `feat/simulation-mapping` 병합  
관련 설계: [[03_Architecture/0005_Surface-Geometry|Surface Geometry]], [[05_Development/Notes/0001_Geometry-Preprocessing|Geometry Preprocessing]]

## 목표

Mapping의 TriangleID와 barycentric coordinate로 Solver가 읽을 정적 `SharedSurfaceGeometryData`를 만든다. 이 브랜치도 CPU 결과까지 완성하고 Vulkan upload는 다음 브랜치로 넘긴다.

또한 정적 Surface data를 매 Runtime의 Asset/Scene load에서 생성하는 경로를 연결한다. 이 브랜치는 Profile Distribution 입력을 `SurfaceProfileMap`으로 변환하고, 각 고유 Mesh/Profile Distribution 조합마다 CPU preprocessing을 수행해 Runtime 메모리 Asset으로 등록·공유하는 end-to-end 경로를 소유한다. `.Surface` persistent cache와 Save/Load는 사용하지 않는다.

## 4주차 구현 범위

반드시 구현:

- `ValidMask`
- `TexelSurfaceIndex`
- `SurfacePosition`
- `SurfaceNormal`
- `NeighborIndex[8]`
- 기본 `MesoVirtualHeight = 0`
- 기본 `ConcavityWeight = 0`

모든 Surface의 texel 해상도는 `512 × 512`로 고정한다. 해상도 상수는 한 곳에서 관리하고 UI·씬별 설정은 이번 구현에 넣지 않는다. Runtime Geometry data에는 실제 적용한 Surface별 width/height를 보유한다.

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

CPU 단계에서도 이웃 거리 배열을 보유하거나 Runtime Surface data에 저장하지 않는다. 거리와 전달 방향은 Solver가 `Position[j] - Position[i]`에서 필요할 때 계산한다. GPU는 ADR 기준에 따라 invalid `SurfaceID` sentinel을 사용한다.

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

## Runtime 전처리 및 수명

- 각 애플리케이션 Runtime에서 Asset/Scene load 중 고유 Mesh와 Profile Distribution 조합마다 한 번 Mapping과 Shared Geometry를 생성한다.
- 같은 입력 조합을 사용하는 여러 Mesh instance는 같은 Runtime 결과를 공유한다. 매 frame 또는 instance마다 다시 계산하지 않는다.
- 결과는 Runtime Asset/resource로 등록하고, Asset/Scene이 소유하는 참조를 통해 수명을 관리한다.
- 전처리 실패는 Asset/Scene load 실패로 전달하며, 입력 경로와 Surface/Triangle 진단 정보를 포함한다.
- `.Surface` binary serialization, cache path, content fingerprint, format version 및 stale/missing cache 분기는 구현하지 않는다.
- 이전 Branch 2에서 구현한 `.Surface` Save/Load, metadata/fingerprint API와 관련 cache 테스트를 제거하고 Runtime preprocessing API만 남긴다.

## Profile Distribution 입력 계약

- Mesh와 같은 stem의 `<MeshStem>.SurfaceProfileMap` JSON sidecar를 사용한다. 예: `DemoCube.obj` 옆의 `DemoCube.SurfaceProfileMap`.
- `profiles` 배열의 순서가 이 Mesh의 local Profile table 순서다. 경로는 sidecar 디렉토리 기준 상대 `.SRProfile` 경로로 기록한다.
- `surfaces` 배열은 각 dense `surfaceId`를 `profileIndex`에 연결한다. 한 Surface의 모든 valid texel은 같은 Profile을 사용한다.
- 로더가 JSON type/version, 자료형, 중복·누락 Surface ID, Profile 경로와 범위를 검증한다.
- valid texel의 Profile index 범위, invalid texel sentinel, 미등록 Profile 참조를 검증한다.
- Runtime build 입력으로 사용할 canonical Profile Distribution을 반환한다. Persistent cache용 hash는 만들지 않는다.
- 같은 입력 조합을 Runtime에서 중복 build하지 않도록 생성된 결과를 Asset/Scene 수명 동안 공유한다.

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
- Asset/Scene load path에서 Runtime Surface preprocessing 및 결과 등록 연결
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
- 매 Runtime에서 preprocessing이 수행되고, 같은 입력을 사용하는 instance가 생성된 결과를 공유한다.
- `.Surface` 파일을 읽거나 쓰지 않는 것을 확인한다.
