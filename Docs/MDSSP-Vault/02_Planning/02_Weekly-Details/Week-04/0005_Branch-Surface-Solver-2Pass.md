# Branch 5 — Surface Solver 2-Pass

브랜치: `feat/surface-solver-2pass`  
선행 조건: `feat/surface-gpu-resources` 병합  
관련 설계: [[03_Architecture/0004_Surface-State-Update|Propagation Solver]], [[05_Development/Notes/0002_Next-State-Calculation|Next State 계산]]

## 목표

synthetic State와 Profile을 사용해 GPU에서 Pass 1/Pass 2를 실행하고 State A/B를 교환한다. 실제 Raycast/Contact 연결은 다음 브랜치에서 수행한다.

## 구현 대상

- `Source/SurfaceStateSystem/State/SurfaceStateSolver.h/.cpp`
- `Shaders/SurfaceNextState.comp`를 Pass별 파일로 분리 권장
  - `Shaders/SurfaceSolverPass1.comp`
  - `Shaders/SurfaceSolverPass2.comp`
- 필요 시 compute pipeline wrapper
- `TSurfaceStateSystem`의 step 호출

기존 `SurfaceTransport.comp`, `SurfaceDecay.comp`, `SurfaceNextState.comp` placeholder는 역할이 겹치지 않게 정리한다. 2-Pass 기준으로 합칠지 제거할지는 이 브랜치에서 한 번만 결정한다.

## Dispatch 규칙

1 invocation이 local texel 하나를 담당한다.

```glsl
uint localIndex = gl_GlobalInvocationID.x;
if (localIndex >= localTexelCount) return;
```

초기 workgroup size는 `local_size_x = 64`를 권장한다. 성능 최적값으로 간주하지 말고 추후 측정한다.

invalid texel은 조기에 종료하되 Pass 1의 TempAlpha와 Pass 2의 NextState를 0으로 명시한다.

각 texel에는 Registry가 정한 수만큼의 State channel이 있다. GPU의 실제 layout은 Branch 4에서 확정하지만 Solver는 이름이나 채널 개수를 하드코딩하지 않고 `stateChannelCount`와 `ChannelIndex`를 사용한다. Invocation은 texel 하나를 담당하며 등록된 모든 channel을 처리한다.

```glsl
uint stateChannelCount; // Registry에서 전달
float loadState(uint texelIndex, uint channelIndex);
void storeNextState(uint texelIndex, uint channelIndex, float value);
```

`loadState`와 `storeNextState`는 Branch 4에서 선택한 layout을 감춘다. 예시 demo의 State name은 Profile/Registry에서 찾아 ID로 다루며, Solver의 계산 경로는 특정 State 이름에 분기하지 않는다.
Profile이 어떤 Registry State를 정의하지 않은 경우의 동작은 아직 결정하지 않았다. 이 브랜치에서 Profile/Channel 지원 mask를 사용해 가능한 정책을 비교하고, 입력·Transport·Decay·Transition 각각의 처리 규칙을 결정해 테스트로 고정한다. 지원 여부는 Branch 4의 Profile GPU representation에서 전달한다.

## Pass 1

입력:

- Current State
- TexelSurfaceIndex / InvalidSurfaceID
- NeighborIndex
- Position/Normal/GeometryScalar (neighbor distance/direction is derived from Position)
- Shared Geometry의 texel별 `ProfileIndex` map
- Profile parameters
- `DeltaTime`, `gravityLocal`

출력:

- `TempAlpha[i]`

채널별 처리:

1. `Saturation = State / stateCapacity`
2. Decay 계산
3. 8방향 `RawFlux(i→j)` 계산
4. `RawOutgoing` 합산
5. `AvailableState = max(State - Decay, 0)`
6. `alpha = min(1, AvailableState / RawOutgoing)`

`RawOutgoing == 0`이면 `alpha = 1`이다.

## Barrier 1

TempAlpha write가 Pass 2 read에 보이도록 `vkCmdPipelineBarrier2`를 사용한다.

```text
srcStage  = COMPUTE_SHADER
srcAccess = SHADER_STORAGE_WRITE
dstStage  = COMPUTE_SHADER
dstAccess = SHADER_STORAGE_READ
```

barrier 대상 buffer range를 TempAlpha로 제한한다.

## Pass 2

각 texel은 gather 방식으로 자기 NextState만 쓴다.

1. 자기 `RawFlux(i→j)`와 `alpha[i]`로 Outgoing 계산
2. 이웃 `RawFlux(j→i)`를 재계산
3. `alpha[j]`를 곱해 Incoming 계산
4. Decay 재계산 또는 Pass 1과 동일 함수 사용
5. InputDelta를 한 번 더함
6. NextState 계산 및 Capacity 범위 제한

```text
Next
= clamp(Current + InputDelta + Incoming - Outgoing - Decay,
        0,
        stateCapacity)
```

Pass 1과 Pass 2가 동일한 Flux/Decay 함수를 사용하도록 GLSL include 또는 공통 함수 파일을 둔다. 두 shader에 수식을 복사해 서로 달라지게 만들지 않는다.

## Drive와 Weight

```text
RawFlux
= (SaturationDrive * saturationTransferRate
 + GeometryDrive   * geometryTransferRate)
 * TransferWeight
 * DeltaTime
```

```text
TransferWeight
= DistanceWeight
 * NormalWeight
 * CurvatureWeight
 * ProfileBoundaryWeight
```

세부식이 아직 미정인 항목은 4주차 기본값을 명시한다.

- `MesoVirtualHeight = 0`
- `ConcavityWeight = 0`
- 동일 Profile 경계 Weight = 1
- 다른 Profile 경계는 임시로 1 또는 전파 차단 중 하나를 선택해 상수로 표시
- GeometryDrive를 아직 검증하지 못하면 0으로 두고 SaturationDrive부터 완성

권장은 SaturationDrive만 먼저 통과시킨 뒤 GeometryDrive를 같은 브랜치의 다음 commit으로 추가하는 것이다.

## Barrier 2와 ping-pong

Pass 2 이후:

- 다음 compute step이 읽으면 compute write→read barrier
- renderer가 읽으면 해당 shader stage의 storage read를 포함
- barrier 완료 후에만 Current/Next 논리 역할 교환

frame-in-flight가 여러 개면 CPU가 같은 buffer를 덮지 않도록 fence/timeline 정책을 확인한다.

## 테스트 시나리오

### 1. 균일 상태

모든 valid texel의 State가 같으면 Saturation transport가 0이어야 한다.

### 2. 단일 source texel

중앙 texel에 State를 주고 주변으로 퍼지는지 확인한다.

### 3. Capacity 차이

State 절댓값이 같아도 Capacity가 다르면 Saturation 차이에 따라 이동해야 한다.

### 4. 보유량 제한

큰 rate/DeltaTime에서도 outgoing 합이 Decay 이후 가용 State를 넘지 않아야 한다.

### 5. Seam

source를 seam 직전에 놓고 반대 chart로 전달되는지 확인한다.

### 6. Invalid texel

초기값에 쓰레기 값을 넣어도 한 step 후 0으로 정리되어야 한다.

### 7. Frame-rate 독립성

동일 총 시간에 대해 `1/30`과 `1/60` step 결과 차이를 허용 오차 안에서 비교한다.

### 8. 동적 State channel 저장

Registry channel count가 1, 4, 6 이상인 경우 각 State가 올바른 channel index에서 독립적으로 갱신되고 서로 섞이지 않아야 한다.

## 디버그 readback

초기에는 작은 8×8 또는 16×16 grid를 host-visible buffer로 실행하고 CPU로 읽어 예상 결과와 비교한다. 화면 결과만 보고 수식 정확성을 판단하지 않는다.

확인할 값:

- Current/Next
- TempAlpha
- texel별 Incoming/Outgoing 디버그 합
- NaN/Inf 존재 여부

Incoming/Outgoing debug buffer는 Debug build에서만 둘 수 있다.

## 권장 커밋 분할

1. `Feat: Surface Solver Compute Pipeline 생성`
2. `Feat: Solver Pass 1에서 Outgoing Alpha 계산`
3. `Feat: Solver Pass 2에서 Next State 수집`
4. `Feat: Solver Barrier와 State Ping-Pong 추가`
5. `Test: 2-Pass Solver 불변 조건 검증`

## 완료 조건

- validation warning 없이 여러 step을 실행한다.
- State A/B가 step마다 정확히 교환된다.
- Solver가 임의의 Registry channel count에서 State를 서로 독립적으로 처리한다.
- 보유량 제한과 Capacity 범위가 유지된다.
- seam/invalid texel test를 통과한다.
- synthetic input만으로 결과를 반복 재현할 수 있다.

## 제외 범위

- 실제 Raycast와 ContactWeight
- 최종 ProfileBoundaryWeight 결합식
- Accumulation geometry 갱신
- 렌더링 품질 최적화
