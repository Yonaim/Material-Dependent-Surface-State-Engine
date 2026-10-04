# 셰이더 변수명과 의미

> **한 줄 요약:** 현재 셰이더 변수명을 기준으로 CPU/GPU 인터페이스, 렌더링 전달값, 자주 쓰는 계산 용어의 의미를 정리한다.

상태: **현재 코드 기준 참고 문서** · 범위: `Shaders/`의 주요 공용 변수 및 외부 인터페이스

이 문서는 변수명과 실제 코드에서의 의미를 대조하기 위한 이름 사전이다. 설계 정의는 관련 Architecture 문서를 기준으로 하며, 이름이 의미를 완전히 설명하지 못하거나 오해할 수 있는 항목은 `검토 메모`에 기록한다. Shader 내부의 짧은 임시 변수까지 전부 나열하지 않고, CPU와 맞물리는 데이터와 여러 파일에서 공유되는 용어를 우선 정리한다.

## Solver 공용 데이터

정의는 [`SurfaceSolverCommon.glsl`](../../../../Shaders/Simulation/SurfaceSolver/SurfaceSolverCommon.glsl), CPU 대응 push constant는 [`SurfaceGPUResourceLayout.h`](../../../../Source/SurfaceState/GPU/SurfaceGPUResourceLayout.h)에 있다. 버퍼의 binding 번호와 원소 형식은 CPU/GPU 계약이므로 이름·필드 순서·stride를 함께 유지해야 한다.

### Push constant `Solver`

| 셰이더 이름 | 의미 |
|---|---|
| `DeltaTime` | 이번 solver step의 시간 간격(초). |
| `StateChannelCount` | 등록된 State 채널 수. 채널 종류를 고정 목록으로 뜻하지 않는다. |
| `LocalTexelCount` | 현재 instance에서 처리하는 texel 수. |
| `Flags` | solver 동작 옵션 bit field. 코드의 bit test와 대응한다. |
| `GravityWorld` | 월드 좌표 중력 벡터. |
| `ModelLinearColumns` | instance 변환의 선형부 열 벡터. 위치 차이를 world 방향으로 변환한다. |
| `NormalMatrixAndUpColumns` | xyz에 normal matrix 열, 각 열의 w에 월드 up 벡터의 x/y/z를 싣는다. 이름만 보면 `w`의 별도 용도를 알기 어렵다. |

### Solver 버퍼

| 변수명 | 용도 |
|---|---|
| `TexelSurfaceIndices` | texel이 속한 Surface ID. |
| `TexelProfileIndices` | texel에 대응하는 Profile 인덱스. |
| `Positions`, `Normals` | 전처리된 texel 위치와 법선. |
| `GeometryScalars` | texel별 `MesoVirtualHeight`, `ConcavityWeight`, `MesoMeanCurvature`, `MesoGaussianCurvature`. |
| `NeighborIndices` | texel별 8방향 이웃 texel 인덱스. |
| `ProfileParameters` | Profile × channel 파라미터 레코드. 각 레코드는 `CapacityInputAndTransfer`, `DecayAndGeometry`, `AccumulationThickness` vec4 세 개다. |
| `ProfileSupported` | 해당 Profile이 해당 channel을 지원하는지 나타내는 값. |
| `CurrentState`, `NextState` | step 입력 State와 계산된 출력 State. 저장 순서는 texel-major/channel-minor다. |
| `OutgoingFluxScale` | source texel/channel의 총 유출량을 보유량 이하로 제한하는 배율. |
| `InputDelta` | 이번 step에 소비할 외부 입력량. |
| `TransferWeights` | source texel과 8방향 이웃 사이의 전달 가중치. |
| `RawOutgoingBuffer` | source texel/channel의 방향별 raw flux 총합. 이름의 `Buffer` 접미사는 다른 버퍼 변수와 표기가 다르다. |
| `RawFluxBuffer` | source texel/channel/direction별 raw flux 캐시. `UseRawFluxCache`가 켜진 경로에서 사용한다. |
| `MesoNormals` | Meso 형상이 반영된 법선. |
| `ReverseNeighborDirectionIndices` | 이웃에서 현재 texel로 돌아오는 방향 인덱스를 찾는 packed 값. |
| `WorldTexelAreas` | texel의 월드 면적. |
| `DynamicGeometry` | 동적 위치·평균 이웃 거리와 local normal·마지막 생성 높이를 묶어 저장한다. |
| `AccumulationHeights` | accumulation 높이와 같은 GPU 할당 안의 dirty/workgroup/indirect-dispatch 메타데이터. 따라서 이름은 할당 전체의 내용을 다 드러내지 않는다. |
| `DynamicConcavityWeights` | 현재 동적 형상에서 사용하는 concavity weight. |

`TSurfaceGPUProfileParameters`의 field 의미는 profile 직렬화/상태 문서를 기준으로 확인한다. Shader에서 쓰는 주요 slot은 capacity=`CapacityInputAndTransfer.x`, saturation transfer=`.z`, geometry transfer=`.w`, decay/geometry factor=`DecayAndGeometry`의 각 slot, 두께 및 cavity retention=`AccumulationThickness`다.

## 렌더링 Material 파라미터

정의는 [`MaterialParameters.glsl`](../../../../Shaders/Rendering/Surface/MaterialParameters.glsl), CPU 값 채우기는 [`Renderer.cpp`](../../../../Source/Rendering/Renderer.cpp)에 있다. `Material`은 이 uniform block의 인스턴스명이다.

| 변수명 | 현재 코드 의미 |
|---|---|
| `BaseColor` | base color texture에 곱하는 material 색상. |
| `RenderMode` | render mode 값. |
| `FlipNormalY`, `NormalStrength` | normal map의 Y 방향 반전과 XY 세기. |
| `AmbientLight` | 공통 조명 함수의 ambient 항. |
| `DebugStateChannel`, `StateChannelCount` | 디버그에 사용할 State ID와 전체 channel 수. |
| `DebugViewParameter`, `ReliefShadingEnabled`, `DebugOptions`, `DebugFlags` | 디버그 표시 및 relief 설정. 세부 slot은 호출 경로별로 해석한다. |
| `DemoStateChannels` | x=Wetness ID, y=Mud ID, z=WaterFilm ID, w=demo 효과 활성 여부. ID는 Registry에서 동적으로 얻는다. |
| `DemoOptions` | x=dry roughness, y=wet roughness, z=mud roughness, w=Lit height display scale. |
| `DemoEffectOptions` | x=wetness strength, y=wet specular strength, z=waterfilm opacity, w=waterfilm roughness. |
| `WetnessTint`, `WaterFilmTint` | 각각 Wetness와 WaterFilm의 색조. |
| `DemoExtraStateChannels` | x=Lava ID, 나머지는 예약 slot. |
| `CameraPosition` | 월드 좌표 카메라 위치. |

`Demo*` 필드는 현재 데모 재질 경로의 파라미터 이름이다. 범용 State 효과 인터페이스를 의미하지 않는다.

## Surface Lit stage 전달 변수

[`SurfaceLit.vert`](../../../../Shaders/Rendering/Surface/SurfaceLit.vert)에서 fragment stage로 전달하고, [`SurfaceLit.glsl`](../../../../Shaders/Rendering/Surface/SurfaceLit.glsl)에서 읽는다. `Frag` 접두사는 fragment shader 입력이라는 역할을 표시한다.

| 이름 | 의미 |
|---|---|
| `FragNormal` | 월드 공간 표면 법선. |
| `FragTangent`, `FragTangentSign` | tangent와 bitangent 방향(handedness) 복원에 쓰는 부호. |
| `FragUV` | texture 및 Surface State sampling 좌표. |
| `FragSurfaceIndex` | State sampling에 사용할 Surface 식별자. |
| `FragMesoNormalWS` | 월드 공간 Meso/변위 법선. `WS`는 World Space 약어다. Static mesh 경로에서는 현재 `FragNormal`과 같은 값을 전달한다. |
| `FragWorldPosition` | 월드 공간 위치. |
| `BaseColorTexture`, `NormalTexture` | material의 base color 및 tangent-space normal texture. |
| `OutColor` | fragment 최종 색상 출력. |

`Texel` lit 경로는 vertex tangent 대신 world position과 UV의 screen derivative로 tangent basis를 복원한다. 이 때 `PositionDx/Dy`, `UVDx/UVDy`의 접미사 `Dx/Dy`는 화면 x/y 방향 미분이다.

## 반복되는 계산 용어

| 이름 | 의미 |
|---|---|
| `TexelIndex`, `ChannelIndex`, `DirectionIndex` | 각각 texel, 동적 State channel, 8방향 이웃 배열의 direction index. |
| `StateValueIndex` | texel-major State 배열의 `TexelIndex * StateChannelCount + ChannelIndex`. |
| `Profile`, `Record`, `ProfileParameters` | `Profile`은 profile index, `Record`는 profile × channel의 flat index, `ProfileParameters`는 그 레코드 데이터다. |
| `State`, `Current`, `Amount` | State 총량을 나타내는 문맥 의존 로컬 이름. 코드에 따라 현재값 또는 양을 뜻하므로 단독으로는 구별되지 않는다. |
| `Capacity`, `Saturation`, `SourceSaturation` | capacity, `State / Capacity`, source texel의 해당 비율. Transport용 saturation은 초과량을 보존하므로 표시용 clamp와 같다고 가정하면 안 된다. |
| `RawFlux`, `RawOutgoing`, `Incoming`, `Outgoing`, `Decay`, `EventInput` | 제한 배율 적용 전 방향별 flux, 그 합, 유입, 제한 후 유출, 감쇠량, 이번 step 외부 입력량. |
| `SaturationDrive`, `GeometryDrive` | 각각 source와 target의 saturation 차이, 표면 위 월드 기하/중력 방향에서 얻는 전달 구동량. |
| `TransferWeight`, `Retention` | edge별 전달 가중치, 오목한 경계에서 남기는 비율. |
| `Meso`, `Accumulation`, `Following`, `Cavity` | 각각 Meso 형상, 적층 높이, cavity 내부를 채우고 남는 표면 위 두께, 오목한 영역에 채워지는 부분. 문맥에 따라 `Height` 또는 `Amount`와 조합된다. |
| `Local`, `World`, `WS` | local/model 좌표, world 좌표, World Space. 접미사가 생략된 벡터는 선언 위치와 변환 식으로 좌표계를 확인한다. |

## 검토 메모

- `AccumulationHeights`는 scalar height 배열만을 뜻하는 것처럼 보이지만, binding 22의 할당에는 dirty plane과 dispatch용 flag/command 데이터도 들어간다. 실제 layout은 GPU resource 구현을 기준으로 읽어야 한다.
- `RawOutgoingBuffer`와 `RawFluxBuffer`는 실제 변수명이며 다른 리소스 이름의 `Buffer` 유무와 일관되지 않는다. 가독성 개선을 원하면 관련 CPU descriptor/layout 참조와 함께 이름 변경을 검토해야 한다.
- `NormalMatrixAndUpColumns`는 이름에 담기지 않은 xyz/w 이중 의미가 있다. CPU와 GLSL 모두 같은 3개 vec4 packing을 사용하므로 단독 rename은 인터페이스를 설명하지 못한다.
- `Current`, `State`, `Amount`, `Value`는 여러 shader에서 범용적으로 쓰여 처음 읽을 때 저장 단위·정규화 여부가 드러나지 않을 수 있다. 코드 변경 시 `CurrentStateAmount`, `CapacityNormalizedSaturation`처럼 의미를 포함할지 검토할 수 있다.
- `FragMesoNormalWS`는 Static mesh 경로에서도 `FragNormal` 값을 받는다. 변수는 texel-lit 경로의 변위 법선 의미를 반영한다.

구현 상태와 전체 렌더링 경로는 [[../../04_Architecture/0009_Rendering|Surface State Rendering]], GPU 리소스 계약은 [[Surface-State-GPU-Resource|Surface State GPU Resource]]를 참조한다.
