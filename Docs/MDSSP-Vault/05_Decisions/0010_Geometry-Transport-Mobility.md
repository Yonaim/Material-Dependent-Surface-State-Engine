# Decision 0010 — 출발 포화도에 비례하는 Geometry 전달

> **한 줄 요약:** Geometry 이동량은 출발 위치의 State 양에 비례시키고, 포화도 차이 이동은 별도로 유지한다.

- 분류: **Simulation**
- Status: **Accepted**
- Date: 2026-09-29

## 쉽게 읽기

기울기에 따른 Geometry 이동량을 출발점의 State/Capacity에 비례시킨다. 포화도 차이로 퍼지는 경로는 별도로 유지한다.

## Context — 왜 필요했나

기존 Geometry 원시 전달량은 높이·방향·계수에 의해 정해졌다. source 총량이 두 배여도 alpha가 제한하지 않는 구간에서는 같은 양을 보냈다. 같은 형상과 밀도 기준에서 보유량에 비례해 보내는 모델을 채택한다.

## Decision — 무엇을 정했나

```text
sigma_i = State_i / (Profile.StateCapacity_i × AreaScale_i)
RawFlux_i→j = [SatRate_i × max(sigma_i - sigma_j, 0)
             + GeoRate_i × HeightDrive_i→j × DirectionDrive_i→j × sigma_i]
             × TransferWeight_i→j × dt
```

- `sigma_i`는 1에서 자르지 않는다. 같은 Capacity·형상에서 State가 두 배면 Geometry 원시 전달량도 두 배다. alpha·Decay·이웃 상태 등 다른 조건이 달라지면 최종 전달량까지 항상 두 배인 것은 아니다.
- SaturationDrive는 이웃과의 포화도 **차이**, Geometry mobility는 출발점의 **포화도 자체**다. 두 역할은 중복되지 않는다.
- 같은 포화도라도 경사에서는 Geometry 전달이 생기고, 평평하지만 포화도가 다르면 Saturation 전달이 생긴다.
- SaturationDrive OFF에서도 Geometry mobility를 계산한다. GeometryDrive OFF에서도 SaturationDrive는 유지한다.
- `GeometryDrive = HeightDrive × DirectionDrive`의 정의를 유지한다. HeightDrive는 절대 월드 높이차, DistanceWeight는 `clamp(dRef/d,0,1)` 상대 거리 감쇠다. 높이차/거리 또는 추가 `1/d`로 바꾸지 않는다.
- source alpha, 2-Pass, 입력의 다음 step 재전달은 유지한다. 당시 유지한 RawFlux cache ON/OFF는 후속 Decision 0025에서 제거했다. 새 pass·mobility buffer는 없다.

## Alternatives Considered — 다른 방법

- 기존 Geometry 원시 전달량 유지: 보유량에 비례하는 전달을 만들지 못한다.
- 포화도를 1에서 제한: 초과량에서도 정확한 비례를 유지하려는 조건을 만족하지 못한다.
- SaturationDrive 제거: 기울기 없는 곳의 포화도 차에 따른 퍼짐이 사라지므로 유지한다.

## Consequences — 결정의 영향

Geometry 전달의 물질량 의존이 명시된다. 이는 같은 조건에서 비슷한 이동 비율을 만드는 모델 선택이며 모든 물질의 물리 법칙이라는 뜻은 아니다. 기존 Profile 튜닝 결과는 바뀔 수 있다.

균일 평면에서 해상도를 두 배로 높이면 같은 밀도의 State·Capacity는 약 1/4, 높이차는 약 1/2, 포화도와 상대 Weight는 유지된다. Geometry 전달량/보유량은 약 두 배이고 이동 간격은 절반이어서 월드 이동 거리의 해상도 차이를 줄일 기반이 생긴다. alpha가 빈번하게 제한하거나 여러 방향·UV 왜곡이 있으면 이 단순 스케일 설명만으로 결과를 보장하지 못한다.

## Related — 관련 문서

- [[05_Decisions/0008_Normalized-Transport-Factors|Decision 0008 — 전달 계수]]
- [[05_Decisions/0009_Texel-Area-and-State-Amounts|Decision 0009]]
- [[05_Decisions/0011_Accumulated-Simulation-Timestep|Decision 0011]]
- [[03_Architecture/0006_Surface-State-Update|State Update]]
