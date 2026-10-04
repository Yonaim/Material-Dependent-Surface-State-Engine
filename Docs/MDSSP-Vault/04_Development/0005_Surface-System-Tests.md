# Surface System Tests

> **한 줄 요약:** Surface 데이터 계약, Mapping, Normal Map 전달과 Scene resource 회귀 테스트를 한 곳에서 관리한다.

## 테스트 구성 가이드

이 문서는 테스트 코드와 입력 fixture를 구분하는 공통 원칙을 간략히 정리한다. 개별 구현의 구체적인 테스트 사례는 별도 문서에 둔다.

### 테스트 코드와 fixture의 역할

| 구성 요소 | 담당 역할 |
|---|---|
| 테스트 코드 | 조건을 구성하고 동작과 결과를 검증 |
| Fixture | parser/loader 등 실제 입력 파일 경로를 검증 |
| 테스트 runner | 테스트 실행 및 결과 보고 |

JSON parser의 파일 처리 자체를 확인하는 사례에는 fixture를 쓴다. 값 범위나 자료구조 불변조건은 테스트 코드에서 객체를 직접 구성하면 fixture가 불필요하게 늘어나는 것을 막을 수 있다.

### 테스트 사례 문서화

사례 문서에는 검증 대상, 입력·조건, 기대 결과와 테스트 방식을 기록한다. 반복 적용할 기준만 여기에 남기고, 구현별 함수명·fixture명·개별 결과는 주제별 문서에서 관리한다.

### Fixture 관리

- fixture는 `Tests/Fixtures/`에 두고 검증 의도가 드러나는 이름을 사용한다. 예: `Valid.SRProfile`, `UnknownState.SRProfile`.
- 정상 입력과 실패 입력을 각각 최소 단위로 구성한다. 하나의 fixture에 여러 오류를 섞지 않는다.
- 테스트가 실행 당시 작업 디렉터리에 의존하지 않도록 CMake가 fixture 경로를 전달하게 한다.
- fixture는 실제 parser/loader를 통해 읽고, 객체 직접 검증을 대신하는 용도로 쓰지 않는다.

### GPU 및 통합 테스트 경계

| 검증 대상 | 적절한 단계 |
|---|---|
| CPU 로직과 파일 parsing | CPU 단위 테스트 |
| GPU 배치와 동기화 | layout 단위 테스트 또는 Vulkan 통합 테스트 |

GPU resource가 아직 존재하지 않는 단계에서 GPU 배치 검증을 완료로 표시하지 않는다.

세부 사례는 같은 디렉터리의 번호별 테스트 문서에서 관리한다.

## Surface Data Contract 테스트 사례

CPU 자료형, `.SRProfile` loader, State Registry, Surface geometry/Profile map 및 Runtime preprocessing 계약의 테스트 사례를 관리한다. 공통 원칙은 이 문서의 `테스트 구성 가이드`를 따른다.

### 테스트 함수 구성

| 테스트 함수 | 입력 방식 | 검증 범위 |
|---|---|---|
| `TestProfileAndRegistry` | Profile 객체와 JSON fixture | 임의 State 이름, 정규화, Registry ID, Transition 참조 및 재현성 |
| `TestTransferFactorValidation` | C++ Profile 객체 | 두 Factor의 `[0,1]` 경계값 허용 및 음수·상한 초과·NaN/Inf 거부 |
| `TestGeometryAndInstanceData` | C++ 객체 직접 구성 | Surface 범위, sentinel, texel Profile map, 동적 State 채널 |
| `TestContactInputType` | `TSurfaceContactInput` 직접 구성 | Registry `TStateId`와 입력 기본값 |
| `TestSurfacePreprocessing` | Mapping/Profile Distribution 입력 | Profile map 구성, deterministic build 및 같은 입력의 결과 공유 |

### 검증 사례

| 대상 | 입력·조건 | 기대 결과 | 방식 |
|---|---|---|---|
| 정규화 전달 계수 | version 2의 두 TransferFactor | Factor를 그대로 로드하고 Solver 기준 속도는 적용하지 않음 | `Valid.SRProfile` |
| 이전 schema | version 1의 실제 Rate 필드 | version 2 요구 오류 | `LegacyRates.SRProfile` |
| 범위 밖 전달 계수 | version 2에서 GeometryTransferFactor=50 | 필드 경로와 `[0,1]` 오류 | `InvalidTransferFactor.SRProfile` |
| Factor 범위 | 각 Factor의 0, 0.5, 1 및 음수·1 초과·NaN/Inf | 정상 범위 허용, 나머지 거부 | C++ 직접 검증 |
| 부분 State 정의 | Profile에 임의 State key 하나만 선언 | 고정된 채널 집합 없이 로드 | 정상 JSON fixture |
| 이름 정규화 | 대소문자와 앞뒤 ASCII whitespace 차이 | 같은 canonical name; 구두점과 내부 공백은 보존 | 직접 검증 및 fixture |
| Registry 구성 | 둘 이상의 Profile에 서로 다른 State 선언 | 전체 State union에 deterministic ID 배정 | C++ 직접 검증 |
| ID 재현성 | 동일 Profile 집합을 다른 순서로 제공 | bytewise 이름 정렬에 따라 같은 ID | C++ 직접 검증 |
| 미지원 State slot | 한 Profile에만 있는 State를 다른 Profile에서 조회 | `optional` empty로 미지원 표시 | C++ 직접 검증 |
| Transition endpoint | 등록 State를 가리키는 source/target | runtime `TStateId`로 변환 | C++ 직접 검증 |
| 알 수 없는 Transition endpoint | 전체 Profile 집합에 없는 State 참조 | Registry 생성 오류 | `UnknownState.SRProfile` |
| 잘못된 JSON 자료형 | 숫자 필드에 문자열 입력 | JSON 경로를 포함한 오류 | `WrongFieldType.SRProfile` |
| 필수 parameter 누락 | State parameter 하나 생략 | 누락된 필드 경로를 표시 | `MissingParameter.SRProfile` |
| parameter 범위 | 음수 Rate, Capacity 0, Factor 범위 초과 | 검증 오류 | C++ 직접 검증 |
| Factor 경계 | Factor에 `0`과 `1` 입력 | 허용 | C++ 직접 검증 |
| 자기 전이 | 정규화 후 source와 target이 같음 | 거부 | C++ 직접 검증 |
| Geometry sentinel | 예약 Surface ID를 실제 ID로 입력 | 거부 | C++ 직접 검증 |
| 빈 Surface 목록 | 빈 정의 목록 전달 | 거부 | 생성자 검증 |
| Texel Profile map | valid/invalid texel에 정상 index/sentinel 설정 | count·sentinel·ProfileCount 검사 통과 | C++ 직접 검증 |
| 잘못된 Profile index | index가 `ProfileCount` 이상 | Runtime Surface build 거부 | C++ 직접 검증 |
| Runtime build 재현성 | 같은 입력으로 전처리 반복 | geometry 및 Profile map 동일 | CPU builder |
| Runtime 공유 | 동일 Mesh/Profile Distribution을 쓰는 복수 instance | 동일한 전처리 결과 참조 | Runtime Asset/Scene 연결 |
| 전처리 입력 변경 | Mesh UV 또는 Profile Distribution 변경 | 새 입력으로 Runtime 결과 재생성 | C++ 직접 검증 |

### Fixture 목록

| 파일 | 목적 |
|---|---|
| `LegacyRates.SRProfile` | version 1의 Rate schema를 거부 |
| `InvalidTransferFactor.SRProfile` | version 2의 정규화 계수 범위 위반을 거부 |
| `Valid.SRProfile` | 정규화할 이름과 Transition을 포함하는 다중 State Profile |
| `MissingState.SRProfile` | 임의의 단일 State만 지원하는 부분 Profile |
| `UnknownState.SRProfile` | 존재하지 않는 Transition endpoint를 Registry에서 거부 |
| `WrongFieldType.SRProfile` | 숫자 필드가 잘못된 JSON 자료형인 경우 |
| `MissingParameter.SRProfile` | 필수 parameter가 누락된 경우 |

### 실행 및 완료 확인

| 항목 | 기준 |
|---|---|
| 실행 명령 | `ctest --test-dir <build-dir> --output-on-failure` |
| 실행 조건 | CPU contract test는 Vulkan 초기화 없이 실행 |
| 완료 기준 | 모든 contract test가 성공 |

GPU memory packing, descriptor와 barrier 검증은 GPU Resource 단계에서 별도로 다룬다.

### 면적·시간 계약 확장

`MDSS_SimulationTransport`는 15·30·60·120 FPS clock, 반복 한도·backlog·pause·배속 및 128·256·512의 비균일 scale 면적 합을 검사한다. `MDSS_SurfaceGPUResource`는 면적 환산 Capacity·Decay, 상한 없는 Geometry mobility, SaturationDrive OFF, 한 command buffer의 반복·입력 한 번 소비와 단일 이웃의 공간 스케일을 검사한다. cache ON/OFF 검사는 이전 구현 당시의 기록이다. `MDSS_SceneResources`는 실제 접촉 입력의 면적 환산을 확인한다. 전체 결과와 아직 실시하지 않은 비교 범위는 [[0004_Solver-Validation#검증 — 면적 환산과 누적 시간|면적·시간 회귀 검증]]을 따른다.

Geometry 기준값 6000은 `MDSS_SurfaceGPUResource`에서 기본 Factor 0.5의 128·256·512 국소 이동률을 검사한다. 과거 cache ON/OFF 비교 결과는 [[0004_Solver-Validation#검증 — Geometry 전달 기준값 재보정|Geometry 재보정 검증]]의 역사 기록으로 남는다. `MDSS_SceneResources`에서는 Factor 변경 및 Geometry OFF에 따른 안전 시간 간격 변경을 검사한다.

`MDSS_SimulationTransport`는 Fixed·Auto 네 조합과 기본값 ON·OFF를 검사한다. 작은 Transport 상한에서도 Auto OFF의 고정 dt=1/60초가 유지되는지, Auto ON이 전체 고정 구간 대기·마지막 짧은 substep·반복 한도 이후 재개·전환·Reset에서 시간을 보존하는지 확인한다. 현재 정책은 [[0013_Fixed-Timestep-and-Auto-Substepping|Decision 0013]]다.

## Surface Mapping 테스트 사례

OBJ 원본 topology 보존, UV rasterization, chart-local 이웃과 seam 연결 결과를 검증한다. 공통 원칙은 이 문서의 `테스트 구성 가이드`를 따른다.

### 테스트 함수 구성

| 테스트 함수 | 입력 방식 | 검증 범위 |
|---|---|---|
| `TestOBJTopologyPreservation` | UV seam OBJ fixture | render vertex 분리 이후에도 원본 position·UV index와 Surface ID 보존 |
| `TestSingleTriangleRasterization` | 단일 triangle fixture | Valid texel 생성과 barycentric 범위 |
| `TestRegularChartNeighbors` | seam 없는 quad fixture | 공유 UV edge와 chart-local neighbor |
| `TestSeamNeighbors` | UV seam quad fixture | 서로 다른 chart 사이의 양방향 seam neighbor |
| `TestDisconnectedTopology` | 분리된 두 quad fixture | UV와 공간상 가까운 별도 topology의 연결 방지 |
| `TestDeterministicResult` | 동일 fixture를 두 번 변환 | Triangle, Chart, barycentric과 neighbor 결과 일치 |
| `TestMultipleSurfaceRanges` | C++ 입력 직접 구성 | 서로 다른 해상도의 연속 Surface texel range |
| `TestInvalidFixtures` | 오류 OBJ fixture | UV·overlap·non-manifold 입력 거부 |
| `TestInvariantValidation` | 정상 결과를 의도적으로 훼손 | 단방향 neighbor 탐지 |

### 정상 fixture

| 파일 | 목적 | 기대 결과 |
|---|---|---|
| `SingleTriangle.obj` | 기본 rasterization | 한 개 이상의 Valid texel과 정상 barycentric 생성 |
| `QuadNoSeam.obj` | 같은 chart의 triangle 두 개 | 공유 edge를 가로지르는 regular neighbor 생성 |
| `QuadSeam.obj` | 같은 원본 edge, 서로 다른 UV edge | 원본 position topology를 이용한 seam neighbor 생성 |
| `DisconnectedQuads.obj` | 공간상 가깝지만 원본 position이 다른 두 quad | 서로 다른 chart 사이 neighbor 없음 |

### 오류 fixture

| 파일 | 오류 조건 | 기대 결과 |
|---|---|---|
| `DegenerateUV.obj` | UV triangle 면적 0 | mapping 생성 거부 |
| `Overlap.obj` | 서로 다른 triangle이 같은 UV 영역 점유 | 소유 Surface·Triangle·texel을 포함한 overlap 오류 |
| `NonManifold.obj` | 하나의 원본 edge에 triangle 세 개 연결 | non-manifold 오류 |
| `MissingUV.obj` | OBJ face에 `vt` index 없음 | Simulation UV 누락 오류 |

### 실행 및 완료 확인

| 항목 | 기준 |
|---|---|
| 테스트 target | `MDSS_SurfaceMappingTests` |
| CTest 이름 | `MDSS_SurfaceMapping` |
| 실행 명령 | `ctest --test-dir <build-dir> --output-on-failure` |
| GPU 의존성 | Vulkan device나 GPU resource 초기화 없음 |
| 완료 기준 | 정상·오류 fixture와 mapping 불변조건 검증이 모두 통과 |

GPU buffer upload, descriptor와 Solver 연결은 후속 브랜치에서 다룬다.

## Normal Map Transfer 테스트 사례

Simulation texel mapping으로 지정된 triangle/barycentric 좌표에서 Material UV를 얻고, Normal Map tangent-space normal을 mesh-local transfer normal로 바꾸는 CPU 전처리를 검증한다. GPU fixture는 이 결과가 instance별 TransferWeight cache와 Solver flux까지 이어지는지 확인한다.

### CPU fixture

| 사례 | 검증 범위 |
|---|---|
| Flat / tilted normal | 평탄 Map이 geometric normal을 보존하고 tangent-space 기울기가 mesh tangent로 변환됨 |
| Barycentric UV | 삼각형의 barycentric 좌표로 보간한 Material UV에서 sample함 |
| UV chart 경계 | 서로 다른 triangle UV가 각 texel의 Map sample 좌표를 독립적으로 결정함 |
| Repeat 주소 지정 | UV가 `[0, 1]` 바깥에 있어도 repeat sampler와 같은 좌표 wrapping을 적용함 |
| Tangent handedness | mirrored tangent frame에서 bitangent 방향을 반전함 |
| 잘못된 입력 fallback 신호 | 퇴화 tangent, 누락/잘못된 texture, triangle 또는 Surface 불일치는 geometric normal fallback을 요청함 |

테스트 코드는 `Tests/NormalMapTransferTests.cpp`, CPU 변환은 `Source/SurfaceStateSystem/Mapping/NormalMapTransferNormalBuilder.cpp`에 있다. 전처리 fixture는 Vulkan device 없이 실행한다.

### GPU cache 및 Solver fixture

`Tests/SurfaceGPUResourceTests.cpp`의 `TestTransferWeightSolver`는 precomputed transfer normal을 shared geometry에 설정해 다음을 확인한다.

- texel의 transfer normal 내적이 cached `NormalWeight`와 Solver flux를 조절한다.
- non-uniform instance scale에서도 normal inverse-transpose 결과를 쓴다.
- Map normal이 없는 texel은 geometric normal로 fallback한다.
- NormalWeight debug contribution을 끄면 중립값 `1.0`을 쓴다.

### 실행 및 결과

| 항목 | 값 |
|---|---|
| CPU target / CTest | `MDSS_NormalMapTransferTests` / `MDSS_NormalMapTransfer` |
| GPU target / CTest | `MDSS_SurfaceGPUResourceTests` / `MDSS_SurfaceGPUResource` |
| 전체 실행 | `ctest --test-dir <build-dir> --output-on-failure` |
| 로컬 검증 | 2026-09-27: 전체 CTest 5/5 통과, Apple M1에서 데모 1 frame 실행 및 Vulkan validation 오류 없음 |
| 제한 | live Normal Map/UV/tangent hot reload는 구현되어 있지 않다. 재생성은 asset load 수명에 한정된다. |

## Scene Resource 통합 테스트

관련 결정: [[05_Decisions/0007_Scene-State-Registry-and-Shared-Profile-Table|Decision 0007]]

작은 Native GLFW window와 실제 Vulkan context에서 SceneLoader·AssetManager·Renderer·SurfaceStateSystem의 자원 수명을 검증한다. 테스트는 임시 디렉터리에 최소 OBJ·`.SRProfile`·`.SurfaceProfileMap`·`.Scene`을 생성해 실제 loader로 읽고 종료 시 입력 파일을 제거한다. `.Surface` 캐시는 빌드 디렉터리의 `SceneTestCache`에 둔다.

| 조건 | 기대 결과 |
|---|---|
| 외부 Profile을 먼저 캐시한 뒤 Wetness Scene 시작 | 현재 Scene의 Wetness 채널만 Registry에 포함 |
| 다음 Scene의 프로파일 로드 | 활성 Registry의 ID 유지 |
| Wetness → Mud Scene 전환 | Registry를 Mud 채널 하나로 교체 |
| 서로 다른 Mesh·Map 및 역순 로컬 Profile 테이블을 사용하는 mixed Scene | 고유 Profile 두 개를 한 GPU 테이블에 저장하고 모든 descriptor가 같은 Parameters·Supported buffer에 연결 |
| 동일 Profile의 Runtime 튜닝 | 단일 공유 record 갱신 |
| 새 Scene으로 전환 | 이전 숫자 State ID 기반 override 제거 |
| Wetness 지원 Mesh 두 개와 Mud Mesh에 Wetness 주입 | 로컬 Profile 순서와 무관하게 Wetness Mesh에만 입력 반영, instance State 독립 유지 |
| 동일 Scene 해상도 변경 | Registry ID와 Profile 튜닝 유지 |
| storage-buffer range를 초과하는 Scene 로드 | 이전 Registry·GPU handle·튜닝 값 복원 |
| 빈 Scene 및 이전 Scene으로 재전환 | 빈 Registry 처리 및 실패한 로드의 캐시 State 제외 |
| 생성·Compute·파괴 | Vulkan validation error 없음 |

`Tests/SurfaceDebugRenderingTests.cpp`는 같은 Vulkan context에서 실제 `SurfaceDebug.vert`·`SurfaceDebug.frag`를 float32 RGBA offscreen attachment에 렌더링하고 GPU 출력 픽셀을 읽는다. 삼각형 비율 계산을 CPU에서 반복하는 대신 알려진 단위 정사각형의 표시 결과를 검증한다.

| 렌더 조건 | 기대 결과 |
|---|---|
| 단위 정사각형, 기준 `1 / 256²`, 해상도 128·256·512 | 각각 빨강·초록·파랑 |
| 동일 화면 크기를 유지한 월드 2배 확대 | 텍셀당 면적 4배로 빨강 |
| 비균일 scale `(2, 0.5, 3)`의 XY 표면 | 표면 면적은 같으므로 초록 |
| 원근 투영 및 비스듬한 Camera | 기준 면적 색 유지 |
| UV의 두 축을 1/2로 축소 | 텍셀당 면적 4배로 빨강 |
| 반전 UV / 퇴화 UV | 정상 면적 / 계산 불가 분홍색 |
| 격자 8×8 → 16×16 변경 | 개별 텍셀 경계 위치를 유지하고 묶음 경계 변경 |
| 화면에서 구분 불가능한 격자 | 평균 중립색으로 축소 패턴 숨김 |
| invalid simulation texel | 두 기하 진단 뷰는 원본 mesh를 계속 표시 |
| 실제 Renderer·ImGui frame에서 Grid·Area·Transfer Weight·Meso Displacement 표시 | pipeline·UI context 정상 동작, Vulkan validation error 없음 |
| 해상도 128 → 256 → 128 | Grid 묶음 크기와 Area 색 기준 유지 |

Scene 전환은 Loader와 Renderer API로 실행한다. UI 파일 다이얼로그의 클릭 동작은 이 CTest의 검증 범위에 포함하지 않는다. Native window 또는 Vulkan context를 만들 수 없는 환경에서는 return code 77로 skip한다.

```sh
cmake --build Build --parallel 4
ctest --test-dir Build --output-on-failure
```

2026-09-29 로컬 실행에서 Scene Resource 통합 테스트를 포함한 CTest 7개가 통과했다.
