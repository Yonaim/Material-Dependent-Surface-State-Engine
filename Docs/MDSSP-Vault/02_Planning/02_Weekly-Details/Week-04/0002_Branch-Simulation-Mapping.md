# Branch 2 — Simulation Mapping

브랜치: `feat/simulation-mapping`  
선행 조건: `feat/surface-data-contract` 병합  
관련 설계: [[05_Development/Notes/0000_Surface-Simulation-Mapping|Surface Simulation Mapping]]

## 목표

OBJ의 준비된 UV를 Simulation UV로 사용해 Mesh 표면을 texel graph로 변환한다. 결과는 GPU와 무관한 CPU 자료이며, 다음 브랜치의 Shared Geometry 입력이 된다.

## 가장 먼저 해결할 기존 코드 문제

현재 `OBJLoader`는 `(position index, normal index, UV index)` 조합으로 render vertex를 만든다. UV seam에서는 같은 position이 서로 다른 render vertex로 분리되므로, render vertex index만으로는 seam 반대편 triangle을 찾을 수 없다.

따라서 loader 결과에 최소한 다음 정보를 보존해야 한다.

```cpp
struct MeshTriangleSource
{
    std::array<std::uint32_t, 3> RenderVertexIndices;
    std::array<std::int32_t, 3> OriginalPositionIndices;
    SurfaceLocalID SurfaceID;
};
```

seam edge key는 `OriginalPositionIndices`로 만들고, rasterization 속성은 render vertex의 Position/Normal/UV에서 읽는다.

## 출력 자료

```cpp
struct TexelMapping
{
    std::uint32_t TriangleID = InvalidTexelIndex;
    glm::vec2 Barycentric{0.0F}; // b0, b1; b2 = 1-b0-b1
    SurfaceLocalID SurfaceID = 0;
    bool Valid = false;
};

struct SurfaceMappingData
{
    std::uint32_t Width = 0;
    std::uint32_t Height = 0;
    std::vector<TexelMapping> Texels;
    std::vector<std::array<LocalTexelIndex, 8>> Neighbors;
};
```

CPU mapping 결과는 `Valid`를 보존한다. GPU upload 시 별도 ValidMask buffer를 만들지 않고 invalid texel의 `TexelSurfaceIndex`에 `InvalidSurfaceID`를 기록한다.

## 구현 구성

추가 권장 파일:

- `Source/SurfaceStateSystem/Mapping/SurfaceMappingBuilder.h/.cpp`
- `Source/SurfaceStateSystem/Mapping/UVRasterizer.h/.cpp`
- `Source/SurfaceStateSystem/Mapping/MeshAdjacency.h/.cpp`
- `Source/SurfaceStateSystem/Mapping/SurfaceMappingValidation.h/.cpp`

수정 대상:

- `Source/AssetManager/Loader/OBJLoader.h/.cpp`
- `Source/AssetManager/MeshAsset.h/.cpp`
- `Source/SurfaceStateSystem/SharedSurfaceGeometryData.h`

## 단계 1 — 입력 검증

- 해상도 `width > 0`, `height > 0`
- UV가 `[0,1]` 범위
- UV triangle 면적이 epsilon 초과
- triangle index 범위 확인
- Surface ID 범위 확인
- 의도하지 않은 UV overlap 탐지
- 최소 한 texel도 얻지 못하는 triangle 경고
- non-manifold edge 탐지

4주차에는 자동 unwrap이나 자동 보정을 하지 않는다. 실패 위치를 Asset/Surface/Triangle ID와 함께 출력한다.

## 단계 2 — UV Rasterization

1. UV를 `[0,width] × [0,height]` texel 좌표로 변환한다.
2. triangle bounding box를 grid 범위로 clamp한다.
3. texel center `(x+0.5, y+0.5)`를 edge function으로 검사한다.
4. 공유 edge는 top-left rule로 소유 triangle을 결정한다.
5. barycentric `(b0,b1)`와 TriangleID, SurfaceID를 저장한다.

OBJLoader가 V축을 이미 뒤집고 있으므로 mapping 단계에서 다시 뒤집지 않는다. 이 규칙은 test로 고정한다.

## 단계 3 — Chart ID

UV에서 edge를 실제로 공유하는 triangle끼리 flood fill하여 `ChartID`를 만든다. 단순히 texel 좌표가 붙어 있다는 이유로 다른 chart를 연결하지 않는다.

같은 chart의 valid texel에만 기본 8-neighbor를 만든다.

## 단계 4 — Mesh Adjacency와 Seam

```text
EdgeKey = sort(originalPositionIndexA, originalPositionIndexB)
```

- incident triangle 1개: 실제 open boundary
- incident triangle 2개 + UV edge 일치: 일반 연결
- incident triangle 2개 + UV edge 불일치: seam
- incident triangle 3개 이상: non-manifold 오류 또는 명시적 제외

seam 양쪽 boundary texel을 edge parameter `t`로 대응시킨다. 상대 texel은 기존 invalid neighbor slot에 넣고 반드시 양방향으로 등록한다.

4주차에는 한 seam edge에서 대응 후보가 8-neighbor 제한을 넘으면 오류로 보고하고 해당 Asset을 테스트 대상에서 제외한다. 임의로 먼 이웃을 제거하지 않는다.

## 단계 5 — 불변조건 검증

- 자기 자신을 이웃으로 갖지 않음
- 중복 이웃 없음
- 모든 이웃 index가 texel 범위 안
- invalid texel이 이웃으로 참조되지 않음
- `i → j`이면 `j → i`
- texel당 이웃 수 `<= 8`

검증기는 Debug build뿐 아니라 mapping cache 생성 시에도 실행할 수 있게 순수 함수로 작성한다.

## 시각화와 디버깅

GPU DebugUI까지 기다리지 말고 CPU 결과를 PPM 또는 단순 RGBA 이미지로 저장하는 기능을 권장한다.

- ValidMask: 흰색/검정
- TriangleID: hash color
- SurfaceID: Surface별 색
- NeighborCount: 0–8 heat map
- Seam texel: 별도 강조색

이 파일은 테스트 출력이므로 저장소에 커밋하지 않는다.

## 테스트 Mesh

최소 네 가지 fixture를 둔다.

1. 단일 triangle
2. 두 triangle으로 된 quad, seam 없음
3. 같은 quad이지만 UV seam으로 분리
4. 공간상 가깝지만 topology상 분리된 두 quad

추가 오류 fixture:

- UV 면적 0
- overlap
- non-manifold edge
- UV 없는 OBJ

## 권장 커밋 분할

1. `Feat: preserve OBJ topology indices for simulation mapping`
2. `Feat: rasterize mesh triangles into simulation texels`
3. `Feat: build regular texel neighbors`
4. `Feat: stitch UV seam neighbors`
5. `Test: validate simulation mapping invariants`

## 완료 조건

- 네 가지 정상 fixture의 결과가 deterministic하다.
- seam 유무와 관계없이 실제 topology 이웃이 연결된다.
- Mapping 결과를 파일로 저장하지 않아도 재생성 가능하다.
- GPU/Vulkan 없이 모든 test가 통과한다.

## 제외 범위

- 자동 UV unwrap
- 보수적 rasterization
- geodesic distance
- GPU buffer upload
- Normal Map 기반 Meso geometry
