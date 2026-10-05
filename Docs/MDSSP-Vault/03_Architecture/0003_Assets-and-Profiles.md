# 에셋과 Surface Response Profile

> **한 줄 요약:** 이 문서는 데이터의 파일 직렬화와 Asset 연결 관계만 정의한다.

상태: **현재 Asset·Profile 파일 구조**
근거: [[06_Assets/Documents/0003_Asset-Structure.pdf|에셋 구조]]

---

이 문서는 데이터의 **파일 직렬화와 Asset 연결 관계**만 정의한다. SRProfile 파라미터의 의미와 범위는 [[03_Architecture/0002_Surface-State|표면 상태와 데이터 구조]]를 기준으로 한다.

## 파일 형식

| 용도 | 확장자 | 비고 |
|---|---|---|
| OBJ Mesh | `.obj` | 정점, UV, Normal, Face, Surface별 Material 할당 |
| Render Material | `.mtl` | OBJ Surface의 외관용 Material |
| Texture | `.png`, `.jpg` 등 | Albedo, Normal 등 |
| Scene | `.Scene` | JSON 형식. 배치, Asset 연결 관계, 시작 카메라 및 시뮬레이션 해상도 |
| Demo Animation | `.DemoAnim` | JSON 형식. 버전, 길이, 반복 설정, object transform 및 선택적 camera keyframe. `.Scene`의 선택적 참조이며 구현됨 ([[05_Decisions/0020_Scene-Referenced-Demo-Animation|Decision 0020]]) |
| Surface Response Profile | `.SRProfile` | JSON 형식. Surface State 반응 데이터 |
| Surface Profile Distribution | `.SurfaceProfileMap` | JSON 형식. Scene object가 경로를 선택하며, Surface별 SRProfile 할당을 기록 |
| Surface Preprocessing Cache | `.Surface` | 해상도별 최종 CPU Geometry·Texel 관계·Profile map을 저장하는 생성 바이너리 캐시 |
| Runtime Surface Data | 메모리 객체 | 유효한 `.Surface`를 로드하거나 전처리해 생성하고, 같은 Mesh·Map·해상도의 instance끼리 공유 |

Scene files select Mesh, Material, and Surface Profile assets.

### `.Scene`과 `.DemoAnim` 참조 — 구현

`.Scene`은 선택적으로 `.DemoAnim` 경로를 기록한다. 경로는 Scene 파일이 있는 디렉터리를 기준으로 해석한다. `.DemoAnim`은 버전 필드가 있는 선언형 JSON이며, 접촉 입력이나 Simulation Run/Pause 명령은 포함하지 않는다.

```json
{
  "version": 1,
  "durationSeconds": 6.0,
  "loop": false,
  "tracks": [
    {
      "target": "bunny",
      "property": "rotation",
      "interpolation": "slerp",
      "keys": [
        { "time": 0.0, "value": [0.0, 0.0, 0.0, 1.0] },
        { "time": 3.0, "value": [0.0, 0.0, 0.7071, 0.7071] }
      ]
    }
  ]
}
```

- `target`는 Scene 배열 순서가 아닌 안정적인 object ID를 가리킨다. `.Scene` object의 `id`는 유일해야 하며, `camera`는 예약된 대상이다.
- transform keyframe은 위치·회전·크기를 지원한다. 회전은 quaternion으로 저장하고 Slerp로 보간한다.
- camera keyframe은 선택 항목이며 `target: "camera"`에 `position` 또는 `target` 속성을 쓴다. 위치·크기·카메라는 `linear` 또는 `step`, 회전은 `slerp` 또는 `step`을 허용한다. 첫 key는 0초이고 시간은 엄격히 증가해야 한다.
- 애니메이션 재생·일시정지와 Solver Run·Pause·Step은 별도 UI 제어와 시간 상태를 가진다. 한쪽을 조작해도 다른 쪽을 자동으로 시작·정지하지 않는다.
- 공통 C++ 재생기는 `.DemoAnim`을 읽는다. Python/Lua 실행 환경은 두지 않는다.

### Scene과 Asset 참조

```mermaid
flowchart LR
  SceneFile[".Scene"] --> Loader["TSceneLoader"]
  Loader --> Instance["Scene object / Mesh instance"]
  Instance --> Mesh[".obj + .mtl"]
  Instance --> Map[".SurfaceProfileMap"]
  Mesh --> Textures["Albedo / Normal textures"]
  Map --> Paths[".SRProfile paths"]
  Paths --> Profiles["TSRProfileAsset collection"]
  Profiles --> Registry["State Registry"]
```

Mesh, Normal Map, Profile Distribution, and resolution determine the reusable runtime data.

### Cache와 Runtime Surface Data

```mermaid
flowchart LR
  Inputs["Mesh + Normal Map + Profile Distribution"] --> Lookup["Cache lookup"]
  Lookup -->|valid| Cache[".Surface cache"]
  Lookup -->|missing or stale| Build["Runtime Preprocessor"]
  Build --> Cache
  Build --> Runtime["Runtime Surface Data"]
  Cache --> Runtime
  Runtime --> Geometry["Shared texel geometry"]
```

Profile assets supply the response table, while each texel refers to it by index.

### Profile과 State Registry 조회

```mermaid
flowchart LR
  Profiles[".SRProfile assets"] --> Registry["State Registry"]
  Profiles --> Table["Profile parameters"]
  Runtime["Runtime Surface Data"] --> TexelMap["Texel → ProfileIndex"]
  Registry --> Table
  TexelMap --> Table
```

## `.Scene` 시뮬레이션 해상도

최상위 `simulationResolution`은 Scene 내 모든 simulated Surface에 적용할 정사각형 grid의 한 변 크기다. 정수 128·256·512만 허용하며 필드가 없으면 256을 사용한다. 기존 Scene의 활성 해상도는 새 파일의 기본값에 영향을 주지 않는다.

- `TSceneLoader`는 해상도를 검증한 뒤 `LoadSurfaceData`에 전달한다.
- 후보 Scene을 읽는 동안 기존 활성 해상도를 유지한다. Renderer의 GPU 자원 교체가 성공하면 새 해상도를 적용한다.
- UI에서 바꾼 해상도는 현재 `TScene`에 반영하며, `Save Scene`이 파일에 기록한다.
- 해상도 전환 계약을 따른다 (Simulation Resolution Presets).

## `.Scene` 시작 카메라

최상위 `camera`는 씬 로드 및 `Reset Camera View`에서 사용할 시작 위치, 시선 대상, 수직 FOV를 저장한다. 필드가 없는 기존 Scene은 기본 카메라 설정을 사용한다.

```json
"camera": {
  "position": [2.0, -3.3, 2.0],
  "target": [0.0, 0.0, 0.0],
  "verticalFieldOfViewDegrees": 50.0
}
```

`Save Scene`은 현재 카메라 설정을 기록하고, 성공하면 현재 설정을 재시작 기준으로 저장한다.

## Surface와 Profile의 관계

Render Material과 Surface Response Profile은 서로 다른 책임을 가진다.

- Render Material은 외관을 정의한다.
- `.SRProfile`은 State 반응 파라미터와 Transition을 정의한다.
- 현재는 각 Surface/Material 할당에 SRProfile 하나를 지정하며 해당 Surface의 모든 valid texel이 그 Profile을 사용한다.

Runtime 전처리 결과에는 유효한 UV texel마다 `ProfileIndex` 하나를 저장하는 dense Profile Map이 포함된다.

- 각 texel은 이 인덱스로 별도 Profile table의 반응 파라미터를 조회한다.
- 현재는 Surface/Material 할당에 Profile 하나를 지정하고 해당 Surface의 texel마다 같은 인덱스를 확장한다.
- Dense map은 조회용 표현이며 Surface 내부를 여러 Profile 영역으로 나누는 authoring 기능은 아니다. 세분화는 후속 기능이다.
- Profile Map은 `.Surface`에 정적 데이터로 저장하고 Runtime에서는 공유 Geometry의 일부로 로드한다.
- Profile table의 경로와 순서는 저장하지만 `.SRProfile` 반응 파라미터와 Runtime Asset handle은 저장하지 않는다. (Texel별 Profile Index: Texel Profile Index Map)

```text
UV Texel
├── Render Material assignment (rendering)
└── SurfaceProfileMap
    └── ProfileIndex → SRProfile response data
```

Runtime Surface Data는 Mesh, Normal Map, Profile Distribution, Simulation 해상도에 대응하는 정적 데이터다.

- `.Scene`의 각 object가 사용할 `.SurfaceProfileMap`을 지정한다.
- 같은 Mesh·Map·해상도 조합의 instance는 Runtime 메모리 결과를 공유한다.
- 실행 간에는 `.Surface`로 최종 CPU 전처리 결과를 재사용한다.
- 다른 Map이나 해상도는 별도 cache variant로 유지한다. 경로 계약을 따른다 (Scene Profile Distribution Reference).

데이터 범위는 다음과 같다.

| Runtime Surface Data에 포함 | Runtime Surface Data에 포함하지 않음 |
|---|---|
| 유효성, Normal, Virtual Height, Curvature/Concavity 등 정적 Geometry 값 | 시간에 따라 변하는 instance별 State |
| 최종 Neighbor index, Chart 및 UV seam 연결. Distance와 간선별 Height Difference는 저장하지 않음 | `.SRProfile`의 반응 파라미터와 Transition |
| Texel → `ProfileIndex` map | Instance별 `SurfaceInstanceStateData` |

초기 `.Surface` 설계는 Mesh당 단일 파일을 두었고, 이후 Runtime 전처리만 수행하도록 단순화했다. 현재는 시작 시 전처리 비용을 줄이기 위해 해상도별 캐시를 유지한다. 현재 설계를 따른다 ([[05_Decisions/0006_Resolution-Surface-Cache|Decision 0006]]). 이전 결정 이력은 보존한다 (Surface Preprocessed Asset, Runtime Surface Preprocessing).

## `.Surface` 캐시 경로와 유효성

`.Surface`는 직접 편집하는 authoring 입력이 아니라, `BuildMesoGeometry()`까지 완료한 최종 CPU `TSharedSurfaceGeometryData`와 texel별 Profile map의 캐시다. `.Scene`은 계속 Mesh와 `.SurfaceProfileMap` 원본을 참조하며 캐시 파일 경로를 지정하지 않는다.

```text
Cache/Surface/<MeshName>_<MeshMapIdentity>/<MeshName>_<Resolution>.Surface
예: Cache/Surface/Cube_<identity>/Cube_128.Surface
    Cache/Surface/Cube_<identity>/Cube_256.Surface
    Cache/Surface/Cube_<identity>/Cube_512.Surface
```

- `MeshMapIdentity`는 정규화한 절대 Mesh·Map 경로 쌍의 64-bit FNV-1a 값이다.
- Mesh·Map 조합별 디렉터리를 사용해 같은 이름의 Mesh나 다른 Map이 충돌하지 않게 한다.
- 입력 내용이 바뀌면 같은 해상도 파일만 재생성하고 다른 해상도 파일은 유지한다.
- 초기 실행과 해상도 변경 시 필요한 variant만 만든다. 모든 해상도를 한꺼번에 전처리하지 않는다.
- Cache root는 CMake의 `MDSS_SURFACE_CACHE_DIR`로 `Cache/Surface/`에 지정하며 Git에서 제외한다. 프로젝트 경로를 옮기면 캐시를 다시 생성한다.

| 유효성 입력 | 판정에 포함하는 내용 |
|---|---|
| 파싱된 Mesh | 정점 Position·Normal·UV·Tangent/handedness, triangle의 render vertex·원본 Position/UV index·Surface ID |
| Normal Map | Surface별 정규화 경로와 원본 파일 bytes. 같은 파일은 fingerprint 계산 중 한 번 읽음 |
| Profile Distribution | `.SurfaceProfileMap` 경로와 파일 bytes, Surface별 Profile index, 순서 있는 Profile table 경로 |
| Simulation grid | Surface별 ID·Width·Height. 현재 프리셋은 128·256·512, 기본값은 256 |
| 생성 규칙 | `PreprocessVersion`. Mapping·sample·높이 적분·미분 규칙 변경 시 증가 |
| 파일 표현 | 별도의 `FormatVersion`, magic, payload checksum, 배열 개수와 길이 |

- 입력 fingerprint는 64-bit FNV-1a이며 cache 무효화에 사용한다. 파일 시각이나 크기만으로 입력 동일성을 판정하지 않는다.
- `.SRProfile` 파라미터·Transition·`states`는 Geometry fingerprint에 포함하지 않는다. 매 Runtime에 Profile loader와 `TSurfaceStateRegistry`로 읽는다.
- Profile 경로·순서·배치가 같으면 반응 수치나 Registry channel 종류가 달라져도 Geometry cache를 재사용한다.

## 캐시 로드·재생성 및 파일 계약

1. 이미 로드한 Mesh·Map·해상도 조합은 Runtime 메모리 결과를 공유한다.
2. 새 조합은 원본 Map과 Profile을 로드하고 Surface·Normal Map 입력으로 fingerprint를 계산한다.
3. 유효한 `.Surface`가 있으면 최종 Geometry를 복원한다. Mapping·Normal Map CPU sample·PCG 높이 적분·곡률 계산은 수행하지 않는다.
4. 파일이 없거나 stale·손상된 경우 기존 CPU 전처리 경로로 Geometry를 생성한 뒤 캐시를 저장한다.
5. 복원하거나 생성한 Geometry에서 GPU 버퍼와 instance별 TransferWeight·State 자원을 준비한다.

- File format은 version 4, PreprocessVersion은 2다. `AreaVector`를 포함하며 이전 `.Surface` cache는 재생성한다.
- 정수는 little-endian `uint32`/`uint64`, 실수는 IEEE-754 `float32`로 기록한다. C++ 구조체 메모리는 그대로 덤프하지 않는다.
- Header: magic, format/preprocess version, input fingerprint, Surface/Profile/triangle/texel count, payload checksum
- Payload: Surface 정의, 순서 있는 Profile 경로, texel 레코드
- Texel record는 padding 없이 140 byte다. 세부 필드는 [[03_Architecture/0004_Surface-Geometry#해상도별 정적 Geometry 캐시|형상 정보의 캐시 계약]]을 따른다.

- 현재 입력에 맞는 예상 파일 길이와 개수를 먼저 확인한다.
- Checksum, Surface·Profile table 대응, 유한한 형상 값, normal 유효성, invalid sentinel, 이웃 범위·중복·양방향 연결을 검사한 뒤 Runtime에 등록한다.
- Cache 오류는 재생성 사유로 로그에 남긴다. 원본 Asset 오류는 load 실패다.

- 같은 디렉터리의 고유한 임시 파일에 저장하고 flush·close 후 rename으로 완성 파일을 교체한다.
- 저장 실패 시 임시 파일을 제거하고 기존 완성 파일을 보존한다.
- Cache 저장 실패는 경고를 남기고 이미 생성한 Runtime Geometry로 계속 실행한다. Cache는 삭제해도 재생성되며, 저장 실패는 simulation 입력 실패가 되지 않는다.

현재 구현은 동기 load/save와 자동 재생성이다. 별도 Asset Build 도구, 압축, streaming IO, 캐시 용량 제한/자동 정리 및 source Asset hot reload는 후속 기능이다. 해상도 전환 시 CPU 캐시가 hit해도 GPU 자원 재생성과 State 초기화 규칙을 유지한다 (Simulation Resolution Presets).

## State Registry

- `.SRProfile`의 `states` key가 State 이름을 제공한다.
- 현재 Scene의 Runtime Profile table에 참조된 Profile만으로 `TSurfaceStateRegistry`를 구성한다.
- 문자열 State 이름은 `TStateId`/`ChannelIndex`로 변환한다.
- 이전 Scene이나 실패한 load의 자산은 cache에 남아도 활성 Registry에 포함하지 않는다.
- Scene 전환과 공유 GPU Profile table은 해당 규칙을 따른다 ([[05_Decisions/0007_Scene-State-Registry-and-Shared-Profile-Table|Decision 0007]]). 이름 정규화와 데이터 주도 처리는 Registry 계약을 따른다 (Dynamic State Registry).

## `.SRProfile` 예시

Profile `stateCapacity`와 `decayRate`는 고정 기준 면적 `1/256² world-length²`에 대한 값이다. 런타임에서 실제 월드 texel 면적 비율을 곱한다. 기준 면적은 Low·Medium·High 선택과 함께 바뀌지 않는다. [[05_Decisions/0009_Texel-Area-and-State-Amounts|Decision 0009]]

현재 schema는 **version 4**다. `saturationSpreadFactor`, `gravityFlowFactor`, `cavityExitResistanceFactor`, `cavityDecayProtectionFactor`를 사용한다. 기존 version 3 파일의 이전 키도 version 3으로 선언된 경우 읽을 수 있다. 서로 다른 version의 키를 한 파일에 섞으면 Loader가 오류를 낸다. Loader는 version 1·2를 거부한다. 정규화된 전달 계수와 기준 Rate는 [[05_Decisions/0008_Normalized-Transport-Factors|Decision 0008]], [[05_Decisions/0012_Geometry-Rate-Recalibration|Decision 0012]]을 따른다. 두께값의 의미는 State Thickness Per Amount, 키 이름 변경은 [[05_Decisions/0024_Transport-Role-Names-and-Curvature-Removal|Decision 0024]]에 기록한다.

아래 수치는 **튜닝 전 예시값**이며, Profile이 여러 State 응답을 정의할 수 있음을 보여준다. 예시 State 이름은 고정된 전역 채널 목록이 아니다.

```json
{
  "type": "SurfaceResponseProfile",
  "version": 4,
  "name": "Brick",
  "states": {
    "wetness": {
      "stateCapacity": 1.0,
      "inputFactor": 0.75,
      "saturationSpreadFactor": 0.40,
      "gravityFlowFactor": 0.0005,
      "decayRate": 0.06,
      "cavityDecayProtectionFactor": 0.50,
      "accumulationFactor": 0.0,
      "cavityFillFactor": 0.0,
      "thicknessPerAmount": 0.01
    },
    "heat": {
      "stateCapacity": 1.0,
      "inputFactor": 0.30,
      "saturationSpreadFactor": 0.30,
      "gravityFlowFactor": 0,
      "decayRate": 0.10,
      "cavityDecayProtectionFactor": 0.0,
      "accumulationFactor": 0.0,
      "cavityFillFactor": 0.0,
      "thicknessPerAmount": 0.01
    },
    "burn": {
      "stateCapacity": 1.0,
      "inputFactor": 0.0,
      "saturationSpreadFactor": 0.0,
      "gravityFlowFactor": 0,
      "decayRate": 0.01,
      "cavityDecayProtectionFactor": 0.0,
      "accumulationFactor": 0.0,
      "cavityFillFactor": 0.0,
      "thicknessPerAmount": 0.01
    },
    "mud": {
      "stateCapacity": 1.0,
      "inputFactor": 0.65,
      "saturationSpreadFactor": 0.04,
      "gravityFlowFactor": 0.0008,
      "decayRate": 0.04,
      "cavityDecayProtectionFactor": 0.80,
      "cavityExitResistanceFactor": 0.85,
      "accumulationFactor": 0.65,
      "cavityFillFactor": 0.60,
      "thicknessPerAmount": 0.01
    }
  },
  "transitions": [
    {
      "source": "heat",
      "target": "burn",
      "threshold": 0.7,
      "transitionRate": 0.2
    }
  ]
}
```

## `.Scene` 연결 예시

- 각 Scene object는 Mesh와 선택적인 Profile Distribution 파일을 지정한다.
- 두 경로는 Scene 파일 디렉터리 기준 상대 경로다.
- 같은 Mesh를 쓰는 Scene마다 다른 `.SurfaceProfileMap`을 선택할 수 있다.
- Map 내부 `.SRProfile` 경로는 Map 파일 위치를 기준으로 해석한다.

```json
{
  "type": "TScene",
  "version": 1,
  "objects": [
    {
      "mesh": "../Models/clothes.obj",
      "surfaceProfileMap": "../SurfaceProfileMaps/clothes_default.SurfaceProfileMap",
      "transform": {
        "position": [0.0, 0.0, 0.0],
        "rotationDegrees": [0.0, 0.0, 0.0],
        "scale": [1.0, 1.0, 1.0]
      }
    }
  ]
}
```

- Simulation grid 해상도는 `.Scene`의 `simulationResolution`에 저장한다.
- Simulation 탭에서 Low(128), Medium(256), High(512)를 선택한다. 값이 없으면 Medium을 사용한다.
- Scene 내 모든 simulated Surface에 같은 해상도를 적용한다.
- 해상도별 cache를 재사용하며, 전환 시 State를 초기화한다 (Simulation Resolution Presets).

`.Scene`에서 `surfaceProfileMap`을 생략한 object는 렌더링 전용이며 Runtime Surface simulation data를 만들지 않는다. Scene 경로와 Profile map 참조 정책을 따른다 (Scene Profile Distribution Reference).

Simulation UV는 렌더링 UV와 논리적으로 분리한다. UV 생성·검증과 현재 구현 범위는 [[03_Architecture/0004_Surface-Geometry#Simulation Mapping|Surface Simulation Mapping]]을 본다.
