# Decision 0030 — 공통 윗면의 Mud·WaterFilm 재질 합성

> **한 줄 요약:** `H_total` 공통 누적 윗면 하나를 유지하고, 머드와 물의 색·투명도를 한 fragment 결과로 합성한다.

- 분류: **Rendering**
- Status: **Accepted (코드 반영; 화면 품질 검증 전)**
- Date: 2026-10-20

## Context

Lit은 이미 State별 높이를 합산한 공통 누적 윗면을 사용한다. 기존 fragment shader는 원본 BaseColor에서 재질 합성을 시작하고 최종 alpha를 1로 출력하므로, 머드 형상에도 BaseColor 무늬가 남고 WaterFilm 아래 표면이 비치지 않는다.

요구 외관은 원본 Base Surface를 보존하면서 누적 표면에서 머드는 갈색으로 덮고, 물은 반투명하게 합성하는 것이다. Mud와 WaterFilm의 실제 퇴적 순서는 State 데이터에 저장되지 않으며, 이번 외관에는 범용 Layer Stack이 필요하지 않다.

## Decision

1. 원본 Base Surface와 기존 `H_total` 누적 윗면을 유지한다. 별도 State mesh나 `H_opaque` 높이장을 추가하지 않는다.
2. Mud가 있는 누적 표면은 Mud color ramp를 사용한다. Mud overlay 색은 원본 BaseColor texture를 샘플링하지 않는다.
3. WaterFilm은 공통 윗면에서 translucent color로 출력하고 고정 기능 alpha blending을 사용한다. 기본 opacity는 `0.35`이며 `WaterFilmOpacity` 설정으로 조절한다.
4. Mud와 WaterFilm이 겹치는 fragment는 WaterFilm을 Mud 위에 source-over 합성해 단일 straight-alpha RGBA를 출력한다. Lava는 기존처럼 opaque 우선 재질로 유지한다.
5. 이 합성은 표시 규칙이다. 실제 State 간 퇴적 순서나 머드 표면 아래의 별도 바닥 형상을 보존하지 않는다.
6. Solver의 `H_total` 계산은 렌더링 opacity와 재질 합성에 영향을 받지 않는다.

## Consequences

- Mud만 있는 곳은 원본 텍스처 무늬 대신 갈색 표면을 보여준다. WaterFilm만 있는 곳은 기존 Base Surface가 비치고, Mud와 WaterFilm이 겹치는 곳은 물 색이 갈색 위에 합성된다.
- 모든 material을 한 draw에서 혼합하므로 별도 WaterFilm geometry나 추가 height computation은 없다.
- 일반 alpha blending은 겹치는 투명 오브젝트의 순서 독립 합성을 제공하지 않는다. 현재 Overlay pipeline은 depth write를 유지하므로 겹치는 transparent instance의 렌더 순서는 향후 필요에 따라 다뤄야 한다.
- 물 아래에 실제로 파인 Mud 바닥을 독립된 깊이로 보여줘야 하는 경우에는 이 공통 윗면 모델만으로 충분하지 않다.

## Related

- [[05_Decisions/0028_Shared-Accumulation-Top-and-Optional-Transparent-WaterFilm|Decision 0028 — 공통 누적 윗면과 선택적 투명 WaterFilm]]
- [[03_Architecture/0008_Rendering|Surface State Rendering]]
