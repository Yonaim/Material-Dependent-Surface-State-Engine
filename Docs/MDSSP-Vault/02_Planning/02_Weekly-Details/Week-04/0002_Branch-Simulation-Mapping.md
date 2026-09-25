# Branch 2 — Simulation Mapping

브랜치: `feat/simulation-mapping`  
선행 조건: `feat/surface-data-contract` 병합  
관련 설계: [[05_Development/Notes/0000_Surface-Simulation-Mapping|Surface Simulation Mapping]]

## 목표

OBJ의 준비된 UV를 Simulation UV로 사용해 Mesh 표면을 texel graph로 변환한다. 이번 브랜치에서는 여기에 `.SRProfile` 기반 동적 State Registry와 Runtime에 생성되는 정적 Surface data의 연결 계약을 추가한다. Mapping 및 전처리 결과는 GPU와 무관한 CPU 자료로 검증하고, 후속 Shared Geometry/GPU 브랜치에 전달한다.

## 추가 설계 결정 — State Registry

State 종류를 고정 C++ enum이나 별도 `SurfaceStateSchema`로 정의하지 않는다. 로드한 `.SRProfile`의 `states` key를 모아 `SurfaceStateRegistry`를 구성하며, Profile은 소재별 State 반응 파라미터와 Transition을 소유한다. 런타임에서는 문자열 대신 `StateId` 또는 `ChannelIndex`를 사용한다.

| 작업 | 구현·검증 내용 |
|---|---|
| 이름 정규화 | 앞뒤 whitespace 제거 후 lowercase로 변환하고 나머지 문자는 보존한다. |
| Transition 참조 | source/target에도 동일 정규화를 적용하고 Registry State로 해석되는지 확인한다. |
| ID 배정 | 같은 Profile 집합에서 재현 가능한 배정 규칙을 정하고 테스트로 고정한다. |
| Solver 경계 | Solver가 `Wetness`, `Heat` 등 State 이름에 의존하지 않고 채널을 순회하도록 계약을 정리한다. |
| 스키마 경계 | 별도 `SurfaceStateSchema` 파일을 만들지 않는다. |

## 추가 설계 결정 — Runtime Surface 전처리

정적 Surface data는 각 Runtime의 Asset/Scene load 시 Mesh, Normal Map, Profile Distribution에서 생성해 메모리에 둔다. `.Surface` persistent file/cache는 사용하지 않는다. 이번 브랜치는 Mapping 결과와 Runtime Build API 계약을 정하고, 호출자 연결은 Branch 3에서 다룬다.

| 데이터·책임 | 이번 브랜치에서 정할 내용 |
|---|---|
| Mapping 출력 | 유효성, texel 이웃 및 seam topology를 포함하는 GPU 독립 CPU 데이터 |
| `SurfaceProfileMap` | UV Texel → `ProfileIndex` 매핑. Render Material과 SRProfile을 1:1로 취급하지 않는다. Runtime data 변환 및 sentinel/range 검증 구현됨. |
| 전처리 입력 | Mesh, Normal Map, Profile Distribution 입력의 경계와 검증 책임을 정한다. Profile Distribution의 authoring 형식은 미결 항목으로 명시하고 임의의 파일 형식을 확정하지 않는다. |
| 동적 데이터 제외 | State, Overflow, SRProfile 반응 파라미터는 공유 Runtime Geometry에 넣지 않는다. |
| 저장 경계 | 전처리 결과를 디스크에 저장하지 않는다. Runtime 메모리에서 고유 입력별 결과를 공유한다. GPU resource 배치는 후속 설계 범위다. |

## 구현 상태

| 결정 | 현재 상태 | 남은 연결 작업 |
|---|---|---|
| Dynamic State Registry | 이름 정규화, Profile State union, deterministic ID, Transition ID 변환 및 AssetManager lazy registry 구현·테스트 완료 | Registry 크기를 Branch 4의 instance/GPU resource 생성에 전달하고, Branch 5 Solver가 channel count를 순회하며, Branch 6 Input이 `StateId`를 해석하도록 후속 브랜치에 배정 |
| Dynamic Instance State | texel별 동적 vector channel과 `SurfaceContactInput::StateId` 적용 | Branch 4에서 Registry channel count를 instance state/GPU resource에 연결 |
| Runtime Surface payload | Mapping → shared geometry/texel Profile map 변환, sentinel·범위 검증 완료 | Profile Distribution 입력 형식·loader와 Runtime Asset/Scene 호출 연결을 Branch 3에 배정 |
| 이전 binary cache API | serializer 및 metadata API가 구현돼 있음 | 새 결정에서는 목표 경로에서 제거하거나 비활성화. Cache lookup/save는 Branch 3 범위에서 제외 |
| Normal Map 전처리 | Normal Map은 Runtime builder 입력으로 예정 | CPU texel sample로 Meso/Curvature를 생성하는 알고리즘 미정·미구현 |

## 가장 먼저 해결할 기존 코드 문제

현재 `OBJLoader`는 `(position index, normal index, UV index)` 조합으로 render vertex를 만든다. UV seam에서는 같은 position이 서로 다른 render vertex로 분리되므로, render vertex index만으로는 seam 반대편 triangle을 찾을 수 없다.

따라서 loader 결과에 최소한 다음 정보를 보존해야 한다.

```cpp
struct MeshTriangleSource
{
    std::array<std::uint32_t, 3> RenderVertexIndices;
    std::array<std::int32_t, 3> OriginalPositionIndices;
    std::array<std::int32_t, 3> OriginalUVIndices;
    SurfaceLocalID Surface;
};
```

seam edge key는 `OriginalPositionIndices`로 만들고, rasterization 속성은 render vertex의 Position/Normal/UV에서 읽는다.

## 출력 자료

```cpp
struct SurfaceMappingTexel
{
    SurfaceLocalID Surface = InvalidSurfaceID;
    std::uint32_t Triangle = InvalidTriangleID;
    std::uint32_t Chart = InvalidChartID;
    glm::vec3 Barycentric{0.0F};
    glm::vec3 Position{0.0F};
    glm::vec3 Normal{0.0F, 1.0F, 0.0F};
    std::array<LocalTexelIndex, 8> Neighbors;
};

struct SurfaceMappingData
{
    std::vector<SurfaceTexelRange> Surfaces;
    std::vector<SurfaceMappingTexel> Texels;
    std::vector<std::string> Warnings;
};
```

별도의 `Valid` 필드를 두지 않는다. `Surface`, `Triangle`, `Chart`의 sentinel로 유효성을 판단하며, GPU upload 시에도 별도 ValidMask buffer 대신 invalid texel의 Surface index에 `InvalidSurfaceID`를 기록한다.

## 구현 구성

추가 권장 파일:

- `Source/SurfaceStateSystem/Mapping/SurfaceMappingBuilder.h/.cpp`
- `Source/SurfaceStateSystem/Mapping/UVRasterizer.h/.cpp`
- `Source/SurfaceStateSystem/Mapping/MeshAdjacency.h/.cpp`
- `Source/SurfaceStateSystem/Mapping/SurfaceMappingValidation.h/.cpp`

수정 대상:

- `Source/AssetManager/Loaders/OBJLoader.h/.cpp`
- `Source/AssetManager/Assets/MeshAsset.h/.cpp`
- `Source/AssetManager/Core/AssetManager.cpp`

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

검증기는 Debug build뿐 아니라 Runtime mapping/geometry 생성 시에도 실행할 수 있게 순수 함수로 작성한다.

## 추가 단계 — Registry 및 Profile Map 연결

1. Profile 집합에서 정규화된 State key를 수집해 재현 가능한 `SurfaceStateRegistry`를 생성한다.
2. 각 Profile의 Transition source/target을 Registry ID로 해석하고 미등록 참조를 거부한다.
3. Profile Distribution 입력으로 texel별 `ProfileIndex`를 만들고 유효 texel에 대해 범위와 참조 유효성을 검증한다.
4. Render Material 식별자와 Profile index를 혼동하지 않도록 mapping 결과에 두 책임을 분리해 보존한다.

정규화 후 같은 이름이 된 State key는 하나의 canonical State로 취급한다. Profile Distribution의 authoring 입력 형식이 아직 정해지지 않은 부분은 정책을 임의로 보완하지 말고 미결 결정으로 남긴다.

## 시각화와 디버깅

GPU DebugUI까지 기다리지 말고 CPU 결과를 PPM 또는 단순 RGBA 이미지로 저장하는 기능을 권장한다.

- ValidMask: 흰색/검정
- TriangleID: hash color
- SurfaceID: Surface별 색
- NeighborCount: 0–8 heat map
- Seam texel: 별도 강조색

이 파일은 테스트 출력이므로 저장소에 커밋하지 않는다.

## 테스트 Mesh

구체적인 fixture, 검증 함수와 실행 결과는 [[06_Testing/0002_Surface-Mapping-Tests|Surface Mapping 테스트 사례]]에서 관리한다.

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

Registry/Profile 연결 테스트도 이번 브랜치 문서 범위에 포함한다.

| 검증 대상 | 입력·조건 | 기대 결과 |
|---|---|---|
| State 이름 정규화 | 대소문자 및 앞뒤 공백만 다른 Profile key | 동일 Registry ID로 병합 |
| 이름 구분 | 구두점 또는 내부 공백이 다른 State key | 별도 이름으로 유지 |
| Transition 참조 | 정규화 후 존재하는 source/target | Registry ID로 해석 |
| 알 수 없는 Transition State | Profile 집합에 없는 source/target | 명확한 로드 오류 |
| 재현성 | 동일 Profile 집합을 반복 등록 | 동일한 ID 배정 결과 |
| Profile map | 유효 texel이 Profile index를 참조 | texel count, sentinel 및 Profile count 검사 통과 |
| Runtime 결과 생성 | 동일 입력으로 builder를 반복 호출 | mapping과 geometry가 deterministic하게 생성 |
| Runtime 결과 공유 | 같은 Mesh/Profile Distribution 조합의 복수 instance | 한 전처리 결과를 공유, instance마다 별도 전처리하지 않음 |

## 권장 커밋 분할

1. `Feat: Simulation Mapping용 OBJ Topology Index 보존`
2. `Feat: Mesh Triangle을 Simulation Texel로 Rasterization`
3. `Feat: Regular Texel 이웃 생성`
4. `Feat: UV Seam 이웃 연결`
5. `Test: Simulation Mapping 불변 조건 검증`

## 완료 조건

- 네 가지 정상 fixture의 결과가 deterministic하다.
- seam 유무와 관계없이 실제 topology 이웃이 연결된다.
- State Registry가 Profile 기반으로 생성되고 이름·Transition 검증과 deterministic ID 테스트가 통과한다.
- Mapping 결과의 texel Profile map에서 sentinel과 Profile count 검증이 통과한다.
- Runtime preprocessing 결과가 deterministic하며 같은 입력의 instance 간 공유된다.
- Runtime 결과에 동적 State/Overflow 또는 SRProfile 반응 파라미터가 포함되지 않는다.
- GPU/Vulkan 없이 모든 test가 통과한다.

현재 구현은 별도의 CPU test target으로 위 조건을 검증하며 Vulkan device 초기화가 필요하지 않다.

## 제외 범위

- 자동 UV unwrap
- 보수적 rasterization
- geodesic distance
- GPU buffer upload
- persistent binary serialization 및 cache invalidation은 새 결정에 따라 구현 범위에서 제외
- Normal Map 기반 Meso geometry 값의 최종 생성 알고리즘 및 품질 조정
