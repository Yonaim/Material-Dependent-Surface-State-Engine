# Branch 3 — Shared Geometry Build

브랜치: `feat/shared-geometry-build`  
선행 조건: `feat/simulation-mapping` 병합  
관련 설계: [[03_Architecture/0005_Surface-Geometry|Surface Geometry]], [[05_Development/Notes/0001_Geometry-Preprocessing|Geometry Preprocessing]]

## 목표

Mapping의 TriangleID와 barycentric coordinate로 Solver가 읽을 정적 `SharedSurfaceGeometryData`를 만든다. 이 브랜치도 CPU 결과까지 완성하고 Vulkan upload는 다음 브랜치로 넘긴다.

## 4주차 구현 범위

반드시 구현:

- `ValidMask`
- `TexelSurfaceIndex`
- `SurfacePosition`
- `SurfaceNormal`
- `NeighborIndex[8]`
- `Distance[8]`
- 기본 `MesoVirtualHeight = 0`
- 기본 `ConcavityWeight = 0`

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
    std::vector<std::array<float, 8>> NeighborDistances;
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

CPU 단계에서는 mapping/debug를 위해 `ValidMask`와 `NeighborDistances`를 유지해도 된다. GPU upload는 ADR 기준으로 invalid `SurfaceID` sentinel을 사용하고 이웃 거리는 Position 차이에서 계산하므로, 이 CPU 필드들을 별도 GPU buffer로 복사하지 않는다.

## Distance

4주차 기준:

```text
Distance[i][k]
= max(length(Position[j] - Position[i]), DistanceEpsilon)
```

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

초기에는 메모리 cache만 구현해도 된다.

```text
CacheKey
= mesh content hash
+ simulation UV hash
+ state resolution
+ preprocessing version
```

disk serialization은 자료형이 안정된 뒤 추가한다. version 없는 binary dump는 만들지 않는다.

## 구현 대상

- `Source/SurfaceStateSystem/Geometry/SharedSurfaceGeometryData.h/.cpp`
- `Source/SurfaceStateSystem/Mapping/SurfaceGeometryBuilder.h/.cpp`
- 필요 시 `Source/AssetManager/Assets/MeshAsset.h/.cpp`
- `Tests/SharedSurfaceGeometryTests.cpp`

## 작업 순서

1. 모든 출력 배열 크기와 기본값 초기화
2. valid texel의 Position 복원
3. Normal 복원과 fallback
4. Mapping neighbor 복사
5. Distance 계산
6. Surface ID 복사
7. 전체 validation
8. Debug summary 로그

## Validation

- 모든 배열 크기가 texel count와 동일
- valid texel의 Position/Normal이 finite
- Normal 길이가 1에 가까움
- invalid texel은 기본값 유지
- neighbor와 distance slot이 함께 유효/무효
- Distance가 양수이고 finite
- 양방향 Distance 일치
- Surface ID가 SurfaceRange 범위 안

## 테스트

- 단일 triangle의 barycentric Position이 예상값과 일치
- 평면 quad의 모든 Normal이 동일
- 대각선 neighbor distance가 축 neighbor보다 큼
- UV seam edge에서도 거리 연속성 유지
- instance 두 개가 같은 CPU Shared Geometry 객체를 참조할 수 있음

## 권장 커밋 분할

1. `Feat: Texel별 Surface Geometry 복원`
2. `Feat: 이웃 Surface Distance 계산`
3. `Test: Shared Surface Geometry 필드 검증`

## 완료 조건

- Mapping fixture에서 모든 필수 Geometry field가 생성된다.
- CPU validation/test가 통과한다.
- GPU upload에 필요한 배열과 명확한 크기가 준비된다.
- Normal Map이 없어도 Solver를 실행할 수 있는 기본값이 존재한다.
