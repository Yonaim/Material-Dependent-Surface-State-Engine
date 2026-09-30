# Surface Water Wetting

> **한 줄 요약:** 표면을 따라 이동하는 물 일부를 Wetness로 남겨 젖은 흔적을 표현한다. 표면 물 State는 현재 데모의 `waterfilm`을 가리킨다.

상태: **향후 검토**

## 목표

표면 물이 옆으로 퍼지거나 흘러간 뒤에도 접촉했던 위치에 수분이 남도록 한다. 고여 있는 물과 재질에 남은 Wetness는 별도 State로 표현한다.

## 검토안

초기 후보는 최소 잔류량과 잔류 시간이었다. 현재 검토안은 Profile의 표면 물 State에 `wettingFraction`을 두고, 새로 입력되거나 이웃에서 유입된 양 중 일부를 Wetness로 전환하는 방식이다.

```text
DepositedWetness = NewSurfaceWater × wettingFraction
RemainingSurfaceWater = NewSurfaceWater - DepositedWetness
```

전환은 기존에 고여 있던 전체 SurfaceWater가 아니라 새 Input과 Incoming에만 적용한다. 물이 제자리에 있어도 매 timestep마다 계속 흡수되는 현상을 막고, 물의 총량은 SurfaceWater와 Wetness 사이에서 보존한다. Wetness는 자체 `decayRate`로 마르고 SurfaceWater는 기존 Transport 규칙에 따라 이동한다.

## 구현 전 결정

- `wettingFraction`의 범위와 기본값을 정한다. 큐브 데모에서는 `0.1`을 시작값으로 검토한다.
- SurfaceWater와 Wetness를 모두 지원하지 않는 Profile의 동작을 정한다.
- 현재 `.SRProfile` Transition은 Solver에서 미구현이므로, 전환을 별도 solver 경로로 둘지 Transition 모델을 확장할지 결정한다.

## 검증 기준

- 평평한 표면에서 물이 이동한 경로에 Wetness가 남는다.
- 입력량 = 남은 SurfaceWater + Wetness 전환량 + 기존 Solver 손실량을 만족한다.
- 정지한 SurfaceWater가 timestep 경과만으로 Wetness로 계속 바뀌지 않는다.
- Wetness는 Profile의 `decayRate`에 따라 감소한다.
