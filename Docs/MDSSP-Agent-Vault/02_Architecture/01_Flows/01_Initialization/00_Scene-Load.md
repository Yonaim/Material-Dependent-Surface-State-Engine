# Scene Load — Asset과 Instance 입력

Scene 로더가 Mesh·Profile Map·해상도를 읽고 Runtime Surface를 요청하는 경로다. UV/Geometry 전처리의 상세 계산 의미는 [[02_Architecture/02_Contracts/01_Assets-and-Profiles|Asset 계약]]과 [[02_Architecture/02_Contracts/02_Geometry|Geometry 계약]]이 기준이다.

**연결 문서**
- [[02_Architecture/01_Flows/01_Initialization/01_Surface-Build-and-GPU-Setup|Surface Build]]
- [[02_Architecture/03_Source/Src_Cpp/02_Asset-Pipeline|Asset C++]]

## 로드 경로

1. [`TSceneLoader`](../../../../../Source/Scene/SceneLoader.cpp)가 Scene의 mesh, `.SurfaceProfileMap`, simulation resolution을 읽어 instance를 구성한다. [`TSurfaceDataManager::LoadSurfaceData`](../../../../../Source/SurfaceState/Runtime/SurfaceDataManager.cpp)는 mesh·profile distribution·resolution 조합의 runtime data를 조회하거나 생성한다.

## 산출물과 다음 단계

Mesh/profile distribution/resolution으로 Runtime Surface를 조회한 뒤 Mapping·Geometry build와 GPU resource ensure로 이어진다. [[02_Architecture/03_Source/Src_Cpp/03_SurfaceState/01_Runtime-and-Registry|Runtime 소스]]를 참고한다.

## 파싱 단계의 실제 분기

[SceneLoader.cpp](../../../../../Source/Scene/SceneLoader.cpp)의 `TSceneLoader::Load`는 `simulationResolution` 값을 처리한 뒤 Mesh instance를 로드한다. `surfaceProfileMap`이 있는 경우에만 `TSurfaceDataManager::LoadSurfaceData(MeshHandle, DistributionPath, Resolution)`로 시뮬레이션 runtime handle을 연결한다. 표시 가능한 Static Mesh라고 해서 반드시 Solver용 Surface 데이터를 가진 것은 아니다.

해상도 값·Profile Map의 경로와 유효성 오류는 이 단계에서 먼저 확인한다. Runtime Surface가 구성된 뒤에는 [[02_Architecture/03_Source/Src_Cpp/03_SurfaceState/04_Cache-and-Lifecycle|CPU 캐시]]를 확인한다.
