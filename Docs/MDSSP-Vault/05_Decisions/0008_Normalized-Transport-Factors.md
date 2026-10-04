# Decision 0008 — 정규화된 Transport Factor와 Solver 기준 속도

> **한 줄 요약:** Profile에는 0–1 이동 계수만 두고 실제 전달 속도는 Solver의 공통 기준으로 계산한다.

- 분류: **Simulation**
- Status: **Accepted**
- Date: 2026-09-29

## 후속 결정 — 2026-09-29

Decision 0012에서 Geometry 기준 Rate를 100→6000으로 재보정했다. 아래 Rate 50·기준값 100과 schema 변환 예시는 최초 정규화 당시의 기록이다. 현재 실행값은 공용 SurfaceSolverRates.h를 따른다.

Decision 0009·0010·0011에서 면적 환산, Geometry 출발 포화도, 누적 시간 반복을 추가했다. 아래 데모 동등성 검증은 정규화 변경 당시의 결과이며 이후 분포·시간 의미는 새 계약을 따른다.

## 쉽게 읽기

Profile에는 0–1 범위의 이동 계수를 저장하고, 공통 기준 속도는 Solver가 곱한다. 기준 속도는 Decision 0012에서 다시 조정됐으므로 이 문서의 최초 수치는 역사적 기록이다.

## Context — 왜 필요했나

초기 `.SRProfile` version 1은 `saturationTransferRate`, `geometryTransferRate`에 실제 전달 속도를 저장했다. SaturationDrive는 무차원이고 GeometryDrive는 world-length이므로 두 Rate의 단위와 숫자 규모가 달랐다. DemoWetness와 DemoStone의 `(0.2, 50.0)`은 유효한 수식 입력이지만 Profile 튜닝 과정에서 내부 속도 및 길이 스케일을 함께 해석해야 했다.

현재 결정은 Profile의 전달 성향과 Solver의 기준 속도를 분리하는 것이다. 높이·방향·거리의 역할 분리와 기존 데모 전달량은 유지한다.

## Decision — 무엇을 정했나

### Profile 및 자료형

- `.SRProfile` schema를 version 2로 변경한다. 필수 필드는 `saturationTransferFactor`, `geometryTransferFactor`다.
- CPU 자료형은 `SaturationTransferFactor`, `GeometryTransferFactor`를 사용한다. 두 값의 기본값은 `0.0`이며 유한한 `[0,1]` 값을 허용한다.
- GPU Profile 레코드의 첫 vec4는 `StateCapacity, InputFactor, SaturationTransferFactor, GeometryTransferFactor`를 저장한다. 최초 upload와 Runtime override 모두 Factor를 그대로 pack한다.
- Profile Tuning UI는 두 계수를 `[0,1]` Slider로 조절한다.
- Loader는 version 1을 거부한다. 이전 Rate를 새 Factor로 자동 해석하지 않는다.

### Solver 계산 — 최초 채택 당시

아래 `100.0` Geometry 기준값은 이 Decision을 처음 구현할 때의 값이다. Decision 0012에서 현재 기준값을 `6000.0`으로 바꿨다. 나머지 식과 Profile Factor 계약은 유지한다. 공통 C++/GLSL 상수는 `Source/SurfaceState/Types/SurfaceSolverRates.h`에서 관리한다.

| 상수 | 값 | 단위 |
|---|---:|---|
| `BaseSaturationTransferRate` | `1.0` | `State / second` |
| `BaseGeometryTransferRate` | `100.0` | `State / (world-length · second)` |

`State`는 각 Registry State의 시뮬레이션 상태량 단위다. 기준값은 기존 데모 동작을 유지하는 초기 보정값이며 물성 검증값이 아니다.

```text
SaturationTransferRate_i = SaturationTransferFactor_i × BaseSaturationTransferRate
GeometryTransferRate_i   = GeometryTransferFactor_i   × BaseGeometryTransferRate

RawFlux(i→j) = (SaturationDrive(i→j) × SaturationTransferRate_i
               + GeometryDrive(i→j) × GeometryTransferRate_i)
              × TransferWeight(i→j) × Δt
```

두 기준 속도는 Pass 1과 cache OFF의 Pass 2가 공유하는 `rawFlux` 안에서 적용한다. CPU pack 단계에서는 적용하지 않는다.

### 자산 변환

```text
saturationTransferFactor = 이전 saturationTransferRate / 1.0
geometryTransferFactor   = 이전 geometryTransferRate / 100.0
version                  = 2
```

| Profile | 이전 Saturation / Geometry Rate | 현재 Saturation / Geometry Factor |
|---|---|---|
| DemoWetness | `0.2 / 50.0` | `0.2 / 0.5` |
| DemoStone | `0.2 / 50.0` | `0.2 / 0.5` |
| DemoMud | `0.03 / 0.5` | `0.03 / 0.005` |

변환 결과가 `[0,1]`을 벗어나면 새 기준 속도 범위에서 재튜닝해야 한다. 저장소의 데모와 정상 JSON fixture는 version 2로 변환한다. 거부 동작을 확인하는 legacy fixture는 version 1을 유지한다.

## Alternatives Considered — 다른 방법

### 1. Profile에 실제 Rate를 계속 저장

초기 방식이다. 속도를 직접 지정할 수 있지만 두 전달 경로의 단위와 내부 스케일을 Profile 작성자가 알아야 하므로 정규화된 Factor를 채택한다.

### 2. Geometry 높이차를 이웃 거리로 나눠 정규화

Slope 방식은 Profile 표현뿐 아니라 GeometryDrive의 동작도 바꾸므로 이번 변경에 포함하지 않는다. 기존 `HeightDrive = abs(ΔHeight)`와 별도 DistanceWeight를 유지한다.

## Consequences — 결정의 영향

- Profile 계수는 일관된 `[0,1]` 범위이며 현재 공통 기준 속도는 Saturation `1.0`, Geometry `6000.0`이다. 이 기준 속도는 Profile 계수가 1일 때의 기준값이며 실제 flux에는 drive와 TransferWeight도 반영된다. 따라서 기준 속도가 실제 flux의 상한을 뜻하지는 않는다. SaturationDrive와 높이차의 크기는 계속 flux에 반영된다.
- 데모의 실제 Rate와 기존 GPU 회귀 fixture의 예상 전달량을 유지한다. 이전에 상한보다 높은 Saturation Rate를 쓰던 cache/alpha fixture는 Rate와 Decay를 1/10, timestep을 10배로 바꿔 기존 제한 조건과 예상값을 유지한다.
- 기존 GPU 레코드 크기·필드 슬롯·descriptor·State channel 구성은 유지한다. 셰이더 재빌드는 필요하다.
- Saturation 및 State의 Capacity 초과 허용, source alpha 제한, TransferWeight, 시간 적분 규칙은 유지한다.
- Profile 정규화만으로 모델 크기·해상도·timestep에 따른 이동 특성이 보정되지는 않는다. 기준 장면과 목표 이동 시간에 따른 속도 보정 및 timestep 비교는 후속 검증 대상이다.
- 과거 성능 기록의 Rate는 측정 당시 실제 속도로 보존하고 현재 Factor 표현과 구분한다.

2026-09-29 검증: 전체 C++/Shader 빌드, CTest 7개(CPU 계약·Scene 자원·GPU Solver 포함), 기본 Scene의 5-frame 실행이 통과했다. 두 Factor의 경계값·범위 오류와 version 1 거부를 검사했으며 GPU cache ON/OFF의 기존 예상 전달량을 유지했다. 기본 Scene 실행의 Vulkan validation 오류는 없었다.

## Related — 관련 문서

- [[05_Decisions/0010_Geometry-Transport-Mobility|Decision 0010 — Geometry 전달]]
- [[03_Architecture/0007_Surface-GPU-Data-Layout|Surface GPU Data Layout]]
- [[05_Decisions/0004_State-Overcapacity-Transport|Decision 0004 — Capacity 초과량 보존]]
- [[03_Architecture/0002_Surface-State|Surface State]]
- [[03_Architecture/0003_Assets-and-Profiles|Assets and Profiles]]
- [[03_Architecture/0006_Surface-State-Update|Surface State Update]]
- Surface Data Contract Tests
- Dev Demo
