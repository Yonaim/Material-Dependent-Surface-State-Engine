# 시뮬레이션 Asset과 Profile

> Asset 계약의 핵심은 같은 입력이 같은 texel graph·Profile 배치를 만들고, Profile 수치 변경은 정적 Geometry cache를 무효화하지 않는다는 점이다.

## 입력과 수명

| 데이터 | 현재 역할 |
|---|---|
| `.Scene` | 현재 에디터의 `SurfaceScene` v1은 프로파일·차트·초기 상태와 장면 필드를 지정한다. 연구 입력인 `TScene` v2는 `surfaceProfile`과 선택적 `surfaceProfileOverrides`로 메시 프로파일을 할당한다. |
| OBJ·MTL·Texture | Macro mesh, Simulation UV, 렌더 재질과 Meso 복원용 Normal Map |
| `.SurfaceProfileMap`, `.SRProfile` | 보관된 `TScene` v1과 연구·테스트 입력용 호환 형식 |
| `.SurfaceProfile` | 현재의 `SurfaceProfile` v1로 물리 상태·수송·규칙·필드를 연결한다. 연구용 메시 로더는 `SurfaceResponseProfile` v4/v5도 읽을 수 있다. |
| `.Surface` | 정적 CPU texel Geometry·이웃·Profile index의 해상도별 생성 캐시. 직접 편집하지 않음 |
| `Assets/Simulation/` | YAML 본문의 `.PhysicalMaterial`, `.Interaction`, `.SurfaceProfile`. 표면층과 바탕 상태는 `.SurfaceProfile`에서 정의한다. [Surface Simulation 계약](08_Surface-Simulation.md#00-경계와-단위) |

`TScene` v2의 프로파일 참조는 공용 에셋 ID 또는 씬에 상대적인 `.SurfaceProfile` 경로다. 기본 할당은 메시의 모든 표면에 적용된다. 오버라이드는 표면 ID별로 기본값을 덮어쓰며 `null`은 해당 표면을 시뮬레이션에서 제외한다. 캐시는 프로파일 계수 대신 정규화한 할당과 메시·격자 입력을 식별한다.

프로파일 할당이 없는 표면은 렌더링 전용이다. 같은 Mesh·Map·해상도 조합의 instance는 Runtime Surface Data를 공유하지만 State는 별도로 가진다. `simulationResolution`은 Scene 전체에 128·256·512 중 하나를 적용하며 생략하면 256이다. [[02_Architecture/01_Contracts/03_Geometry|Geometry]], [[02_Architecture/01_Contracts/05_GPU-Layout|GPU 데이터]]

연구용 `.DemoAnim`은 선택적 버전형 JSON이다. Transform·Camera keyframe 외에 `contactEvents`와 시각화용 weather cue를 담을 수 있다. 이벤트가 있는 clip은 loop하지 않으며, Application이 이벤트 시점에 내부 Contact를 제출한다. 연구 재현 입력은 `Scripts/Research/Fixtures`에 있다. 현재 데모의 재생·카메라·물질 주입은 `Source/Application/Editor/SurfaceSimulation/SurfaceSceneBuilder.cpp`가 스텝 시간으로 계산한다.

## Profile과 Registry

현재 Scene이 참조하는 고유 Profile의 `states` key를 정규화해 `FSurfaceStateRegistry`를 만든다. Registry가 이름을 동적 channel index로 연결하며 별도 전역 State enum은 없다. Profile table은 Scene에서 공유하고 각 texel은 `ProfileIndex`로 응답값을 찾는다. version 5는 `reactions`로 전이·상호작용을 선언한다. version 3/4 Transition은 기존 의미의 조건부 생성 규칙으로 변환해 실행한다. [[02_Architecture/01_Contracts/06_Reactions|Reaction 계약]]을 따른다. 파라미터의 의미·범위는 [[02_Architecture/01_Contracts/01_State-and-Profile|State 계약]]이 기준이다.

Version 4의 전달·보유 키는 `saturationSpreadFactor`, `gravityFlowFactor`, `cavityExitResistanceFactor`, `cavityDecayProtectionFactor`다. Version 3은 이전 키로만 읽고 서로 다른 version의 키를 섞으면 실패한다. Version 1·2는 지원하지 않는다.

## `.Surface` cache

```text
Cache/Surface/<MeshName>_<MeshMapIdentity>/<MeshName>_<Resolution>.Surface
```

`MeshMapIdentity`는 정규화한 절대 Mesh·Map 경로 쌍의 64-bit FNV-1a 값이다. 입력 fingerprint는 Mesh의 파싱된 Geometry·UV·topology, Normal Map bytes, Profile Map의 경로·배치, 해상도와 전처리 version을 포함한다. `.SRProfile`의 반응 수치와 Transition은 포함하지 않으므로 그 변경만으로 정적 Geometry를 재생성하지 않는다.

Cache hit는 CPU mapping·meso 복원을 생략한다. Miss·stale·손상 파일은 원본 입력으로 재생성한다. Cache 저장 실패는 경고 후 생성된 Runtime 데이터를 사용하지만 원본 Asset 오류는 load 실패다. 파일은 format version·fingerprint·payload checksum·원소 개수를 검사하며 임시 파일을 완성 파일로 교체한다. 현재 format version 4, preprocess version 4, texel record 140 B다. 필드·좌표계는 [[02_Architecture/01_Contracts/03_Geometry#해상도별 정적 Geometry 캐시|Geometry cache 계약]]을 따른다.

Cache hit여도 GPU 자원과 instance State는 준비한다. 해상도 변경은 해당 variant를 선택한 뒤 GPU 자원을 다시 만들고 State·입력을 초기화한다. 따라서 cache hit와 시뮬레이션 결과 유지 여부를 혼동하지 않는다.
