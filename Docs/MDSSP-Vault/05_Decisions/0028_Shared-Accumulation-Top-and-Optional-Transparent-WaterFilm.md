# Decision 0028 — 공통 누적 윗면과 선택적 투명 WaterFilm

> **한 줄 요약:** State별 Render Layer 대신 누적 높이의 공통 윗면에서 재질을 합성하고, 옵션을 켠 경우에만 WaterFilm을 투명 윗면으로 분리한다.

- 분류: **Rendering**
- Status: **Accepted (코드 반영, 128 해상도 제한 프레임 실행 확인; 화면 품질·성능 검증 진행 중)**
- Date: 2026-10-05

## Context

Decision 0019의 초기 구현은 Mud·WaterFilm·Lava를 각각 별도 윗면과 옆면으로 그렸다. 이 구조는 State별 Draw와 형상 계산을 반복한다. 현재 데모의 State 값은 각 texel의 Registry 채널에 독립적으로 저장되며, 물질의 위아래 순서나 층 번호를 보유하지 않는다. 따라서 State별 윗면을 물리적인 Layer Stack으로 해석할 근거가 없다.

얇은 WaterFilm은 같은 표면에서 재질을 합성할 수 있다. 물의 시각적 높이와 아래 재질의 비침이 중요한 장면에는 별도 투명 윗면이 필요하다.

## Decision

1. 원본 Base Surface는 기존 위치에 한 번 그린다. Heat는 Base의 색 반응이다.
2. 기본 Lit 경로는 지원되는 State의 물리 누적 높이 `H_total`로 공통 윗면과 필요한 경계 옆면을 만든다. 공통 fragment shader가 Heat·Mud·WaterFilm·Lava의 State saturation을 읽어 재질을 합성한다. WaterFilm은 기본 경로에서 얇은 막의 tint·roughness·반사 반응으로 처리한다. 윗면은 불투명하고 depth를 기록한다.
3. `Transparent surface` 옵션을 켜면 물을 제외한 `H_opaque`의 공통 불투명 윗면과 물을 포함한 `H_total`의 투명 WaterFilm 윗면을 사용한다. WaterFilm은 불투명 표면 뒤에 alpha blending으로 합성하고 depth를 기록하지 않는다. 물의 옆면은 `H_opaque`와 `H_total` 사이에만 만든다. 두 윗면은 같은 정적 texel topology를 사용하지만 서로 다른 높이 출력과 graphics pipeline으로 그린다.
4. `H_opaque`는 렌더 경로에서 WaterFilm 채널을 제외해 임시로 계산한다. 지속적인 층 번호나 Layer Stack은 만들지 않는다. 기본 경로에는 투명 WaterFilm 패스가 없다.
5. Solver의 Accumulation Geometry Update는 옵션과 관계없이 모든 지원 State가 기여한 물리 `H_total`을 사용한다. 렌더링 전용 높이 배율과 smoothing은 Solver에 입력하지 않는다.
6. State ID는 로드된 `.SRProfile`의 `states`에서 Registry가 부여한다. Heat·Mud·WaterFilm·Lava는 현재 데모 adapter가 조회하는 이름이다.
7. 공통 윗면의 표시 범위는 누적 높이뿐 아니라 해당 재질 State의 포화도도 사용한다. 두께가 0이어도 State가 있는 표면은 재질을 합성한다. 투명 옵션의 불투명 윗면 표시 범위에서는 WaterFilm을 제외한다.

## Alternatives Considered

- State별 Overlay Draw 유지: 독립 채널만으로는 실제 층 순서를 표현하지 못하고 같은 texel의 높이와 Draw 작업을 중복한다.
- WaterFilm을 항상 별도 투명 Draw로 처리: 얇은 막만 필요한 장면에도 추가 형상 계산과 투명 패스 비용이 든다.
- 영속적인 Layer Stack 도입: 현재 독립 State 모델에는 층 번호·순서·재배치 규칙이 없고 이번 렌더링 선택에 필요하지 않다.

## Consequences

- 기본 경로는 Base Surface, 공통 적층 윗면, 필요한 경계 옆면을 그린다. 투명 옵션은 불투명 공통 윗면과 WaterFilm 윗면을 별도 graphics pipeline으로 그리며 물 옆면도 투명 패스를 사용한다.
- 투명 WaterFilm은 아래쪽 Base 또는 불투명 공통 윗면을 비쳐 보이게 하지만, 화면 공간 alpha blending이므로 다중 투명면의 정확한 순서 독립 합성은 제공하지 않는다.
- 투명 옵션에서는 `H_opaque`와 `H_total`을 각각 계산해 추가 GPU 작업이 발생한다. 경계 옆면은 두 높이의 차이를 사용한다.
- 별도 층 순서가 없으므로 같은 texel의 Mud·Lava 재질 우선순위는 fragment shader의 합성 순서로 정한다. 이 순서는 물리적 적층 순서를 뜻하지 않는다.
- BrickCube Lava처럼 `thicknessPerAmount=0`인 State는 높이만으로 윗면을 선별하면 벽돌 면에서 사라진다. 재질 State 포화도를 표시 범위에 포함해 기존의 얇은 표면 표현을 유지한다.
- 현재 데모의 개별 `Enable layer` 스위치는 Overlay 제출을 제어한다. Solver 피드백의 물리 높이 계산은 이 렌더 스위치와 독립적이다.
- 물 접촉이 있는 128 해상도 데모에서 기본 모드와 투명 모드를 각각 3프레임 실행했고 두 실행 모두 정상 종료됐다. 두 실행에서 Vulkan 오류 로그는 없었다. 투명 모드는 검증 시 기본값을 잠시 켜 실행한 뒤 기본값 OFF로 복원했다. 화면 품질과 성능 비교는 이 실행만으로 판정하지 않는다.

## Related

- [[05_Decisions/0019_Base-Surface-and-Accumulation-Overlay|Decision 0019 — 초기 Base/Overlay 분리]]
- [[05_Decisions/0027_Heat-Red-Lit-Demo|Decision 0027 — Heat 외관]]
- [[03_Architecture/0008_Rendering|Surface State Rendering]]
