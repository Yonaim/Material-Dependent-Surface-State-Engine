# ADR 0035 — 적층 디버그 뷰와 Texel Inspector

> **한 줄 요약:** 두 적층 디버그 뷰와 선택 texel의 완료 GPU snapshot으로 상태량·높이·형상 표시를 검사한다.

- 분류: **Rendering**
- Status: **Accepted**
- Date: 2026-09-29

## Context

기존 State Heatmap은 면적 환산 Capacity에 대한 포화도를 0–1로 표시한다. 초과량이 같은 색이 되며 Cavity Filling과 Surface Following의 높이는 확인할 수 없다. 동적 적층 계산과 Solver 형상 피드백은 아직 구현되지 않았다.

초기 구현은 Raw State 고정 범위와 높이 고정 상한을 노출했다. 현재 UI 결정은 프로파일과 texel 형상에 이미 있는 기준을 색상 정규화에 사용해 수동 상한 조정을 줄인다.

## Decision

1. State Heatmap은 선택 State의 `State / (Profile Capacity × AreaScale)`를 표시한다. Capacity 초과는 주황색으로 구분하고 실제 총량은 Texel Inspector에서 확인한다. Raw State 색상 모드와 수동 상한은 제공하지 않는다. 새 View Mode는 Accumulation과 Final Geometry 두 개다. Accumulation의 세부 표시값은 총 높이, Cavity 높이, Following 높이, Cavity Fill 비율이다.
2. Texel Inspector는 Simulation Debug의 Inspector 탭으로 제공한다. Shift + 왼쪽 클릭은 Macro mesh의 triangle/Simulation UV를 Surface range의 texel로 변환한다. 현재 표시와 같은 UV floor/clamp를 사용하며 invalid texel을 다른 texel로 대체하지 않는다.
3. 디버그 적층은 선택 State만 독립적으로 표시한다. 물리적 layer의 순서·합성·다중 State 적층 구현을 뜻하지 않는다. `SurfaceDebugData.glsl`의 같은 식을 vertex, fragment, Inspector compute shader가 사용한다.
4. State는 texel 총량이므로 미리보기의 `ReferenceAmount = min(State, Capacity) / (WorldTexelArea / SurfaceStateReferenceArea)`를 적층 식의 입력으로 사용한다. Capacity 초과량은 State에 보존하지만 형상 기여에는 포함하지 않는다. `ReferenceAmount × accumulationFactor`를 Cavity/Following으로 배분하고 Cavity 비율의 1 초과분은 Following으로 넘긴다. Geometry와 Solver도 같은 State별 Capacity 제한을 적용한다 ([[0039-State-Thickness-Per-Amount|ADR 0039]]).
5. 적층 높이는 State별 `.SRProfile`의 `thicknessPerAmount`와 선택 texel의 Meso cavity depth를 사용한다. Lit·디버그 미리보기의 `Lit height display scale`은 표시 형상에만 적용하며 Solver와 Heatmap의 기준색을 바꾸지 않는다. UI에서는 Profile Capacity로 제한된 성분별 높이를 색상 기준으로 계산한다.
6. Final Geometry는 `MesoVirtualHeight + selected AccumulationHeight`를 Macro normal 방향으로 변위한다. 초기 구현은 원본 메시 정점 변위였으며 현재 texel 연결면과 compute 표시 결과는 [[0036-Texel-Geometry-Preview|ADR 0036]]을 따른다. 이웃 높이의 tangent-plane least-squares gradient로 normal을 재구성하며 fit이 불가능하면 기존 MesoNormal을 사용한다. 원본 Normal Map을 중복 적용하지 않는다. Display scale은 표시 위치·normal에 적용하며 Inspector 수치·State·Solver에는 적용하지 않는다.
7. Inspector는 선택 texel의 GPU 결과만 96 byte snapshot으로 기록한다. 프레임마다 별도 host-coherent buffer를 사용하고 대응 fence 완료 후 CPU가 읽는다. snapshot은 step 번호와 State A/B를 보유한다. 선택·State 채널·높이 기준값·Profile override·State reset은 이전 결과를 무효화하고, Scene/해상도 교체 성공 시 선택과 리소스를 재생성한다. 실패한 교체는 기존 Inspector를 유지한다.
8. 높이 Heatmap은 선택 Profile의 Capacity와 해당 texel의 Meso cavity depth를 이용해 Capacity 제한 높이를 기준으로 색상을 정규화한다. Total/Cavity/Following은 해당 성분의 Capacity 높이를 100% 기준으로 사용하며, Cavity Fill은 cavity depth 대비 0–100%다. 별도 Height max 입력은 두지 않는다. 비정상 수치/면적은 자홍색, 미지원 State·미할당 Profile·invalid texel은 각각 구분한다.

## Alternatives Considered

- 각 높이 항목을 독립 View Mode로 추가: 같은 유형의 표시가 여러 항목으로 늘어난다. 하나의 Accumulation 모드 안에서 선택한다.
- Raw State와 Height의 수동 고정 상한(초기 구현): 씬마다 색상 상한을 사용자가 입력해야 하므로 유지하지 않는다. 기존 Profile Capacity와 Meso 형상을 기준색에 사용하고 실제 총량은 Inspector에 남긴다.
- 기존 Saturation 색만 사용: 저장량 초과와 높이 변환 단계를 검사할 수 없다.
- 전체 State buffer를 CPU에 내려받아 검사: 선택 texel 검사에 필요하지 않은 데이터까지 읽는다. 같은 GPU 식으로 작은 결과만 기록한다.

## Consequences

- 구현됨: 용량 상대 State Heatmap, 두 적층 미리보기 뷰, 선택 texel GPU snapshot, 적층 파라미터 runtime override.
- 검증: 전체 build와 CTest 8개 통과. 실제 GPU fragment에서 Raw State·높이 항목·Cavity Fill 범위·정점 변위를 확인했고, Inspector compute 결과의 면적 환산·Cavity 초과·A/B·normal gradient·무효화 및 Renderer 비동기 readback·해상도 교체를 검증했다. Vulkan validation 오류는 없었다.
- 후속 상태: 선택 State 미리보기는 디버그 렌더링 전용으로 유지한다. Simulation의 별도 `Accumulation feedback` 옵션은 기본 OFF이며, ON에서 모든 적층 State의 Capacity 제한 형상 기여를 공통 높이로 합성한다. 현재 높이 단위와 변환은 ADR 0039를 따른다. 물리 재질별 layer 순서·상호작용은 후속 과제다.
- 현재 Inspector는 설계식의 GPU 미리보기 값을 검사한다. 미래 적층 pass가 다른 buffer를 생성하면 Inspector가 그 실제 출력도 읽도록 확장해야 한다.
- 2026-09-30 후속 결정: 위 Decision 5의 공통 Height reference는 State별 `.SRProfile` `thicknessPerAmount`와 Lit·디버그 미리보기 공통의 무차원 `Lit height display scale`로 대체했다. Solver는 Profile 두께를 사용하고 표시 배율은 읽지 않는다. 초기 고정 `0.01` Simulation 기준도 제거했다. 추가로 State 저장·수송은 Capacity 초과량을 보존하되, 미리보기와 Solver Geometry feedback의 형상 기여는 Capacity에서 제한한다 ([[0039-State-Thickness-Per-Amount|ADR 0039]]).
- 초기 정점 변위는 원본 메시 밀도로 실루엣 세부가 제한되었다. 현재 연결면은 시뮬레이션 샘플을 포함하고 원본 topology의 seam 경계를 봉합한다 ([[0038-Source-Topology-Seam-Stitching|ADR 0038]]). 원본 메시의 열린 경계는 유지한다. 표시 배율은 위치와 gradient normal에 함께 적용하지만 실제 두께·물리적 layer 합성과 별도로 해석한다.
- 선택 값은 완료 프레임의 snapshot이며 Running 중에는 현재 화면보다 늦을 수 있다. Pause 후 완료될 때까지 기다리고 Step으로 비교한다.

## Related

- [[0003-Dynamic-Accumulation-Geometry|ADR 0003 — 동적 적층 형상]]
- [[0028-Accumulation-Height-and-Normal-Map|ADR 0028 — 높이와 Normal Map]]
- [[0030-Texel-Area-and-State-Amounts|ADR 0030 — texel 면적과 State 총량]]
- [[../04_Architecture/0010_UI-Interface|UI Interface]]
