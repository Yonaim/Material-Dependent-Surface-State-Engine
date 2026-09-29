# ADR 0035 — 적층 디버그 뷰와 Texel Inspector

> **한 줄 요약:** 두 적층 디버그 뷰와 선택 texel의 완료 GPU snapshot으로 상태량·높이·형상 표시를 검사한다.

- 분류: **Rendering**
- Status: **Accepted**
- Date: 2026-09-29

## Context

기존 State Heatmap은 면적 환산 Capacity에 대한 포화도를 0–1로 표시한다. 초과량이 같은 색이 되며 Cavity Filling과 Surface Following의 높이는 확인할 수 없다. 동적 적층 계산과 Solver 형상 피드백은 아직 구현되지 않았다.

## Decision

1. 기존 State Heatmap에 Raw State 선택과 고정 최대값을 추가한다. 새 View Mode는 Accumulation과 Final Geometry 두 개다. Accumulation의 세부 표시값은 총 높이, Cavity 높이, Following 높이, Cavity Fill 비율이다.
2. Texel Inspector는 Simulation Debug의 Inspector 탭으로 제공한다. Shift + 왼쪽 클릭은 Macro mesh의 triangle/Simulation UV를 Surface range의 texel로 변환한다. 현재 표시와 같은 UV floor/clamp를 사용하며 invalid texel을 다른 texel로 대체하지 않는다.
3. 디버그 적층은 선택 State만 독립적으로 표시한다. 물리적 layer의 순서·합성·다중 State 적층 구현을 뜻하지 않는다. `SurfaceDebugData.glsl`의 같은 식을 vertex, fragment, Inspector compute shader가 사용한다.
4. State는 texel 총량이므로 미리보기의 `ReferenceAmount = State / (WorldTexelArea / SurfaceStateReferenceArea)`를 적층 식의 입력으로 사용한다. Capacity 초과량을 보존한다. `ReferenceAmount × accumulationFactor`를 Cavity/Following으로 배분하고 Cavity 비율의 1 초과분은 Following으로 넘긴다.
5. 현재 미리보기는 조절 가능한 공통 Height reference를 사용한다. 기본값 0.01은 mesh-local 길이의 디버그 설정이다. Architecture의 Surface별 `Meso_Height_Reference` 산정·저장 계약을 확정하거나 대체하지 않는다.
6. Final Geometry는 `MesoVirtualHeight + selected AccumulationHeight`를 Macro normal 방향으로 변위한다. 초기 구현은 원본 메시 정점 변위였으며 현재 texel 연결면과 compute 표시 결과는 [[0036-Texel-Geometry-Preview|ADR 0036]]을 따른다. 이웃 높이의 tangent-plane least-squares gradient로 normal을 재구성하며 fit이 불가능하면 기존 MesoNormal을 사용한다. 원본 Normal Map을 중복 적용하지 않는다. Display scale은 표시 위치·normal에 적용하며 Inspector 수치·State·Solver에는 적용하지 않는다.
7. Inspector는 선택 texel의 GPU 결과만 96 byte snapshot으로 기록한다. 프레임마다 별도 host-coherent buffer를 사용하고 대응 fence 완료 후 CPU가 읽는다. snapshot은 step 번호와 State A/B를 보유한다. 선택·State 채널·높이 기준값·Profile override·State reset은 이전 결과를 무효화하고, Scene/해상도 교체 성공 시 선택과 리소스를 재생성한다. 실패한 교체는 기존 Inspector를 유지한다.
8. 높이 Heatmap은 고정 범위이며 상한 초과는 주황색으로 구분한다. Cavity Fill만 고정 0–1 범위다. 비정상 수치/면적은 자홍색, 미지원 State·미할당 Profile·invalid texel은 각각 구분한다.

## Alternatives Considered

- 각 높이 항목을 독립 View Mode로 추가: 같은 유형의 표시가 여러 항목으로 늘어난다. 하나의 Accumulation 모드 안에서 선택한다.
- 기존 Saturation 색만 사용: 저장량 초과와 높이 변환 단계를 검사할 수 없다.
- 전체 State buffer를 CPU에 내려받아 검사: 선택 texel 검사에 필요하지 않은 데이터까지 읽는다. 같은 GPU 식으로 작은 결과만 기록한다.

## Consequences

- 구현됨: Raw State 표시, 두 적층 미리보기 뷰, 선택 texel GPU snapshot, 적층 파라미터 runtime override.
- 검증: 전체 build와 CTest 8개 통과. 실제 GPU fragment에서 Raw State·높이 항목·Cavity Fill 범위·정점 변위를 확인했고, Inspector compute 결과의 면적 환산·Cavity 초과·A/B·normal gradient·무효화 및 Renderer 비동기 readback·해상도 교체를 검증했다. Vulkan validation 오류는 없었다.
- 후속 작업: 실제 동적 Geometry buffer, Surface별 높이 기준값, 물리적 다중 layer 합성, 갱신 normal/거리/곡률 및 TransferWeight cache의 Solver 피드백. 기존 SurfaceAccumulation/SurfaceGeometryUpdate placeholder는 유지한다.
- 현재 Inspector는 설계식의 GPU 미리보기 값을 검사한다. 미래 적층 pass가 다른 buffer를 생성하면 Inspector가 그 실제 출력도 읽도록 확장해야 한다.
- 초기 정점 변위는 원본 메시 밀도로 실루엣 세부가 제한되었다. 현재 texel 연결면은 시뮬레이션 샘플 밀도를 따르며 chart 경계는 열린 상태다. 표시 배율은 위치와 gradient normal에 함께 적용하지만 실제 두께·물리적 layer 합성과 별도로 해석한다.
- 선택 값은 완료 프레임의 snapshot이며 Running 중에는 현재 화면보다 늦을 수 있다. Pause 후 완료될 때까지 기다리고 Step으로 비교한다.

## Related

- [[0003-Dynamic-Accumulation-Geometry|ADR 0003 — 동적 적층 형상]]
- [[0028-Accumulation-Height-and-Normal-Map|ADR 0028 — 높이와 Normal Map]]
- [[0030-Texel-Area-and-State-Amounts|ADR 0030 — texel 면적과 State 총량]]
- [[../04_Architecture/0010_UI-Interface|UI Interface]]
