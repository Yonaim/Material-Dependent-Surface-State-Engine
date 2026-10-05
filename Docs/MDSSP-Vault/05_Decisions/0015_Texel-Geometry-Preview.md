# Decision 0015 — Texel 연결면 기반 형상 미리보기

> **한 줄 요약:** GPU에서 텍셀별 표면 위치와 법선을 계산해 높이 변화 형상을 미리 본다.

- 분류: **Rendering**
- Status: **Accepted**
- Date: 2026-09-29

## 쉽게 읽기

시뮬레이션 texel을 잇는 표면을 만들어 높이 변화가 실제 실루엣에 나타나게 한다. 표시용 위치와 법선은 GPU에서 계산하고 Solver 상태는 바꾸지 않는다.

## Context — 왜 필요했나

초기 Meso Offset과 Final Geometry는 원본 메시 정점의 UV에서 높이를 조회했다. 원본 정점 사이의 texel에만 높이 변화가 생기면 State와 높이 수치는 정상이어도 형상에 반영되지 않았다. 시뮬레이션 샘플 해상도에 맞는 형상 미리보기가 필요하다.

## Decision — 무엇을 정했나

1. Meso Color/Displacement, Accumulation, Final Geometry는 texel 연결면을 사용한다. State Heatmap과 다른 mapping 진단은 기존 Macro mesh 경로를 유지한다. 새 View Mode는 추가하지 않는다.
2. 초기 연결면은 절대 texel index로 같은 Surface·UV chart의 2×2 grid cell을 삼각형으로 연결했다. 현재 runtime 연결면은 원본 triangle 내부에 texel 중심을 삽입하고 원본 position/edge topology로 seam 경계를 공유한다. 별도 정적 render vertex/index buffer 및 경계 변위 stencil은 [[05_Decisions/0017_Source-Topology-Seam-Stitching|Decision 0017]]을 따른다. 원본 topology가 없는 합성 fixture는 기존 grid 연결을 유지한다.
3. `TexelGeometry.comp`는 마지막 Solver step 이후 최신 State A/B를 읽고 texel별 mesh-local 표시 위치·법선을 한 번 계산한다. 선택 State의 높이 식은 `SurfaceDebugData.glsl`을 Inspector 및 heatmap과 공유한다. 계산 결과는 Solver에 피드백하지 않는다.
4. 위치는 `Position + MacroNormal × DisplayScale × (MesoVirtualHeight + selected AccumulationHeight)`다. Meso 전용 뷰는 적층을 제외하고 배율 1을 사용한다. Accumulation과 Final Geometry는 같은 배율을 공유한다. 갱신 normal의 높이 gradient에도 같은 표시 배율을 적용한다. Inspector 높이와 normal은 배율 1 기준이다.
5. 초기 `TexelGeometry.vert`는 `gl_VertexIndex`로 texel compute 출력을 직접 읽었다. 현재 공통 `TexelMeshVertex.glsl`은 정적 render vertex의 sample/weight로 compute 높이·normal을 보간한다. UV는 원본 corner, 공통 edge의 보간 UV, 내부 texel UV를 사용하며 chart별 UV 분리는 유지한다.
6. 정적 표시 연결면은 Scene 공유 GPU geometry resource 생성 때 구성해 해당 resource 수명 동안 재사용한다. 표시 위치·법선 결과는 Scene instance마다 하나의 GPU-only buffer를 처음 사용할 때 생성하고 매 프레임 compute에서 갱신한다. 이전 vertex 읽기/compute 쓰기 → 다음 compute 쓰기, compute 쓰기 → vertex 읽기 barrier로 같은 queue의 프레임 간 재사용을 보호한다. Scene resource를 다시 구성하면 정적 메시와 출력 buffer를 다시 생성하며 교체 실패 시 기존 리소스를 유지한다.

## Alternatives Considered — 다른 방법

- 원본 메시 정점 변위: 초기 구현이며 원본 정점 사이의 높이를 놓친다.
- texel당 독립 surfel 패치: 샘플 위치를 드러내지만 독립 패치의 틈·겹침을 해결하려면 추가 복원 과정이 필요하다. 현재 미리보기는 연결된 삼각형을 채택한다.
- Vertex shader에서 높이·이웃 법선을 직접 계산: 계산 결과 재사용이 어렵다. Compute에서 계산한 결과를 indexed draw의 정점들이 읽도록 한다.

## Consequences — 결정의 영향

- 원본 메시의 모서리 높이는 0이고 중앙 texel만 올라가는 경우에도 미리보기의 실제 실루엣이 바뀐다. Accumulation heatmap은 같은 변위 면에 표시된다.
- 검증: 전체 build 및 CTest 8개 통과, Vulkan validation 오류 없음. CPU topology에서 중앙 texel 포함·Surface/chart 분리·mirrored UV winding·invalid/퇴화 제외를 확인했다. 실제 GPU 출력과 fragment에서 중앙 적층 실루엣, A/B 전환, 위치·normal의 표시 배율, 미지원 State의 Meso 유지, 반복 계산의 비누적성, State 보존·리셋 및 변위 면의 heatmap을 검증했다.
- 표시 출력은 texel당 두 `vec4`, 16-byte 정렬·32-byte stride이며 추가 padding은 없다. 모든 Surface range의 valid/invalid texel을 포함한 instance별 총 texel 수 T에 대해 `32 × T` byte다. 프레임별 복제는 없고 처음 사용한 instance의 출력은 Scene 교체까지 유지한다. 예를 들어 Surface 하나가 256×256일 때 해당 instance 출력은 2 MiB다. allocator overhead는 제외한다.
- 초기 grid의 정적 index는 `uint32_t`(4 byte, 원소 padding 없음)이며 모든 cell이 유효한 256×256 Surface 하나에서 `6 × 255 × 255 × 4 = 1,560,600` byte였다. 현재 원본 topology 기반 render vertex/index의 계산 및 Runtime별 공유 범위는 Decision 0017을 따른다. CPU 목록은 업로드 후 보존하지 않는다.
- 초기 chart 경계는 texel 중심까지만 연결되어 열린 상태였다. 현재 원본 topology 기반 seam 봉합과 경계 확장은 Decision 0017로 구현했다. 원본 메시 자체의 열린 경계는 유지한다. LOD·대표 Scene 성능 측정은 후속 작업이다. Mud 데모의 Lit 재사용과 높이 grid는 [[05_Decisions/0016_Texel-Grid-and-Demo-Lit-Effects|Decision 0016]]을 따른다.
- 선택 State의 렌더링 미리보기는 Solver 피드백과 독립적이다. 2026-09-30부터 미리보기와 Accumulation Geometry Update는 State별 Capacity 제한 형상 기여 및 `.SRProfile` `thicknessPerAmount`를 사용하며, Solver는 렌더링의 공통 `Lit height display scale`을 읽지 않는다 ([[03_Architecture/0004_Surface-Geometry|Surface Geometry]]). 물리 재질별 다중 layer 순서·상호작용은 후속 구현이다.
- Inspector 선택은 Macro mesh ray hit 기준이다. 변위된 표시 면의 정확한 picking은 후속 작업이다.

## Related — 관련 문서

- [[05_Decisions/0017_Source-Topology-Seam-Stitching|Decision 0017 — 원본 topology 기반 seam 봉합]]

- [[05_Decisions/0014_Accumulation-Debug-and-Texel-Inspector|Decision 0014 — 적층 디버그와 Inspector]]
- [[05_Decisions/0016_Texel-Grid-and-Demo-Lit-Effects|Decision 0016 — Grid와 데모 Lit]]
- [[05_Decisions/0003_Normal-Map-Meso-Geometry|Decision 0003 — Virtual Meso Geometry]]
- [[03_Architecture/0004_Surface-Geometry|Surface Geometry]]
- [[03_Architecture/0008_Rendering|Rendering]]
