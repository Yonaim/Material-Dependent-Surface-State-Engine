# 시뮬레이션 Asset과 Profile

> Asset 계약의 핵심은 같은 입력이 같은 texel graph·Profile 배치를 만들고, Profile 수치 변경은 정적 Geometry cache를 무효화하지 않는다는 점이다.

## 입력과 수명

| 데이터 | 현재 역할 |
|---|---|
| `.Scene` | Static Mesh object, 선택적 `.SurfaceProfileMap`·`.DemoAnim`, 해상도, 시작 Camera·Transform, `initialContacts` |
| OBJ·MTL·Texture | Macro mesh, Simulation UV, 렌더 재질과 Meso 복원용 Normal Map |
| `.SurfaceProfileMap` | Surface/Material별 `.SRProfile` 배치. 현재 한 Surface의 valid texel에는 같은 Profile을 확장 |
| `.SRProfile` | State별 반응 계수와 Transition 선언. 현재 schema version 4 |
| `.Surface` | 정적 CPU texel Geometry·이웃·Profile index의 해상도별 생성 캐시. 직접 편집하지 않음 |

`surfaceProfileMap`이 없는 Scene object는 렌더링 전용이다. 같은 Mesh·Map·해상도 조합의 instance는 Runtime Surface Data를 공유하지만 State는 별도로 가진다. `simulationResolution`은 Scene 전체에 128·256·512 중 하나를 적용하며 생략하면 256이다. [[02_Architecture/02_Contracts/02_Geometry|Geometry]], [[02_Architecture/02_Contracts/04_GPU-Layout|GPU 데이터]]

`.DemoAnim`은 선택적 버전형 JSON이다. Transform·Camera keyframe 외에 `contactEvents`와 시각화용 weather cue를 담을 수 있다. 이벤트가 있는 clip은 loop하지 않으며, Application이 이벤트 시점에 내부 Contact를 제출한다. Animation과 Solver의 재생 제어는 독립적이다. 이 기능의 상세 JSON은 `Source/Scene/DemoAnimation.cpp`와 `Assets/Animations`의 현재 예제를 기준으로 한다.

## Profile과 Registry

현재 Scene이 참조하는 고유 Profile의 `states` key를 정규화해 `TSurfaceStateRegistry`를 만든다. Registry가 이름을 동적 channel index로 연결하며 별도 전역 State enum은 없다. Profile table은 Scene에서 공유하고 각 texel은 `ProfileIndex`로 응답값을 찾는다. Profile이 선언한 Transition은 로드하지만 Solver에서는 아직 실행하지 않는다. 파라미터의 의미·범위는 [[02_Architecture/02_Contracts/00_State-and-Profile|State 계약]]이 기준이다.

Version 4의 전달·보유 키는 `saturationSpreadFactor`, `gravityFlowFactor`, `cavityExitResistanceFactor`, `cavityDecayProtectionFactor`다. Version 3은 이전 키로만 읽고 서로 다른 version의 키를 섞으면 실패한다. Version 1·2는 지원하지 않는다.

## `.Surface` cache

```text
Cache/Surface/<MeshName>_<MeshMapIdentity>/<MeshName>_<Resolution>.Surface
```

`MeshMapIdentity`는 정규화한 절대 Mesh·Map 경로 쌍의 64-bit FNV-1a 값이다. 입력 fingerprint는 Mesh의 파싱된 Geometry·UV·topology, Normal Map bytes, Profile Map의 경로·배치, 해상도와 전처리 version을 포함한다. `.SRProfile`의 반응 수치와 Transition은 포함하지 않으므로 그 변경만으로 정적 Geometry를 재생성하지 않는다.

Cache hit는 CPU mapping·meso 복원을 생략한다. Miss·stale·손상 파일은 원본 입력으로 재생성한다. Cache 저장 실패는 경고 후 생성된 Runtime 데이터를 사용하지만 원본 Asset 오류는 load 실패다. 파일은 format version·fingerprint·payload checksum·원소 개수를 검사하며 임시 파일을 완성 파일로 교체한다. 현재 format version 4, preprocess version 4, texel record 140 B다. 필드·좌표계는 [[02_Architecture/02_Contracts/02_Geometry#해상도별 정적 Geometry 캐시|Geometry cache 계약]]을 따른다.

Cache hit여도 GPU 자원과 instance State는 준비한다. 해상도 변경은 해당 variant를 선택한 뒤 GPU 자원을 다시 만들고 State·입력을 초기화한다. 따라서 cache hit와 시뮬레이션 결과 유지 여부를 혼동하지 않는다.
