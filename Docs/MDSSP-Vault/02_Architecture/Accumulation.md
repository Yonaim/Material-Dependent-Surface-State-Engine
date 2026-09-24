# 적층과 Accumulation Height

상태: **수식 확정** · 근거: [[05_Assets/Documents/Geometry-Integration.pdf|형상 정보 반영]]

적층은 State를 직접 변경하는 Solver 항이 아니라, 계산된 State를 **형상상의 높이 변화**로 변환하는 후속 Geometry 계산이다.

$$
Accumulation\_Height
=
Cavity\_Filling\_Height
+
Surface\_Following\_Height
$$

- **Cavity Filling**: Macro Surface 기준 아래쪽의 Meso cavity를 메운다.
- **Surface Following**: 기존 Meso 요철을 따라 표면 바깥쪽으로 쌓인다.

## 1. 전체 적층량

$$
Accumulation\_Amount
=
State \times Accumulation\_Factor
$$

- `State ∈ [0, stateCapacity]`
- `Accumulation_Factor ∈ [0,n]`
- `Accumulation_Factor = 0`이면 State가 있어도 형상 적층을 만들지 않는다.

## 2. Cavity / Surface 배분

$$
Cavity\_Amount
=
Accumulation\_Amount \times Cavity\_Fill\_Factor
$$

$$
Surface\_Amount
=
Accumulation\_Amount \times (1-Cavity\_Fill\_Factor)
$$

`Cavity_Fill_Factor ∈ [0,1]`이며 SRProfile에서 결정한다.

## 3. 실제 높이로 변환

$$
Cavity\_Depth
=
max(-Meso\_Virtual\_Height, 0)
$$

$$
Surface\_Following\_Height
=
Surface\_Amount \times Meso\_Height\_Reference
$$

$$
Cavity\_Filling\_Height
=
Cavity\_Amount \times Cavity\_Depth
$$

## 4. Cavity Fill 상한

Cavity는 최대 100%까지만 채운다. 초과 적층량은 버리지 않고 Surface Following으로 넘긴다.

```text
Cavity_Fill
= min(Cavity_Amount, 1)

Cavity_Excess
= max(Cavity_Amount - 1, 0)

Cavity_Filling_Height
= Cavity_Fill × Cavity_Depth

Surface_Following_Height
= (Surface_Amount + Cavity_Excess)
  × Meso_Height_Reference

Accumulation_Height
= Cavity_Filling_Height
+ Surface_Following_Height
```

## 5. Simulation의 최종 Height

$$
DynamicFinalHeight
=
MacroHeight
+
MesoVirtualHeight
+
AccumulationHeight
$$

Accumulation Height로 변한 형상은 Rendering뿐 아니라 다음 Simulation의 Normal / Distance / Height / Curvature에도 다시 반영한다. [[03_ADR/0003-Dynamic-Accumulation-Geometry|ADR 0003]]

현재 기본 State 중 Wetness / Heat / Burn은 형상 적층이 없도록 `accumulationFactor = 0`을 사용할 수 있고, Mud는 적층을 표현한다. SurfaceWater / Snow의 적층은 해당 State가 추가될 때 프로필로 정의한다.
