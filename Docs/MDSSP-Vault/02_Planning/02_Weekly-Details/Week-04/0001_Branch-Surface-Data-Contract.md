# Branch 1 — Surface Data Contract

브랜치: `feat/surface-data-contract`  
선행 조건: 없음. 최신 `main`에서 생성한다.  
관련 설계: [[03_Architecture/0002_Surface-State|Surface State]], [[03_Architecture/0003_Assets-and-Profiles|Assets and Profiles]], [[03_Architecture/0007_Demos|Demos]], [[05_Development/Notes/0003_Surface-State-GPU-Resource|GPU Resource]]

## 목표

Mapping, GPU resource, Solver가 공통으로 사용할 **CPU 자료형과 소유권 계약**을 먼저 확정한다. 이 브랜치에서는 실제 UV rasterization이나 Vulkan compute dispatch를 구현하지 않는다.

현재 `SharedSurfaceGeometryData`, `SurfaceInstanceStateData`, `SurfaceStateSolver`, `SRProfileLoader`, `SceneLoader`가 dummy이므로 뒤 단계가 이 파일들을 동시에 수정하지 않게 기반을 만든다.

## 완료 결과

- 기본 상태 네 종류와 Profile parameter의 CPU 표현
- Surface, Geometry, Instance State의 ID/handle 체계
- Mapping과 GPU upload가 공유할 자료형
- `.SRProfile` 검증 경계
- 최소 CPU test target

## 설계 결정

### 상태 채널

```cpp
enum class SurfaceStateChannel : std::uint32_t
{
    Wetness = 0,
    Heat = 1,
    Burn = 2,
    Mud = 3,
    Count = 4
};

inline constexpr std::uint32_t SurfaceStateChannelCount =
    static_cast<std::uint32_t>(SurfaceStateChannel::Count);
```

기본 상태 채널은 `Wetness`, `Heat`, `Burn`, `Mud` 네 가지다. 순서는 기존 GPU 저장 순서와 맞춘다. 문자열과 enum 변환은 하나의 table에서 관리하고, 변환 로직을 여러 loader에 복제하지 않는다. `SurfaceWater`와 `Snow`는 목표 데모를 위한 후속 확장 상태이며 이번 구현에는 포함하지 않는다.

| State | 역할 | 대표 전이/특성 |
|---|---|---|
| `Wetness` | 재질 내부에 흡수된 수분 | 기본 채널 |
| `Heat` | 열 상태 | `Heat → Burn`, Decay 가능 |
| `Burn` | 그을림·탄 정도 | 잔류 상태 |
| `Mud` | 부착·적층되는 진흙 | 높은 cavity retention과 Accumulation |

채널별 고정 크기 자료형은 `std::array<T, SurfaceStateChannelCount>`로 표현한다. 예를 들어 `std::array<SurfaceStateParameters, SurfaceStateChannelCount> States;`에 Profile의 채널별 parameter를 담는다. 네 채널은 GPU에서 texel당 `vec4` 하나로 저장하며, CPU와 Shader에서 채널 순서를 동일하게 유지한다. CPU 도메인 구조체의 메모리 배치를 GLSL ABI에 직접 맞추지는 않는다.

### Profile parameter

```cpp
struct SurfaceStateParameters
{
    float StateCapacity = 1.0F;
    float InputFactor = 1.0F;
    float SaturationTransferRate = 0.0F;
    float GeometryTransferRate = 0.0F;
    float DecayRate = 0.0F;
    float CavityRetentionFactor = 0.0F;
    float AccumulationFactor = 0.0F;
    float CavityFillFactor = 0.0F;
};

struct SurfaceStateTransition
{
    SurfaceStateChannel Source;
    SurfaceStateChannel Target;
    float Threshold = 0.0F;
    float TransitionRate = 0.0F;
};

struct SurfaceResponseProfileData
{
    std::array<SurfaceStateParameters, SurfaceStateChannelCount> States;
    std::vector<SurfaceStateTransition> Transitions;
};
```

검증 규칙:

- `StateCapacity > 0`
- Rate/Factor 중 음수를 허용하지 않는 값은 `>= 0`
- `CavityRetentionFactor`, `CavityFillFactor`는 `[0,1]`
- 알 수 없는 State 이름과 필수 키 누락은 Asset 경로와 함께 오류 처리
- transition의 Source/Target이 다르고 유효한 채널인지 확인
- `Threshold ∈ [0,1]`, `TransitionRate >= 0`
- 기본 구현에서 데모 전이 `Heat → Burn`을 표현할 수 있어야 함. `Snow → SurfaceWater → Wetness`는 확장 상태 추가 시 구현한다.

### ID와 무효값

서로 다른 의미의 index를 같은 typedef로 섞지 않는다.

```cpp
using SurfaceLocalID = std::uint32_t;
using LocalTexelIndex = std::uint32_t;
using SurfaceProfileIndex = std::uint32_t;

inline constexpr LocalTexelIndex InvalidTexelIndex = 0xFFFFFFFFU;
inline constexpr SurfaceLocalID InvalidSurfaceID = 0xFFFFFFFFU;
```

초기 구현에서 강한 타입 wrapper까지 만들 필요는 없지만, 필드명에 `Local`, `Global`, `Base`를 명시한다.
GPU upload에서는 별도 ValidMask를 만들지 않고 invalid texel의 `TexelSurfaceIndex`에 `InvalidSurfaceID`를 기록한다. CPU mapping/cache는 필요하면 별도 validity 정보를 유지할 수 있다.

### 데이터 소유권

```text
MeshAsset
└─ SurfaceRange[]                  Mesh의 Surface 구간

SharedSurfaceGeometryData         같은 전처리 결과를 쓰는 instance가 공유
├─ Mapping metadata
├─ ValidMask
├─ NeighborIndex
└─ 정적 Geometry field

SurfaceInstanceStateData          instance별 소유
├─ State A/B
├─ TempAlpha
├─ InputDelta
└─ Surface→Profile index
```

CPU 구조체는 GPU handle을 필수로 가지지 않는다. CPU 결과와 GPU resource wrapper를 분리해 CPU test가 Vulkan device 없이 실행되게 한다.

## 구현 대상

### 수정

- `Source/SurfaceStateSystem/SharedSurfaceGeometryData.h/.cpp`
- `Source/SurfaceStateSystem/SurfaceInstanceStateData.h/.cpp`
- `Source/SurfaceStateSystem/SurfaceInput.h`
- `Source/AssetManager/SRProfileAsset.h/.cpp`
- `Source/AssetManager/Loader/SRProfileLoader.h/.cpp`
- 필요 시 `Source/AssetManager/Asset.h`

### 추가 권장

- `Source/SurfaceStateSystem/SurfaceStateTypes.h`
- `Source/SurfaceStateSystem/SurfaceMappingTypes.h`
- `Tests/SurfaceStateTypesTests.cpp`

여러 모듈이 사용하는 enum, index, CPU data struct는 `SurfaceStateTypes.h`처럼 의존성이 작은 파일에 둔다.

## 작업 순서

1. State channel과 index type 정의
2. Profile CPU 구조체와 기본값 정의
3. Profile validation 함수 작성
4. `SRProfileAsset`이 검증된 Profile data를 소유하도록 구현
5. `SharedSurfaceGeometryData`의 CPU container 골격 구현
6. `SurfaceInstanceStateData`의 CPU 초기 상태와 Profile mapping 골격 구현
7. CTest 또는 작은 CPU test executable 추가
8. dummy 주석 제거 및 include dependency 정리

## JSON Parser 결정

**선택: CMake `FetchContent`로 `nlohmann/json`을 추가하고 `.SRProfile` parsing까지 구현한다.**

`FetchContent`는 외부 JSON 라이브러리 이름이나 별도 패키지 매니저가 아니라, CMake가 제공하는 내장 모듈이다. `include(FetchContent)`로 불러온 뒤 `FetchContent_Declare`와 `FetchContent_MakeAvailable` 명령으로 configure 단계에서 의존성 소스를 가져와 현재 빌드에 연결한다. 이 프로젝트는 이미 CMake 3.24 이상을 요구하고 `FetchContent`를 사용 중이다.

직접 `ThirdParty/`에 `json.hpp`를 복사하는 방식은 사용하지 않는다.

### 선택 이유

- 현재 프로젝트가 이미 glm, GLFW, tinyobjloader, stb, ImGui를 `FetchContent`로 관리하므로 의존성 방식이 일관된다.
- `nlohmann_json::nlohmann_json` CMake target이 include 경로와 compile requirement를 전달한다.
- vendored header 복사본의 출처·버전·라이선스·업데이트를 수동 관리할 필요가 없다.
- release URL과 version을 고정하면 `master`를 가져오는 것보다 재현성이 높다.

권장 CMake 기준안:

```cmake
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)
FetchContent_MakeAvailable(json)

target_link_libraries(MDSS PRIVATE nlohmann_json::nlohmann_json)
```

`GIT_TAG master`는 사용하지 않는다. 현재 권장 release인 `v3.12.0`을 고정하고, 버전 변경은 별도 dependency commit으로 수행한다.

### ThirdParty 직접 추가가 더 나은 경우

다음 조건이 생기면 release archive 또는 single header vendoring을 다시 검토한다.

- 최초 configure도 완전한 offline 환경에서 수행해야 함
- 외부 다운로드가 금지된 제출/배포 환경
- 모든 dependency source를 저장소에 포함해야 하는 규정

현재 저장소는 이미 configure 단계에서 여러 dependency를 내려받으므로 이 조건에 해당하지 않는다.

### Parser 책임 분리

`SRProfileLoader`는 JSON 문법과 필드 변환만 담당하고, 값의 의미 검증은 별도 함수가 담당한다.

```text
File read
→ JSON parse
→ JSON field/type validation
→ SurfaceResponseProfileData 변환
→ domain validation
→ SRProfileAsset 생성
```

JSON exception은 그대로 외부에 노출하지 않고 Asset 경로와 JSON key path를 포함한 프로젝트 오류로 변환한다.

## 테스트

구체적인 입력, 기대 결과, fixture와 테스트 함수 목록은 [[06_Testing/0001_Surface-Data-Contract-Tests|Surface Data Contract 테스트 사례]]에서 관리한다.

이 브랜치에서는 CPU의 채널 순서까지만 검증한다. 네 채널을 GPU `vec4`의 `x/y/z/w`에 pack하는 검증은 실제 upload 구조체와 Shader 계약을 정의하는 [[02_Planning/02_Weekly-Details/Week-04/0004_Branch-Surface-GPU-Resources|Branch 4 — Surface GPU Resources]]에서 수행한다.

## 권장 커밋 분할

1. `Build: add nlohmann json dependency`
2. `Feat: define four-channel surface state data types`
3. `Feat: parse and validate SRProfile assets`
4. `Test: add surface data contract and JSON tests`

## 완료 조건

- 프로젝트가 warning 없이 빌드된다.
- CPU test가 Vulkan 초기화 없이 실행된다.
- 네 상태와 `Heat → Burn` 전이를 `.SRProfile`로 표현할 수 있다.
- JSON parse 오류가 파일 경로와 key path를 포함한다.
- dummy였던 핵심 데이터 파일에 명확한 소유권과 초기값이 존재한다.
- 다음 브랜치가 추가 설계 없이 `SurfaceMappingData`를 채울 수 있다.

## 제외 범위

- UV rasterization
- seam 탐색
- GPUBuffer 생성
- descriptor와 compute pipeline
- 실제 Solver 수식
