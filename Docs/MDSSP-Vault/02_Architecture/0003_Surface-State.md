# 표면 상태와 데이터 구조

상태: **핵심 의미 확정** · 근거: [[05_Assets/Documents/0002_Surface-System-Data.pdf|시스템 데이터 구조]]

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

`.SRProfile`의 직렬화 형식은 [[02_Architecture/0004_Assets-and-Profiles|에셋과 프로필]]에서 다룬다. 파라미터의 **의미와 범위는 이 문서가 기준**이다.

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

### Transition Parameters

| Parameter | 의미 | 범위 |
|---|---|---|
| `source` | 상태 전이의 원인이 되는 State 채널 | State Channel |
| `target` | 전이 결과 증가하는 State 채널 | State Channel |
| `threshold` | 전이가 시작되는 `source`의 Saturation 임계값 | `[0,1]` |
| `transitionRate` | 조건 만족 후 target State가 증가하는 단위 시간당 기본 속도 | `[0,n]` |

상태 전이의 사용 예는 [[02_Architecture/0009_State-Transitions|State Transition]]을 본다.

## Surface Instance State Data

State별로 현재 상태와 Solver 계산 과정의 임시값을 각각 스칼라 채널로 다룬다.

| 항목 | 저장 단위 | 범위 | 의미 |
|---|---|---|---|
| `State` | Texel별 | `[0, stateCapacity]` | 현재 표면에 반영된 상태량 |
| `TempState` | Texel별 | `[0,1]` | 4주차 2-Pass Solver의 상태별 `alpha` 임시값 |

`TempState`는 Capacity를 초과한 상태량을 보관하지 않는다. **Capacity 초과량은 별도로 저장하지 않는다.**

4주차 GPU 구현에서 Current/Next State는 A/B buffer로 ping-pong하고, `TempState`는 Pass 1의 상태별 `alpha`를 저장하는 `TempAlphaBuffer`로 사용한다. 자세한 배치는 [[04_Development/Notes/0003_Surface-State-GPU-Resource|Surface State GPU Resource]]를 본다.
