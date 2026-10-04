# Decision 0009 — 텍셀 면적과 State 총량

> **한 줄 요약:** 텍셀마다 State 총량을 저장하고, 실제 면적에 맞춰 Capacity·입력·감쇠를 계산한다.

- 분류: **Simulation**
- Status: **Accepted**
- Date: 2026-09-29

## 쉽게 읽기

각 texel에는 총 State 양을 저장하고, texel의 실제 월드 면적에 맞춰 Capacity·입력·감쇠를 계산한다. 그래서 격자 해상도만으로 총량이 불어나지 않게 한다.

## Context — 왜 필요했나

초기 구현은 모든 texel에 같은 Profile Capacity와 같은 Contact 입력량을 사용했다. 해상도를 높이면 같은 표면에 더 많은 texel이 생기므로 같은 입력 동작의 총량과 전체 Capacity가 늘어났다. State 합 보존만으로 이 차이가 사라지지는 않는다.

## Decision — 무엇을 정했나

State A/B는 **texel별 총량**을 유지한다. 밀도를 저장하는 방식으로 바꾸지 않는다. Registry channel마다 양의 의미는 달라질 수 있으며 모든 State를 g 단위로 강제하지 않는다.

기존 Profile 수치의 튜닝 규모를 유지하기 위해 고정 기준 면적을 둔다. 월드 길이의 단위가 바뀌면 이 기준도 함께 재정의해야 한다.

```text
ReferenceArea = 1 / (256 × 256) world-length²
AreaScale_i = WorldTexelArea_i / ReferenceArea
Capacity_i = Profile.StateCapacity × AreaScale_i
Input_i = Strength × ContactWeight_i × InputFactor_i × AreaScale_i
Decay_i = min(State_i, Profile.DecayRate × AreaScale_i × Retention_i × dt)
Saturation_i = State_i / Capacity_i
```

`ReferenceArea`는 선택한 해상도와 무관한 고정 단위다. 128 또는 512로 전환할 때 분모를 바꾸지 않는다. Profile `stateCapacity`는 이 기준 면적의 포화 기준량, `decayRate`는 이 기준 면적의 초당 감소량, Contact `Strength`는 이 기준 면적에 한 사건으로 넣는 양이다. Strength를 브러시 전체 총량으로 정규화하는 정책은 채택하지 않는다. 같은 월드 반경과 falloff로 적용하면 적분 입력 총량이 비슷해지는 정책이다.

면적이 절반인 texel의 Capacity와 같은 접촉으로 들어오는 양도 절반이다. 같은 Saturation을 유지하면서 texel당 총량을 줄이는 것이다. 보존 장부는 `ΣState`이고 밀도 표시에는 `State / Area`를 사용한다. 총량에 면적을 다시 곱하지 않는다.

### 면적 생성과 자원

- UV texel 중심이 속한 Macro 삼각형의 UV→mesh Jacobian에서 local `AreaVector`를 계산한다. UV가 왜곡되면 texel마다 면적이 달라진다.
- instance 선형 변환의 cofactor로 면적 벡터를 변환하고 길이를 취해 월드 면적을 얻는다. 비균일 scale과 반사를 반영한다.
- 실제 사용 면적은 Macro footprint다. Normal Map의 요철 또는 동적 적층으로 늘어난 표면적을 포함하지 않는다.
- Chart 경계의 부분 texel을 polygon clipping으로 적분하지 않는다. texel 중심의 삼각형에 대한 전체 footprint 근사이므로 경계 오차가 남는다.
- Shared CPU geometry와 `.Surface`에 `AreaVector`를 보관한다. Cache format 4, preprocessing version 2, padding 없는 texel record 140 B를 사용하며 이전 cache는 재생성한다.
- GPU 면적은 instance별 `float32` buffer, texel당 4 B, binding 20이다. 선형 transform 변경 시 TransferWeight와 함께 갱신한다. 이동만으로는 면적이 바뀌지 않는다.
- 면적 0인 instance는 전달·감쇠를 중단하고 기존 State 총량을 유지한다.

Capacity는 저장 상한이 아니다. 포화도와 Geometry mobility는 **1에서 자르지 않는다**. 기존 source alpha는 최대 1인 유출 제한 비율이며 다른 개념이다.

## Alternatives Considered — 다른 방법

- 면적당 State를 직접 저장: 가능한 표현이지만 기존 총량 장부와 State ABI를 바꾸므로 채택하지 않는다.
- Profile 수치를 world-length²당 밀도로 즉시 재해석: 기존 입력·전달 계수 규모가 크게 바뀐다. 고정 기준 면적을 통해 기존 숫자 범위를 유지한다.
- 면적 대신 해상도만으로 보정: UV 왜곡과 instance scale을 반영하지 못하므로 채택하지 않는다.

## Consequences — 결정의 영향

전체 Capacity와 외부 입력량의 해상도 의존을 줄인다. 동일 총량을 더 작은 texel이 받으면 밀도와 Saturation은 더 크게 증가한다. State buffer 크기와 channel 구성은 유지한다. 추가 GPU 면적 payload는 texel 수 N에 대해 4N B이며 allocator overhead는 별도다.

기존 장면의 분포와 흐름은 바뀔 수 있다. 임의 UV·seam·요철에서 결과가 완전히 같다는 보장은 없고 동일 월드 조건 실험이 필요하다. Profile JSON version 2는 유지하지만 Capacity·Strength·Decay의 의미가 위 기준 면적으로 명확해졌다.

## Related — 관련 문서

- [[05_Decisions/0010_Geometry-Transport-Mobility|Decision 0010 — Geometry mobility]]
- [[05_Decisions/0011_Accumulated-Simulation-Timestep|Decision 0011 — 누적 시간]]
- [[05_Decisions/0004_State-Overcapacity-Transport|Decision 0004]]
- [[05_Decisions/0006_Resolution-Surface-Cache|Decision 0006]]
- [[03_Architecture/0002_Surface-State|State]]
- [[03_Architecture/0004_Surface-Geometry|Geometry]]
- 해상도·시간 비교 계획
