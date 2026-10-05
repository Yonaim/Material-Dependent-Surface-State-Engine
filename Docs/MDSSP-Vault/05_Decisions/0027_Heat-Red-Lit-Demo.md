# Decision 0027 — Heat 기반 Red Lit 데모

> **한 줄 요약:** 기본 표면 반응 데모를 Wetness에서 Heat로 전환하고, Heat saturation에 따라 표면 albedo를 빨간색으로 바꾼다.

- 분류: **Rendering**
- Status: **Accepted (구현 반영)**
- Date: 2026-10-05

## Context

초기 Lit 데모는 Wetness State를 blue-gray tint, 낮은 roughness와 specular 반응으로 표시했다. 현재 데모 방향은 표면 열이 쌓일수록 표면이 붉게 변하는 Heat 반응이다. State 종류와 ID는 계속 로드된 `.SRProfile`의 `states` key와 Registry가 정한다.

## Decision

1. Lit demo adapter는 현재 Registry에서 `heat` ID를 조회한다. 고정 channel index를 사용하지 않는다.
2. 표시용 Heat 값은 기존 State sampling 및 Capacity/texel area 보정을 그대로 사용한다. 정규화된 값과 설정 세기를 곱해 원래 albedo를 기본 red tint와 보간한다.
3. Heat 표시는 Wetness 전용 roughness 변화, wet specular lobe, grazing reflection을 적용하지 않는다. WaterFilm의 독립적인 외관과 반사는 유지한다.
4. 기본 형상별 demo `.SRProfile`의 `wetness` State를 `heat`로 바꾸고 `DemoWetness.SRProfile`을 `DemoHeat.SRProfile`로 교체한다. 기존 transport 계수 구조는 유지한다. 이 결정은 Heat의 열역학적 물성 모델을 확정하지 않는다.
5. Debug UI에서 Heat tint와 반응 세기를 조정한다. 렌더 설정은 Solver State나 Profile에 저장하지 않는다.

## Alternatives Considered

- Wetness의 어두워짐·specular·roughness 효과를 유지하면서 Heat를 추가하는 방식: 요청된 데모 전환과 맞지 않아 현재 기본 Lit 연결로 채택하지 않는다.
- 범용 State-to-effect 매핑 시스템: 이번 단일 Heat 데모 전환에 필요한 구현 범위를 넘으므로 도입하지 않는다.

## Consequences

- Lit base surface는 Heat saturation이 증가할수록 빨간색으로 변한다. State 양과 transport 계산은 표시용 tint 설정의 영향을 받지 않는다.
- Wetness 전용 appearance adapter와 설정은 기본 데모 코드 및 형상별 demo Profile에서 제거된다. Wetness는 Registry가 정의할 수 있는 일반 State 이름으로 남는다.
- 이전 Wetness 효과의 실제 렌더 검증 기록은 [[0016_Texel-Grid-and-Demo-Lit-Effects|Decision 0016]]의 당시 구현 기록으로 남긴다. 현재 Heat 경로의 시각 검증은 별도로 필요하다.

## Related

- [[0016_Texel-Grid-and-Demo-Lit-Effects|Decision 0016 — 이전 Wetness·Mud·WaterFilm 데모 Lit 구현]]
- [[../03_Architecture/0002_Surface-State|Surface State Registry]]
- [[../03_Architecture/0008_Rendering|Surface State Rendering]]
