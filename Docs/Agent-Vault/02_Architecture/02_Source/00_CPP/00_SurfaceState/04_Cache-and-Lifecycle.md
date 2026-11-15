# CPU Surface Cache — 원본 코드 대조

> 검증 기준: 2026-10-26 전달된 `Shaders.zip`의 `Source/SurfaceState/Cache/` 파일. 런타임의 `.Surface` 전처리 캐시이며 GPU state 저장소가 아니다.

## 캐시 생성과 재사용

1. [FSurfaceDataManager::LoadSurfaceData](../../../../../../Source/SurfaceState/Runtime/SurfaceDataManager.cpp)는 Mesh, SurfaceProfileDistribution, simulation resolution을 확보한다.
2. [FSurfaceCache::Describe](../../../../../../Source/SurfaceState/Cache/SurfaceCache.h)는 parsed mesh inputs, Normal Map bytes, ordered Profile assignments를 fingerprint한다. **SRProfile의 반응 계수와 Registry channel은 fingerprint 대상에서 제외**한다.
3. `GetPath(CacheRoot, MeshPath, DistributionPath, Resolution)`으로 해상도별 `.Surface` 파일을 찾고 `Load`를 시도한다. 캐시가 없거나 손상·구버전이면 `std::nullopt`와 diagnostic으로 재구축한다.
4. 재구축 경로에서는 `FSurfaceMappingBuilder::Build` → `FSurfaceGeometryBuilder::Build` → `BuildNormalMapTransferNormal` → `BuildMesoGeometry`를 실행하고 `Save`한다. 저장이 실패해도 새로 계산한 runtime geometry는 사용할 수 있다.

## Cache 계약과 주의할 점

- [SurfaceCache.h](../../../../../../Source/SurfaceState/Cache/SurfaceCache.h): `PreprocessVersion = 4`, `FormatVersion = 4`. mapping/sampling/integration/derivative 규칙을 변경하면 전처리 버전 무효화를 검토한다.
- [SurfaceCache.cpp](../../../../../../Source/SurfaceState/Cache/SurfaceCache.cpp): 캐시는 명시적인 little-endian 필드를 기록하고 임시 파일 작성 후 rename한다.
- 프로파일의 **소재 반응 계수 변경**과 Surface Profile Map의 **텍셀 배정 변경**을 혼동하지 않는다. 전자는 GPU profile table 갱신 대상이며 후자는 CPU geometry cache fingerprint에 관련된다.
- `LoadSurfaceDataAtResolution`은 기존 handle의 mesh/map 경로를 복사해 새 해상도 variant를 로드한다. 수명 정리는 `ReleaseUnusedSurfaceData`가 담당한다.
- 런타임 캐시와 `FSurfaceGPUResourceManager`의 static geometry GPU buffer 및 Instance State A/B는 별개의 수명이다.

## 확인 가능한 사실 / 미확인 범위

- 소스에서 확인: 캐시 fingerprint 입력, format/preprocess version, cache miss fallback, `.Surface` 쓰기 방식.
- 미검증: 실제 디스크의 cache hit rate, 파일 호환성 테스트, build/test 성공 여부. 실행 파일·테스트 데이터는 첨부되지 않았다.

**함께 보기:** [[02_Architecture/00_Flows/00_Initialization/01_Surface-Build-and-GPU-Setup|초기화 흐름]] · [[02_Architecture/02_Source/00_CPP/00_SurfaceState/02_Mapping-and-Geometry|Mapping / Geometry]] · [[02_Architecture/02_Source/00_CPP/01_GPU/03_Resources-and-Descriptors|GPU 자원]].
