# Branch 1 — Surface Data Contract

브랜치: `feat/surface-data-contract`  
선행 조건: 없음. 최신 `main`에서 생성한다.  
관련 설계: [[03_Architecture/0002_Surface-State|Surface State]], [[03_Architecture/0003_Assets-and-Profiles|Assets and Profiles]], [[../../00_Project-Overview/0002_Demo|Demos]], [[05_Development/Notes/0003_Surface-State-GPU-Resource|GPU Resource]]

## 목표

Mapping, GPU resource, Solver가 공통으로 사용할 **CPU 자료형과 소유권 계약**을 먼저 확정한다. 이 브랜치에서는 실제 UV rasterization이나 Vulkan compute dispatch를 구현하지 않는다.

현재 `TSharedSurfaceGeometryData`, `TSurfaceInstanceStateData`, `TSurfaceStateSolver`, `TSRProfileLoader`, `SceneLoader`가 dummy이므로 뒤 단계가 이 파일들을 동시에 수정하지 않게 기반을 만든다.

## 완료 결과

- Profile에서 선언되는 State와 parameter의 CPU 표현 및 Registry 계약
- Surface, Geometry, Instance State의 ID/handle 체계
- Mapping과 GPU upload가 공유할 자료형
- `.SRProfile` 검증 경계
- 최소 CPU test target

## 설계 결정

### 상태 채널

State 종류는 enum이나 컴파일 시점의 고정 개수로 정의하지 않는다. 로드된 `.SRProfile`의 `states` key 전체에서 `TSurfaceStateRegistry`를 구성한다. Registry가 이름을 `TStateId`와 런타임 `ChannelIndex`에 연결한다. `Wetness`, `Heat`, `Burn`, `Mud`는 대표 데모 State일 뿐 고정 목록이 아니며, `SurfaceWater`, `Snow`를 포함한 새 State도 Profile에서 선언할 수 있다.

Branch 1의 CPU 계약은 Profile 데이터가 State key와 parameter의 연관을 보존하도록 한다. 실제 deterministic ID/Transition 변환은 Registry가 소유하며, 구체적 결정은 [[04_ADR/0006-Dynamic-State-Registry|ADR 0006]]을 따른다.

| State | 역할 | 대표 전이/특성 |
|---|---|---|
| `Wetness` | 재질 내부에 흡수된 수분 | 기본 데모에서 사용하는 예시 State |
| `Heat` | 열 상태 | `Heat → Burn`, Decay 전이 예시 |
| `Burn` | 그을림·탄 정도 | 잔류 State 예시 |
| `Mud` | 부착·적층되는 진흙 | 높은 cavity retention과 Accumulation 예시 |

Registry 크기에 종속되는 State parameter 목록은 고정 길이 `std::array`가 아니라 동적 컨테이너로 표현한다. CPU 도메인 구조체의 메모리 배치를 GLSL ABI에 직접 맞추지 않는다. GPU 배치 방식은 후속 `feat/surface-gpu-resources`에서 결정한다.

### Profile parameter

```cpp
struct TSurfaceStateParameters
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

using TStateId = std::uint32_t;

struct TSurfaceStateTransition
{
    TStateId Source;
    TStateId Target;
    float Threshold = 0.0F;
    float TransitionRate = 0.0F;
};

struct TSurfaceResponseProfileData
{
    std::unordered_map<TStateId, TSurfaceStateParameters> States;
    std::vector<TSurfaceStateTransition> Transitions;
};
```

Profile 등록 후 Registry를 구성할 때 문자열 key와 Transition endpoint를 Registry ID로 해석한다. Profile이 정의하지 않은 Registry State는 해당 Profile의 미정의 slot으로 구분한다. `unordered_map`은 CPU domain model의 예시이며 GPU upload 순서를 정의하지 않는다.

검증 규칙:

- `StateCapacity > 0`
- Rate/Factor 중 음수를 허용하지 않는 값은 `>= 0`
- `CavityRetentionFactor`, `CavityFillFactor`는 `[0,1]`
- 알 수 없는 State 이름과 필수 키 누락은 Asset 경로와 함께 오류 처리
- transition의 Source/Target이 다르고 유효한 채널인지 확인
- `Threshold ∈ [0,1]`, `TransitionRate >= 0`
- 대표 데모 전이 `Heat → Burn`을 표현할 수 있어야 한다. 다른 전이는 Profile에 선언된 State ID로 표현한다.

### ID와 무효값

서로 다른 의미의 index를 같은 typedef로 섞지 않는다.

```cpp
using TSurfaceLocalID = std::uint32_t;
using TLocalTexelIndex = std::uint32_t;
using TSurfaceProfileIndex = std::uint32_t;

inline constexpr TLocalTexelIndex InvalidTexelIndex = 0xFFFFFFFFU;
inline constexpr TSurfaceLocalID InvalidSurfaceID = 0xFFFFFFFFU;
```

초기 구현에서 강한 타입 wrapper까지 만들 필요는 없지만, 필드명에 `Local`, `Global`, `Base`를 명시한다.
GPU upload에서는 별도 ValidMask를 만들지 않고 invalid texel의 `TexelSurfaceIndex`에 `InvalidSurfaceID`를 기록한다. CPU Runtime mapping은 필요하면 별도 validity 정보를 유지할 수 있다.

### 데이터 소유권

```text
TMeshAsset
└─ SurfaceRange[]                  Mesh의 Surface 구간

TSharedSurfaceGeometryData         같은 전처리 결과를 쓰는 instance가 공유
├─ Mapping metadata
├─ ValidMask
├─ NeighborIndex
├─ TexelProfileIndex
└─ 정적 Geometry field

TSurfaceInstanceStateData          instance별 소유
├─ State A/B
├─ OutgoingFluxScale
├─ InputDelta
└─ Texel→ProfileIndex dense map in TSharedSurfaceGeometryData
```

CPU 구조체는 GPU handle을 필수로 가지지 않는다. CPU 결과와 GPU resource wrapper를 분리해 CPU test가 Vulkan device 없이 실행되게 한다.

## 구현 대상

### 수정

- `Source/SurfaceStateSystem/Geometry/SharedSurfaceGeometryData.h/.cpp`
- `Source/SurfaceStateSystem/State/SurfaceInstanceStateData.h/.cpp`
- `Source/SurfaceStateSystem/State/SurfaceInput.h`
- `Source/AssetManager/Assets/SRProfileAsset.h/.cpp`
- `Source/AssetManager/Loaders/SRProfileLoader.h/.cpp`
- 필요 시 `Source/AssetManager/Core/Asset.h`

### 추가 권장

- `Source/SurfaceStateSystem/Types/SurfaceStateTypes.h`
- `Source/SurfaceStateSystem/Types/SurfaceMappingTypes.h`
- `Tests/SurfaceStateTypesTests.cpp`

여러 모듈이 사용하는 enum, index, CPU data struct는 `SurfaceStateTypes.h`처럼 의존성이 작은 파일에 둔다.

## 작업 순서

1. State channel과 index type 정의
2. Profile CPU 구조체와 기본값 정의
3. Profile validation 함수 작성
4. `TSRProfileAsset`이 검증된 Profile data를 소유하도록 구현
5. `TSharedSurfaceGeometryData`의 CPU container 골격 구현
6. `TSurfaceInstanceStateData`의 CPU 초기 상태와 Profile mapping 골격 구현
7. CTest 또는 작은 CPU test executable 추가
8. dummy 주석 제거 및 include dependency 정리

## JSON Parser 결정

**개발 단계 선택: `nlohmann/json` v3.12.0 헤더를 `ThirdParty/`에 포함한다.**

개발 중 반복 빌드가 네트워크 다운로드와 DNS 상태에 좌우되지 않도록 현재는 헤더와 라이선스를 `ThirdParty/nlohmann_json/`에 고정한다. 이 경로는 개발 단계의 임시 의존성 관리 방식이다.

기능 개발이 완료되면 고정된 `v3.12.0` release를 CMake `FetchContent` 또는 패키지 관리 방식으로 전환한다. 버전 변경은 별도 dependency 변경으로 수행한다.

### 개발 단계 선택 이유

- configure와 빌드 때마다 JSON 라이브러리를 네트워크에서 다시 받을 필요가 없다.
- 현재 사용하는 버전과 라이선스를 저장소 안에서 확인할 수 있다.
- 의존성을 CMake 방식으로 정리하는 일은 개발 완료 시점으로 미룬다.

### 이후 CMake 전환 기준안

전환할 때는 moving branch 대신 `v3.12.0`처럼 버전을 고정한다. 예시:

```cmake
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)
FetchContent_MakeAvailable(json)

target_link_libraries(MDSS PRIVATE nlohmann_json::nlohmann_json)
```

의존성 전환 시 현재의 `ThirdParty/nlohmann_json/` include 경로와 CMake target 연결을 함께 교체한다.

### Parser 책임 분리

`TSRProfileLoader`는 JSON 문법과 필드 변환만 담당하고, 값의 의미 검증은 별도 함수가 담당한다.

```text
File read
→ JSON parse
→ JSON field/type validation
→ TSurfaceResponseProfileData 변환
→ domain validation
→ TSRProfileAsset 생성
```

JSON exception은 그대로 외부에 노출하지 않고 Asset 경로와 JSON key path를 포함한 프로젝트 오류로 변환한다.

## 테스트

구체적인 입력, 기대 결과, fixture와 테스트 함수 목록은 [[06_Testing/0001_Surface-Data-Contract-Tests|Surface Data Contract 테스트 사례]]에서 관리한다.

이 브랜치에서는 CPU Profile/Registry 계약을 검증한다. 동적 channel count에 맞춘 GPU layout과 upload 검증은 실제 resource와 Shader 계약을 정의하는 [[02_Planning/02_Weekly-Details/Week-04/0004_Branch-Surface-GPU-Resources|Branch 4 — Surface GPU Resources]]에서 수행한다.

## 권장 커밋 분할

1. `Build: nlohmann/json 의존성 추가`
2. `Feat: Registry 기반 Surface State 데이터 타입 정의`
3. `Feat: SRProfile TAsset 파싱 및 검증`
4. `Test: Surface Data Contract와 JSON 테스트 추가`

## 완료 조건

- 프로젝트가 warning 없이 빌드된다.
- CPU test가 Vulkan 초기화 없이 실행된다.
- 임의 State 이름과 `Heat → Burn` 같은 전이를 `.SRProfile`로 표현할 수 있다.
- JSON parse 오류가 파일 경로와 key path를 포함한다.
- dummy였던 핵심 데이터 파일에 명확한 소유권과 초기값이 존재한다.
- 다음 브랜치가 추가 설계 없이 `TSurfaceMappingData`를 채울 수 있다.

## 제외 범위

- UV rasterization
- seam 탐색
- GPUBuffer 생성
- descriptor와 compute pipeline
- 실제 Solver 수식
