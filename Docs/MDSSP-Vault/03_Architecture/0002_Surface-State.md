# 표면 상태와 데이터 구조

상태: **핵심 의미 확정** · 근거: [[07_Assets/Documents/0002_Surface-System-Data.pdf|시스템 데이터 구조]]

## 전체 데이터 분류

```text
Surface State System Data
├── SurfaceResponseProfile        [Instance 간 공유]
└── SurfaceData
    ├── SharedSurfaceGeometryData [Static / Instance 간 공유]
    └── SurfaceInstanceStateData  [Dynamic / Instance별]
```

`SurfaceResponseProfile`은 여러 Instance가 공유 가능한 소재 반응 데이터이고, `SurfaceData`는 시뮬레이션에 필요한 형상·상태 데이터다.

## State 식별과 런타임 채널

State 종류는 C++ enum에 고정하지 않는다. 로드된 `.SRProfile`의 `states` key를 모아 `SurfaceStateRegistry`를 만들며, Registry가 문자열 State 이름을 런타임 `StateId` 또는 `ChannelIndex`에 연결한다. 별도의 `SurfaceStateSchema` 파일은 두지 않는다.

State 이름은 앞뒤 whitespace를 제거하고 lowercase로 정규화하며, 그 외 문자와 내부 공백·구두점은 그대로 보존한다. 예를 들어 `" Wetness "`와 `"WETNESS"`는 `wetness`로 합쳐지지만 `surface_heat`, `surface-heat`, `surface heat`는 서로 다른 이름이다. Transition의 source와 target에도 같은 규칙을 적용한다.

`.SRProfile`은 State 종류의 전역 목록이 아니라, 해당 Profile이 지원하는 각 State의 반응 파라미터와 Transition을 정의한다. 런타임 Solver와 GPU는 문자열을 직접 분기 기준으로 쓰지 않고 Registry가 부여한 ID/index를 사용한다. ID의 배정과 저장 레이아웃은 구현 계약에서 정한다. 상세 결정은 [[../04_ADR/0006-Dynamic-State-Registry|ADR 0006 — SRProfile 기반 동적 State Registry]]를 따른다.

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

CPU 상태 데이터는 Registry의 State 수에 대응하는 동적 채널 집합으로 표현한다. 구체적인 컨테이너와 GPU 배치는 이 문서가 고정하지 않으며 [[05_Development/Notes/0003_Surface-State-GPU-Resource|Surface State GPU Resource]]에서 다룬다. CPU 도메인 표현과 GPU 메모리 ABI는 별도 계약이다.

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
