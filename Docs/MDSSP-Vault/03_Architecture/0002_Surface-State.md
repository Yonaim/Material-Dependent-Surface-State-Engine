# 표면 상태와 데이터 구조

상태: **핵심 의미 확정** · 근거: [[06_Assets/Documents/0002_Surface-System-Data.pdf|시스템 데이터 구조]]

## 전체 데이터 분류

```text
Surface State System Data
├── SurfaceResponseProfile        [Instance 간 공유]
└── SurfaceData
    ├── SharedSurfaceGeometryData [Static / Instance 간 공유]
    └── SurfaceInstanceStateData  [Dynamic / Instance별]
```

`SurfaceResponseProfile`은 여러 Instance가 공유 가능한 소재 반응 데이터이고, `SurfaceData`는 시뮬레이션에 필요한 형상·상태 데이터다.

## 기본 State 채널

```cpp
enum class SurfaceStateType : uint8_t {
    Wetness,
    Heat,
    Burn,
    Mud
};
```

`Snow`, `SurfaceWater`는 목표 데모에서 필요하지만 현재 기본 enum에는 없는 확장 대상이다.

## State / Capacity / Saturation

```text
stateCapacity = 해당 State가 가질 수 있는 최대 상태량
State         = 현재 표면에 반영된 상태량
Saturation    = State / stateCapacity
```

$$
0 \le State_i \le stateCapacity_i
$$

$$
Saturation_i = \frac{State_i}{stateCapacity_i}
$$

- `stateCapacity`는 State별 SRProfile 독립 파라미터이며 기본값은 `1.0`이다.
- Mud, Snow처럼 큰 누적량을 표현해야 하는 상태는 `stateCapacity > 1`을 사용할 수 있다.
- `Saturation`은 저장 파라미터가 아니라 런타임 파생값이다.
- Saturation 계산 때문에 `stateCapacity`는 0보다 큰 값으로 사용한다.

## Surface Response Profile

`.SRProfile`의 직렬화 형식은 [[03_Architecture/0003_Assets-and-Profiles|에셋과 프로필]]에서 다룬다. 파라미터의 **의미와 범위는 이 문서가 기준**이다.

### State Parameters

| Parameter | 의미 | 범위 | 기본값 |
|---|---|---:|---:|
| `stateCapacity` | 해당 State의 최대 상태량 | `(0, n]` | `1.0` |
| `inputFactor` | 외부 Source 입력을 해당 State에 얼마나 반영할지 결정 | `[0, n]` | `1.0` |
| `saturationTransferRate` | Saturation 차이에 의한 단위 시간당 기본 전달 속도 | `[0, n]` | `0.0` |
| `geometryTransferRate` | 높이·중력·표면 방향 등 Geometry에 의한 단위 시간당 기본 전달 속도 | `[0, n]` | `0.0` |
| `decayRate` | State가 시간 경과에 따라 자연 감소하는 단위 시간당 기본 속도 | `[0, n]` | `0.0` |
| `cavityRetentionFactor` | 오목한 영역에서 Decay가 억제되는 정도 | `[0,1]` | `0.0` |
| `accumulationFactor` | State를 형상상의 적층량으로 변환하는 정도 | `[0,n]` | `0.0` |
| `cavityFillFactor` | 적층량 중 Cavity를 채우는 데 우선 배분할 비율 | `[0,1]` | `0.0` |

상태 전이 규칙과 전이 파라미터의 의미는 [[03_Architecture/0002_Surface-State|State Transition]]에서 정의한다.

상태 전이의 사용 예는 [[03_Architecture/0002_Surface-State|State Transition]]을 본다.

## Surface Instance State Data

State별로 현재 상태와 Solver 계산 과정의 임시값을 각각 스칼라 채널로 다룬다.

| 항목 | 저장 단위 | 범위 | 의미 |
|---|---|---|---|
| `State` | Texel별 | `[0, stateCapacity]` | 현재 표면에 반영된 상태량 |
| `TempState` | Texel별 | Solver에 따라 다름 | Solver 계산 중 필요한 임시 상태값 |

`TempState`는 영구 상태 채널이 아니라 Solver 계산 중 사용하는 임시 데이터다. 현재 설계에서는 Capacity 초과량을 별도로 저장하지 않는다. 구체적인 임시값과 GPU 배치는 [[05_Development/Notes/0003_Surface-State-GPU-Resource|Surface State GPU Resource]]를 본다.

## State Transitions

State Transition은 한 State가 조건을 만족했을 때 다른 State를 증가시키는 규칙이다.

예:

```text
Heat → Burn
```

| Parameter | 의미 |
|---|---|
| `source` | 전이의 원인이 되는 State |
| `target` | 전이 결과 증가하는 State |
| `threshold` | source Saturation의 임계값 |
| `transitionRate` | 조건 만족 후 target State의 단위 시간당 증가 속도 |

예를 들어 `threshold = 0.7`이면 source의 Saturation이 `0.7` 이상일 때 전이 조건을 만족한다. Transition의 실행 순서와 Solver 패스 배치는 [[05_Development/Notes/0002_Next-State-Calculation|Next State 계산 메모]]에서 다룬다.
