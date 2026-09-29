# ADR 0036 — Texel 연결면 기반 형상 미리보기

> **한 줄 요약:** Compute가 texel별 표시 위치·법선을 만들고, 같은 UV chart의 샘플을 연결한 삼각형으로 높이 형상을 표시한다.

- 분류: **Rendering**
- Status: **Accepted**
- Date: 2026-09-29

## Context

초기 Meso Offset과 Final Geometry는 원본 메시 정점의 UV에서 높이를 조회했다. 원본 정점 사이의 texel에만 높이 변화가 생기면 State와 높이 수치는 정상이어도 형상에 반영되지 않았다. 시뮬레이션 샘플 해상도에 맞는 형상 미리보기가 필요하다.

## Decision

1. Meso Color/Displacement, Accumulation, Final Geometry는 texel 연결면을 사용한다. State Heatmap과 다른 mapping 진단은 기존 Macro mesh 경로를 유지한다. 새 View Mode는 추가하지 않는다.
2. 정적 index buffer의 원소는 절대 texel index다. 같은 Surface·UV chart의 유효 샘플을 각 2×2 grid cell에서 두 삼각형으로 연결한다. 유효한 세 모서리만 남은 cell도 한 삼각형을 유지한다. invalid/비정상 위치·퇴화 삼각형은 제외하고 mirrored UV의 winding은 기하 normal에 맞춘다. 서로 다른 chart와 Surface는 연결하지 않는다.
3. `TexelGeometry.comp`는 마지막 Solver step 이후 최신 State A/B를 읽고 texel별 mesh-local 표시 위치·법선을 한 번 계산한다. 선택 State의 높이 식은 `SurfaceDebugData.glsl`을 Inspector 및 heatmap과 공유한다. 계산 결과는 Solver에 피드백하지 않는다.
4. 위치는 `Position + MacroNormal × DisplayScale × (MesoVirtualHeight + selected AccumulationHeight)`다. Meso 전용 뷰는 적층을 제외하고 배율 1을 사용한다. Accumulation과 Final Geometry는 같은 배율을 공유한다. 갱신 normal의 높이 gradient에도 같은 표시 배율을 적용한다. Inspector 높이와 normal은 배율 1 기준이다.
5. `TexelGeometry.vert`는 `gl_VertexIndex`로 compute 출력과 정적 texel 데이터를 읽는다. texel 중심 UV를 fragment에 보간하고 기존 높이 색상·조명 계산을 재사용한다. 원본 메시 vertex/index buffer의 밀도는 이 형상 경로를 제한하지 않는다.
6. 표시 결과는 Scene instance마다 하나의 GPU-only buffer를 필요할 때 생성하고 재사용한다. 이전 vertex 읽기/compute 쓰기 → 다음 compute 쓰기, compute 쓰기 → vertex 읽기 barrier로 같은 queue의 프레임 간 재사용을 보호한다. Scene/해상도 교체 시 해당 리소스를 함께 재생성하며 교체 실패 시 기존 리소스를 유지한다.

## Alternatives Considered

- 원본 메시 정점 변위: 초기 구현이며 원본 정점 사이의 높이를 놓친다.
- texel당 독립 surfel 패치: 샘플 위치를 드러내지만 독립 패치의 틈·겹침을 해결하려면 추가 복원 과정이 필요하다. 현재 미리보기는 연결된 삼각형을 채택한다.
- Vertex shader에서 높이·이웃 법선을 직접 계산: 계산 결과 재사용이 어렵다. Compute에서 계산한 결과를 indexed draw의 정점들이 읽도록 한다.

## Consequences

- 원본 메시의 모서리 높이는 0이고 중앙 texel만 올라가는 경우에도 미리보기의 실제 실루엣이 바뀐다. Accumulation heatmap은 같은 변위 면에 표시된다.
- 검증: 전체 build 및 CTest 8개 통과, Vulkan validation 오류 없음. CPU topology에서 중앙 texel 포함·Surface/chart 분리·mirrored UV winding·invalid/퇴화 제외를 확인했다. 실제 GPU 출력과 fragment에서 중앙 적층 실루엣, A/B 전환, 위치·normal의 표시 배율, 미지원 State의 Meso 유지, 반복 계산의 비누적성, State 보존·리셋 및 변위 면의 heatmap을 검증했다.
- 표시 출력은 texel당 두 `vec4`, 16-byte 정렬·32-byte stride이며 추가 padding은 없다. 모든 Surface range의 valid/invalid texel을 포함한 instance별 총 texel 수 T에 대해 `32 × T` byte다. 프레임별 복제는 없고 처음 사용한 instance의 출력은 Scene 교체까지 유지한다. 예를 들어 Surface 하나가 256×256일 때 해당 instance 출력은 2 MiB다. allocator overhead는 제외한다.
- 정적 index는 `uint32_t`(4 byte, 원소 padding 없음)이며 동일 Surface Runtime handle을 사용하는 instance들이 GPU buffer를 공유한다. 모든 cell이 유효한 256×256 Surface 하나의 최대 payload는 `6 × 255 × 255 × 4 = 1,560,600` byte다. CPU index 목록은 업로드 후 보존하지 않는다.
- chart 경계는 열린 상태이며 texel 중심까지만 면을 연결한다. chart stitching, 경계 확장, subtexel 보간/LOD 및 대표 Scene 성능 측정은 후속 작업이다. watertight 최종 모델이나 Lit 경로의 적층 구현을 뜻하지 않는다.
- 실제 물리적 다중 layer 합성, Surface별 높이 기준값, Solver의 동적 거리·normal·곡률 피드백은 후속 구현이다. `SurfaceAccumulation.comp`와 `SurfaceGeometryUpdate` placeholder는 유지한다.
- Inspector 선택은 Macro mesh ray hit 기준이다. 변위된 표시 면의 정확한 picking은 후속 작업이다.

## Related

- [[0035-Accumulation-Debug-and-Texel-Inspector|ADR 0035 — 적층 디버그와 Inspector]]
- [[0018-Normal-Map-Meso-Geometry|ADR 0018 — Virtual Meso Geometry]]
- [[0003-Dynamic-Accumulation-Geometry|ADR 0003 — Solver 형상 피드백]]
- [[0028-Accumulation-Height-and-Normal-Map|ADR 0028 — 높이와 Normal Map]]
- [[../04_Architecture/0009_Rendering|Rendering]]
