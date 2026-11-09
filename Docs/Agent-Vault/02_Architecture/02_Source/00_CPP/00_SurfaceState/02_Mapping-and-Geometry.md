# Surface Mapping · Geometry CPU

Simulation UV mapping, seam·neighbor와 정적/meso geometry 전처리, render texel mesh 생성 경로를 추적한다.

**연결 문서**
- [[02_Architecture/01_Contracts/03_Geometry|Geometry Contract]]
- [[02_Architecture/02_Source/01_Shaders/00_Simulation/03_Dynamic-Geometry|Dynamic Shader]]

## Texel mapping

| 파일 | 주요 선언·코드 | 역할과 데이터 이동 |
|---|---|---|
| [SurfaceMappingData.h](../../../../../../Source/SurfaceState/Mapping/SurfaceMappingData.h) | `FSurfaceMappingTexel`, `FSurfaceMappingData` | 유효 texel, source triangle/barycentric 정보, 최대 8방향 neighbour graph 저장 형식 |
| [SurfaceMappingBuilder.h](../../../../../../Source/SurfaceState/Mapping/SurfaceMappingBuilder.h) | `FSurfaceMappingBuilder::Build` | mesh vertex/triangle 및 resolution에서 mapping 생성하는 API |
| [SurfaceMappingBuilder.cpp](../../../../../../Source/SurfaceState/Mapping/SurfaceMappingBuilder.cpp) | triangle raster coverage, closest point, seam chart 연결, `AddBidirectionalNeighbor` | Simulation UV에서 texel을 triangle에 매핑하고 UV island seam을 실제 topology로 잇는 주요 알고리즘 |
| [SurfaceMappingValidation.h](../../../../../../Source/SurfaceState/Mapping/SurfaceMappingValidation.h) | `ValidateSurfaceMapping` | Mapping invariant 검사 API |
| [SurfaceMappingValidation.cpp](../../../../../../Source/SurfaceState/Mapping/SurfaceMappingValidation.cpp) | texel 범위·neighbor 대칭·중복 검사 | Mapping builder 결과의 invalid index와 잘못된 edge를 조기 검출 |
| [NormalMapTransferNormalBuilder.h](../../../../../../Source/SurfaceState/Mapping/NormalMapTransferNormalBuilder.h) | `BuildNormalMapTransferNormal` | texel 위치에서 normal-map transfer normal 산출 API |
| [NormalMapTransferNormalBuilder.cpp](../../../../../../Source/SurfaceState/Mapping/NormalMapTransferNormalBuilder.cpp) | repeat/linear texture sample, tangent-space normal 변환 | 색상용 normal sampling과 Solver geometry용 transfer normal 계산을 분리 |

## Geometry·render mesh 준비

| 파일 | 주요 선언·코드 | 역할과 데이터 이동 |
|---|---|---|
| [SharedSurfaceGeometryData.h](../../../../../../Source/SurfaceState/Geometry/SharedSurfaceGeometryData.h) | `FSharedSurfaceGeometryData`, texel/profile lookup | 동일 정적 surface가 GPU instance 사이 공유하는 CPU geometry |
| [SharedSurfaceGeometryData.cpp](../../../../../../Source/SurfaceState/Geometry/SharedSurfaceGeometryData.cpp) | 저장 구조 초기화·lookup 구현 | mapping·texel geometry의 공유 소유 데이터 구성 |
| [SurfaceGeometryBuilder.h](../../../../../../Source/SurfaceState/Geometry/SurfaceGeometryBuilder.h) | `FSurfaceGeometryBuilder` | Mapping·mesh 입력에서 정적 surface geometry를 만드는 API |
| [SurfaceGeometryBuilder.cpp](../../../../../../Source/SurfaceState/Geometry/SurfaceGeometryBuilder.cpp) | source triangle·texel geometry 구성 | Mapping의 barycentric/UV 대응으로 각 Simulation texel의 정적 position, normal, area·neighbor geometry 생성 |
| [MesoGeometryBuilder.h](../../../../../../Source/SurfaceState/Geometry/MesoGeometryBuilder.h) | `FMesoGeometryBuildReport`, `BuildMesoGeometry` | source mesh/normal-map에서 meso-scale geometry 준비 |
| [MesoGeometryBuilder.cpp](../../../../../../Source/SurfaceState/Geometry/MesoGeometryBuilder.cpp) | differential geometry·concavity field 적합 | texel neighborhood에서 meso normal/curvature/concavity 보조량을 계산 |
| [SurfaceGeometryUpdate.h](../../../../../../Source/SurfaceState/Geometry/SurfaceGeometryUpdate.h) | 동적 형상 업데이트 입력·CPU API | State에서 파생한 geometry update를 호출하는 경계 |
| [SurfaceGeometryUpdate.cpp](../../../../../../Source/SurfaceState/Geometry/SurfaceGeometryUpdate.cpp) | accumulation height·dynamic geometry dispatch 준비 | Solver 전후에 instance geometry와 transfer-weight update shader를 기록 |
| [SurfaceTexelMeshBuilder.h](../../../../../../Source/SurfaceState/Geometry/SurfaceTexelMeshBuilder.h) | texel render vertex/range/mesh 형식, `BuildSurfaceTexelMesh` | Simulation texel을 화면에 그릴 mesh와 boundary metadata API |
| [SurfaceTexelMeshBuilder.cpp](../../../../../../Source/SurfaceState/Geometry/SurfaceTexelMeshBuilder.cpp) | grid mesh·subdivision·edge weld·side adjacency | simulation grid 또는 원본 topology에 맞춘 render mesh와 overlay boundary 생성 |

## 전처리 결과를 따라가기

`SurfaceDataManager.cpp`의 `LoadSurfaceData`는 cache key와 resolution variant를 확인한 뒤 mapping과 shared geometry를 만든다. `SurfaceMappingBuilder.cpp`의 triangle raster coverage는 simulation UV texel을 source triangle·barycentric 좌표에 연결한다. texel의 최대 8방향 neighbor는 UV 배열의 인접성과 mesh 실제 topology를 함께 반영하며 seam stitch는 chart가 달라도 원본 mesh에서 붙는 곳을 연결한다. `SurfaceMappingValidation.cpp`가 범위·중복·이웃 대칭을 검사한다.

`SurfaceGeometryBuilder.cpp`는 mapping의 triangle 대응으로 정적 position·normal·area와 이웃 기하량을 만든다. `MesoGeometryBuilder.cpp`는 주변 표본에서 meso normal·concavity를 계산한다. `NormalMapTransferNormalBuilder.cpp`는 tangent-space normal map을 transfer 계산용 normal로 바꾼다. 이 결과는 `FSharedSurfaceGeometryData`로 묶여 동일 runtime surface의 instance 사이에 공유된다. instance의 transform을 적용한 world area·transfer weight와 동적 geometry는 별도이므로 한 instance 변경이 다른 instance State를 직접 바꾸지 않는다.

## CPU 전처리의 실제 함수 순서

`SurfaceDataManager.cpp`의 cache miss branch를 기준으로:

1. [FSurfaceMappingBuilder::Build](../../../../../../Source/SurfaceState/Mapping/SurfaceMappingBuilder.cpp): triangle/Simulation UV raster mapping, chart와 이웃 정보 생성.
2. [FSurfaceGeometryBuilder::Build](../../../../../../Source/SurfaceState/Geometry/SurfaceGeometryBuilder.cpp): mapped texel의 static position/normal/area 및 neighbor geometry 준비.
3. [BuildNormalMapTransferNormal](../../../../../../Source/SurfaceState/Mapping/NormalMapTransferNormalBuilder.cpp): Normal Map bytes를 texture loader로 확보해 transfer용 normal을 구축. Material별 normal map path가 다르면 surface 매칭을 확인한다.
4. [BuildMesoGeometry](../../../../../../Source/SurfaceState/Geometry/MesoGeometryBuilder.cpp): meso virtual height/concavity 등의 세부 기하량을 계산.
5. [FSurfaceCache::Save](../../../../../../Source/SurfaceState/Cache/SurfaceCache.cpp): 전처리 결과를 캐시에 저장.

**주의:** `BuildMesoGeometry`는 함수이며 `TMesoGeometryBuilder` 클래스 호출이 아니다. Normal Map 높이를 실제 변형된 Mesh의 절대 높이로 취급하지 않는다. 코드에 기록된 seam 연결 실패 경고도 확인할 것.
