# ADR 0039 — State별 적층 두께와 렌더 표시 배율

> **한 줄 요약:** Capacity로 제한한 State 기여량과 `.SRProfile`의 `thicknessPerAmount`가 시뮬레이션·렌더 적층 두께를 함께 정한다.

- 분류: **Simulation / Rendering**
- Status: **Accepted (구현, 통합 검증 대기)**
- Date: 2026-09-30

## Context

초기안은 Surface별 `Meso_Height_Reference`를 높이 환산에 쓰는 것이었다. 그 뒤 구현은 시뮬레이션 고정 `0.01` mesh-local 기준과 Lit의 임시 `Accumulation height ref`를 사용했다. 이 두 값으로는 같은 기준 면적당 적층량을 가진 Mud와 WaterFilm의 두께를 State별로 정의하거나, 크기 변환이 다른 instance에서 동일한 월드 기준 두께를 유지하기 어렵다. Capacity 초과 State를 모두 형상에 투영하면 고립된 큰 값이 texel 변위에서 과도한 봉우리를 만든다. 저장·수송용 State는 보존하면서 형상 기여만 Capacity에서 제한하고, 렌더와 Solver feedback이 같은 높이 계산을 사용해야 한다.

## Decision

1. `.SRProfile` schema를 version 3으로 올리고 각 `states` 항목에 필수 유한·비음수 `thicknessPerAmount`를 둔다. 단위는 **world-length / 기준 면적당 적층량 1**이다. 월드 길이의 미터 환산값은 프로젝트 단위계를 별도로 정하기 전까지 가정하지 않는다. `accumulationFactor = 0`이면 두께값이 있어도 적층 형상을 만들지 않는다.
2. State A/B는 Capacity 초과량까지 보존한다. 형상 계산에서는 `GeometryState_i = clamp(State_i, 0, Capacity_i)`를 사용하고, `Amount_i = (GeometryState_i / AreaScale) × accumulationFactor_i`를 계산한다. 여기서 `Capacity_i = stateCapacity_i × AreaScale`이다. `thicknessPerAmount`는 State 저장량·입력·Decay를 수정하지 않고, 제한된 형상 기여량을 표면 위 두께로 환산한다. 초과 State는 계속 수송·감쇠되지만 해당 texel의 높이를 Capacity 기준 기여 이상으로 키우지 않는다.
3. 각 State의 `Cavity_i = Amount_i × cavityFillFactor_i`, `Surface_i = Amount_i × (1-cavityFillFactor_i)`를 계산한다. `CavityTotal = Σ Cavity_i`에 대해 cavity 높이는 `min(CavityTotal,1) × max(-MesoVirtualHeight,0)`이다. 표면 위 월드 두께는 `Σ(Surface_i × thicknessPerAmount_i)`에, `CavityTotal > 1`일 때 `(CavityTotal-1) × Σ(Cavity_i × thicknessPerAmount_i) / CavityTotal`을 더한다. 이는 제한된 형상 기여량의 cavity 초과분을 State별 cavity 기여 비율로 배분하며 재질 layer 순서를 정의하지 않는다.
4. GPU 형상은 기존 mesh-local Macro normal 방향으로 변위한다. Instance 선형 변환 `M`과 단위 로컬 normal `n`에 대해 월드 기하 normal 방향의 두께를 유지하도록 `FollowingHeight_local = FollowingHeight_world × |transpose(inverse(M)) × n|`를 사용한다. 균일 스케일 `s`에서는 로컬 높이가 `1/s`가 되며, 비균일 스케일에서도 월드 기하 normal 방향으로 투영한 두께가 Profile 값과 일치한다. 원래의 local-normal 변위 경로는 유지하므로 비균일 스케일에서 월드 변위에 접선 성분이 있을 수 있다. 특이 변환은 기존 Geometry fallback 규칙을 따른다.
5. `Lit height display scale` 하나를 기본 `1.0`의 무차원 렌더 배율로 사용한다. Lit, 선택 State 미리보기, Inspector가 같은 값을 읽으며 Profile 두께를 이용해 계산한 표시 형상에만 적용한다. Solver의 State·Geometry에는 전달하지 않는다. 선택 State 미리보기와 Inspector는 여전히 전체 State 합산 Solver 형상을 나타내지 않는다.
6. 데모 `.SRProfile`의 수치는 물성 측정값이 아니다. 현재 Mud `0.01`, WaterFilm `0.001` world-length/amount는 State별 두께 차이를 보여주는 데모 값이다.

## Alternatives Considered

- Surface별 `Meso_Height_Reference`: 초기 Architecture에 기록된 방식이다. 두께가 Surface Meso 형상 규모에 종속되고 같은 Surface의 State별 두께를 독립적으로 지정하기 어렵다.
- 고정 `0.01` mesh-local Simulation 기준: 기존 구현 방식이다. Instance 스케일과 State 차이에 따른 두께 계약을 제공하지 않는다.
- Lit의 임시 `Accumulation height ref`를 Simulation에도 사용: 렌더 표시 조정이 다음 Solver step의 형상을 바꾸게 되므로 채택하지 않는다.

## Consequences

- `.SRProfile`의 State 항목과 runtime Profile tuning에 두께값이 추가된다. 새 필드는 로딩 시 필수이며 기존 version 2 Profile은 두께값을 채우고 version 3으로 갱신해야 한다. 저장 자산과 fixture에 값을 명시한다.
- GPU Profile record는 Profile/channel당 `float32` 8개를 담은 `vec4[2]` 32 B에서, 두께값과 padding float32 3개를 더한 `vec4[3]` 48 B로 증가한다. std430 `vec4` 16 B 정렬을 가정한다. Scene 전체가 공유하는 Profile table의 payload 증가는 `16 × profileCount × channelCount` B이며 instance별 중복, allocator/descriptor overhead는 제외한다.
- Solver는 Profile별 두께를 합산한다. Lit/Debug compute와 Inspector는 선택 State의 Profile 두께와 공통 표시 배율을 읽는다. `Meso_Height_Reference`와 시뮬레이션 고정 높이 기준은 현재 높이 환산 경로에서 제거된다.
- 렌더링과 Solver의 동적 Geometry는 모두 Capacity 제한 형상 기여량을 사용한다. 전달용 Saturation과 State 저장량에는 Capacity clamp를 적용하지 않는다. 선택 State 미리보기·Inspector도 해당 State의 제한된 형상 기여량을 사용한다.
- 물리 재질별 layer 순서와 상호작용은 여전히 후속 설계 대상이다. 현재 cavity 초과분은 기여 비율로 배분한다.

## Related

- [[../04_Architecture/0002_Surface-State|Surface State]]
- [[../04_Architecture/0004_Surface-Geometry|Surface Geometry]]
- [[../04_Architecture/0008_Surface-GPU-Data-Layout|Surface GPU Data Layout]]
- [[0030-Texel-Area-and-State-Amounts|ADR 0030 — Texel 면적과 State 양]]
- [[0035-Accumulation-Debug-and-Texel-Inspector|ADR 0035 — 적층 디버그]]
- [[0037-Texel-Grid-and-Demo-Lit-Effects|ADR 0037 — 데모 Lit 반응]]
