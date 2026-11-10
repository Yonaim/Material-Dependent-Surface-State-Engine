# Surface Runtime · Registry

Scene의 State 이름·Profile을 runtime channel과 재사용 가능한 정적 Surface Data로 정규화한다. 이 문서의 표는 실제 C++ 탐색 시작점이다.

**연결 문서**
- [[02_Architecture/02_Source/00_CPP/00_SurfaceState/02_Mapping-and-Geometry|Mapping]]
- [[02_Architecture/02_Source/00_CPP/01_GPU/03_Resources-and-Descriptors|GPU Resource]]

## 상위 조정·상태 형식

| 파일 | 주요 선언·코드 | 역할과 데이터 이동 |
|---|---|---|
| [SurfaceStateSystem.h](../../../../../../Source/SurfaceState/SurfaceStateSystem.h) | `FSurfaceStateSystem`, `SubmitContact`, `ResetState`, `RestartState`, 옵션 setter | Scene surface instance와 solver/resource manager의 상위 façade |
| [SurfaceStateSystem.cpp](../../../../../../Source/SurfaceState/SurfaceStateSystem.cpp) | Scene 등록, pending Contact 처리, state reset/restart | Input·Scene 이벤트를 instance state와 solver로 전달하고 GPU resource 변경을 조정 |
| [SurfaceInput.h](../../../../../../Source/SurfaceState/State/SurfaceInput.h) | `FSurfaceContactInput` | 대상 instance, State ID, 도형, 월드 반경, Strength/Falloff를 담는 내부 Contact payload |
| [SurfaceStateTypes.h](../../../../../../Source/SurfaceState/Types/SurfaceStateTypes.h) | profile parameter·transition·profile 데이터 | Asset loader, State Registry, GPU packer가 공유하는 CPU 측 profile 선언 |
| [SurfaceStateTypes.cpp](../../../../../../Source/SurfaceState/Types/SurfaceStateTypes.cpp) | `NormalizeSurfaceStateName`, `ValidateSurfaceResponseProfileData` | 이름 정규화와 profile 숫자 범위·필수 필드 검증 |
| [SurfaceStateRegistry.h](../../../../../../Source/SurfaceState/Types/SurfaceStateRegistry.h) | `FSurfaceStateRegistry`와 등록 profile 구조 | Scene 내 State 이름→channel 및 profile index 연결 |
| [SurfaceStateRegistry.cpp](../../../../../../Source/SurfaceState/Types/SurfaceStateRegistry.cpp) | Registry 구성·조회 | Scene Profile 집합을 고유한 State channel/profile table로 정규화 |
| [SurfaceSolverRates.h](../../../../../../Source/SurfaceState/Types/SurfaceSolverRates.h) | CPU/GLSL 공통 rate 상수 | Solver CPU timestep 상한과 shader 계산이 동일한 기초 rate를 사용하게 함 |
| [SurfaceMappingTypes.h](../../../../../../Source/SurfaceState/Types/SurfaceMappingTypes.h) | resolution·surface·texel geometry 값 형식 | Mapping/Geometry 전처리와 GPU packer가 쓰는 기반 자료형 |
| [SurfaceInstanceStateData.h](../../../../../../Source/SurfaceState/State/SurfaceInstanceStateData.h) | `FSurfaceInstanceStateData`, `GetProfileIndex` | instance별 CPU metadata, texel profile index와 runtime state 관련 크기 |
| [SurfaceInstanceStateData.cpp](../../../../../../Source/SurfaceState/State/SurfaceInstanceStateData.cpp) | instance state 생성·초기화·조회 | GPU State A/B 초기값과 profile map 연계에 필요한 CPU side state 구성 |
| [SurfaceContactApplication.cpp](../../../../../../Source/SurfaceState/Contact/SurfaceContactApplication.cpp) | Contact의 texel 영향 계산·InputDelta 누적 | Contact 형상 안의 유효 texel에 world-area 보정을 적용해 GPU 입력 delta를 구성 |

## Runtime 준비와 캐시

| 파일 | 주요 선언·코드 | 역할과 데이터 이동 |
|---|---|---|
| [SurfaceRuntimeData.h](../../../../../../Source/SurfaceState/Runtime/SurfaceRuntimeData.h) | `FSurfaceRuntimeData`, `FSurfacePreprocessor::Build` | mapping·geometry·profile map을 묶는 전처리 결과 형식과 builder API |
| [SurfaceRuntimeData.cpp](../../../../../../Source/SurfaceState/Runtime/SurfaceRuntimeData.cpp) | Runtime data 생성·공통 metadata | Geometry와 Mapping 결과의 소유·전달 형식 구현 |
| [SurfaceDataManager.h](../../../../../../Source/SurfaceState/Runtime/SurfaceDataManager.h) | `LoadSurfaceData`, `LoadSurfaceDataAtResolution`, Registry build | cache key에 따른 runtime data 요청·resolution variant 관리 |
| [SurfaceDataManager.cpp](../../../../../../Source/SurfaceState/Runtime/SurfaceDataManager.cpp) | cache key·load/build·profile map·normal transfer·meso build | Mesh/profile 입력→mapping→static geometry→runtime data 순으로 처리하고 동일 입력 결과를 재사용 |

## 실제 코드에서 확인한 런타임 경계

- `FSurfaceDataManager::LoadSurfaceData`는 mesh+distribution+resolution 기반 runtime surface를 준비한다. `.Surface` cache는 매번 rasterization을 반복하지 않도록 한다. 소스: [SurfaceDataManager.cpp](../../../../../../Source/SurfaceState/Runtime/SurfaceDataManager.cpp) · [SurfaceCache.cpp](../../../../../../Source/SurfaceState/Cache/SurfaceCache.cpp).
- `BuildSurfaceStateRegistry(const FScene&)`는 Scene이 참조하는 profile들의 State를 Registry에 반영하는 경로다. 이후 `GetSceneSurfaceProfiles`와 Profile GPU table 구성으로 연결된다.
- `FSurfaceStateSystem::SubmitContact`는 pending queue에만 추가하며 GPU State를 즉시 변경하지 않는다. 리셋 처리 `ResetState`와 초기 이벤트 재등록 `QueueInitialContacts`를 구별한다. [SurfaceStateSystem.cpp](../../../../../../Source/SurfaceState/SurfaceStateSystem.cpp).
- runtime handle의 해상도 변종·정리는 `LoadSurfaceDataAtResolution` / `ReleaseUnusedSurfaceData`에서 찾는다. Scene 객체가 교체되면 기존 GPU descriptor가 그대로 유효하다고 가정하지 않는다.

## Surface Data 준비 흐름

`FSurfaceDataManager::LoadSurfaceData`는 다음 순서로 Runtime Surface를 준비한다.

```text
입력 검증 → Runtime key 조회
  ├─ 재사용 가능: 기존 handle 반환
  └─ 생성 필요: Profile 및 Geometry 입력 구성
                 → LoadOrBuildSurfaceGeometry
                 → Runtime Data / handle 등록
```

`LoadOrBuildSurfaceGeometry`의 `.Surface` 캐시가 사용 가능하면 정적 형상을 복원한다. 캐시가 없거나 무효라면 Mapping·Geometry·Normal Transfer·Meso 정보를 구축한다. Runtime handle 재사용과 파일 캐시 재사용은 별개의 판단이다. 입력 검사·캐시 형식은 [[02_Architecture/02_Source/00_CPP/00_SurfaceState/04_Cache-and-Lifecycle|Cache and Lifecycle]]에서 확인한다.
