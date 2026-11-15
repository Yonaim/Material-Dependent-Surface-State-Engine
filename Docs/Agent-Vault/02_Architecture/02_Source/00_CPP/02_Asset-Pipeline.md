# Asset Pipeline

`FAssetManager`가 경로별 asset을 찾아 loader에 전달한다. Loader는 파일 형식을 CPU 데이터로 해석하고, Asset 클래스는 로드된 데이터를 유지하며 GPU resource가 필요한 유형은 `FVulkanContext`와 Vulkan resource를 만든다. Surface Profile distribution은 Scene mesh의 각 surface에 어떤 `SRProfile`을 적용할지 연결한다.

| 파일 | 코드에서 찾을 것 | 역할과 연결 |
|---|---|---|
| [Asset.h](../../../../../Source/AssetManager/Core/Asset.h) | `FAsset` | 경로·asset 상태·공통 lifecycle 계약 |
| [Asset.cpp](../../../../../Source/AssetManager/Core/Asset.cpp) | 공통 생성·정규화 코드 | Asset 경로와 공통 상태 구현 |
| [AssetManager.h](../../../../../Source/AssetManager/Core/AssetManager.h) | `FAssetManager`, typed load API | Asset 캐시와 loader 호출의 중앙 진입점 |
| [AssetManager.cpp](../../../../../Source/AssetManager/Core/AssetManager.cpp) | load/cache/clear 경로 | 유형별 loader를 골라 경로별 asset을 유지·재사용 |
| [MeshSourceData.h](../../../../../Source/AssetManager/Assets/MeshSourceData.h) | `FVertex`, `FMeshTriangleSource` | OBJ 해석 결과와 mesh asset 생성 사이의 CPU 정점·삼각형 데이터 |
| [OBJLoader.h](../../../../../Source/AssetManager/Loaders/OBJLoader.h) | `FOBJLoader`, `FOBJLoadResult`, `GenerateTangents` | OBJ 읽기 결과와 loader API |
| [OBJLoader.cpp](../../../../../Source/AssetManager/Loaders/OBJLoader.cpp) | OBJ vertex/index/group 해석, tangent 생성 | OBJ의 position·normal·UV를 deduplicate해 `MeshSourceData`를 생성 |
| [MeshAsset.h](../../../../../Source/AssetManager/Assets/MeshAsset.h) | `FMeshAsset`, `FMeshSection` | mesh CPU/GPU 데이터와 section/material 범위 |
| [MeshAsset.cpp](../../../../../Source/AssetManager/Assets/MeshAsset.cpp) | mesh upload·Vulkan buffer 생성 | OBJ CPU 데이터에서 vertex/index GPU buffer를 준비 |
| [MTLLoader.h](../../../../../Source/AssetManager/Loaders/MTLLoader.h) | `FMaterialSourceData`, `FMTLLoader` | MTL material 입력 자료와 parser API |
| [MTLLoader.cpp](../../../../../Source/AssetManager/Loaders/MTLLoader.cpp) | MTL 속성 해석 | diffuse·texture 등의 재질 입력을 material asset 생성용 데이터로 변환 |
| [MaterialAsset.h](../../../../../Source/AssetManager/Assets/MaterialAsset.h) | `FMaterialAsset` | mesh material의 표면·텍스처 참조 유지 |
| [MaterialAsset.cpp](../../../../../Source/AssetManager/Assets/MaterialAsset.cpp) | material 생성·참조 처리 | loader 결과를 renderer가 소비할 재질 asset으로 구성 |
| [TextureLoader.h](../../../../../Source/AssetManager/Loaders/TextureLoader.h) | `FTextureData`, `FTextureLoader` | 디코딩된 pixel buffer와 texture loader API |
| [TextureLoader.cpp](../../../../../Source/AssetManager/Loaders/TextureLoader.cpp) | image decode·pixel 변환 | texture 파일을 GPU 업로드 가능한 pixel format으로 변환 |
| [TextureAsset.h](../../../../../Source/AssetManager/Assets/TextureAsset.h) | `FTextureAsset` | image, image view, sampler GPU 객체 수명 |
| [TextureAsset.cpp](../../../../../Source/AssetManager/Assets/TextureAsset.cpp) | staging upload·image transition | pixel buffer를 Vulkan image로 업로드하고 shader read 상태로 전이 |
| [SRProfileLoader.h](../../../../../Source/AssetManager/Loaders/SRProfileLoader.h) | `FSRProfileLoader` | `.SRProfile` 파싱 API |
| [SRProfileLoader.cpp](../../../../../Source/AssetManager/Loaders/SRProfileLoader.cpp) | JSON 필드·유형·버전 검증 | 파일 내용을 `FSurfaceResponseProfileData`로 변환하고 오류 위치를 보고 |
| [SRProfileAsset.h](../../../../../Source/AssetManager/Assets/SRProfileAsset.h) | `FSRProfileAsset` | 검증된 Surface Response Profile asset |
| [SRProfileAsset.cpp](../../../../../Source/AssetManager/Assets/SRProfileAsset.cpp) | profile 데이터 수명 | loader 결과를 Asset Manager의 공통 asset 수명에 연결 |
| [SurfaceProfileDistributionLoader.h](../../../../../Source/AssetManager/Loaders/SurfaceProfileDistributionLoader.h) | `FSurfaceProfileDistribution`, loader API | surface index→profile path 분포 데이터 |
| [SurfaceProfileDistributionLoader.cpp](../../../../../Source/AssetManager/Loaders/SurfaceProfileDistributionLoader.cpp) | map 파일 파싱·index 검증 | `.SurfaceProfileMap`에서 surface별 profile 배치를 읽음 |

추적 예: Scene의 mesh path→`FAssetManager`→`FOBJLoader`→`FMeshAsset`; profile map→distribution loader→profile assets→`FSurfaceStateRegistry`와 GPU Profile table. GPU 배치 세부는 [[02_Architecture/02_Source/00_CPP/01_GPU/00_Index|GPU Resources]], 파일 계약은 [[02_Architecture/01_Contracts/02_Assets-and-Profiles|Assets and Profiles]]를 본다.

## 실제 로드 경로와 산출물

[`FSceneLoader`](../../../../../Source/Scene/SceneLoader.cpp)가 Scene object별 mesh path와 profile distribution path를 해석한다. `FAssetManager`는 path에 대응하는 asset handle을 반환하고 중복 로드를 피한다. `FOBJLoader`가 position·normal·UV·triangle/group을 CPU mesh 자료로 만들면 `FMeshAsset`이 section/material 범위와 GPU vertex/index buffer를 보유한다. 렌더러는 이 buffer를 base surface draw에 사용하고 Surface 전처리는 동일 mesh의 triangle/UV를 사용한다.

`FMTLLoader`/`FMaterialAsset`과 `FTextureLoader`/`FTextureAsset`은 렌더 재질 경로다. normal texture는 표시 재질인 동시에 `NormalMapTransferNormalBuilder`가 transfer normal을 구성할 때 참고할 수 있으므로, normal 처리 변경은 [[02_Architecture/02_Source/00_CPP/00_SurfaceState/00_Index|Surface State 지도]]도 확인한다. `.SRProfile`의 수치·이름은 `FSRProfileLoader`가 파싱·검증하고 `FSRProfileAsset`이 보관한다. `.SurfaceProfileMap`은 mesh surface index를 profile path에 대응시키며, `FSurfaceDataManager`와 Scene Registry가 이를 texel별 profile lookup과 GPU table로 연결한다.

| 바뀐 입력 | 다시 확인할 결과 | 다음 코드 |
|---|---|---|
| mesh triangle/UV | 유효 texel 수, seam·neighbor graph, texel 면적 | `SurfaceMappingBuilder.cpp`, `SurfaceGeometryBuilder.cpp` |
| profile distribution | surface별 profile index, 지원 State channel | `SurfaceDataManager.cpp`, `SurfaceStateRegistry.cpp` |
| profile 수치 | capacity·decay·transfer parameter의 packed 값 | `ProfilePacking.cpp`, Solver shader |
| texture/normal map | GPU sampler와 transfer normal의 해석 | `TextureAsset.cpp`, `NormalMapTransferNormalBuilder.cpp` |

로드 문제를 좁힐 때는 파일 parser가 반환한 CPU 값, Asset handle, runtime surface handle, GPU upload 결과를 구분한다. 같은 이름의 profile이 Scene Registry에서 어떤 State ID를 얻었는지는 asset 자체만 읽어서는 알 수 없다.

## Scene → Asset → Runtime 의 검증된 분기

- [FSceneLoader::Load](../../../../../Source/Scene/SceneLoader.cpp)가 `simulationResolution`과 `surfaceProfileMap`을 파싱하며, profile distribution 경로가 있는 경우 `FSurfaceDataManager::LoadSurfaceData(MeshHandle, DistributionPath, Scene.GetSimulationResolution())`을 호출한다.
- [LoadSurfaceData](../../../../../Source/SurfaceState/Runtime/SurfaceDataManager.cpp)는 `.Surface` 전처리 캐시 조회 또는 rebuild를 수행한다. 재질 이름만으로 State channel을 가정하지 않고, profile table/registry와 텍셀 배정을 확인해야 한다.
- 캐시 생성/무효화 조건은 [[02_Architecture/02_Source/00_CPP/00_SurfaceState/04_Cache-and-Lifecycle|CPU Surface Cache]]에서 관리한다. 소스 ZIP에 없는 Scene JSON/프로파일 fixture의 실제 값은 검증하지 않았다.
