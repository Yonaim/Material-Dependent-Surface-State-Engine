# Decision 0017 — 원본 topology 기반 UV seam 봉합

> **한 줄 요약:** 원본 Mesh를 텍셀 단위로 나눠 UV 경계에서도 표면 높이와 모양이 이어지게 한다.

- 분류: **Rendering**
- Status: **Accepted (데모 및 형상 미리보기 구현)**
- Date: 2026-09-30

## 쉽게 읽기

원본 삼각형의 연결 관계를 따라 texel 표면을 만들고 UV seam 양쪽의 위치와 변위를 공유한다. 그 결과 seam이 벌어지지 않으며 원본 메시의 열린 경계는 유지한다.

## Context — 왜 필요했나

초기 texel 연결면은 같은 UV chart 안의 유효 중심만 연결했다. 차트 가장자리까지 면이 도달하지 않았고 차트 사이도 연결하지 않아 Bunny의 Mud Lit에 갈라진 띠가 나타났다. Mud 높이가 0이어도 원본 메시를 열린 연결면으로 교체하므로 틈이 발생했다. Solver의 seam 이웃 graph는 높이·State 계산용이며 렌더링 삼각형을 봉합하지 않는다.

원본 정점만 변위하면 중앙 texel의 실루엣 변화가 누락된다. Texel을 포함하면서 원본 메시의 실제 경계와 UV seam을 구분할 렌더링 topology가 필요하다.

## Decision — 무엇을 정했나

1. Runtime의 공유 GPU geometry 생성 시 원본 CPU vertex와 triangle topology를 함께 전달한다. 원본 triangle의 세 corner와 내부 texel 중심을 barycentric 2D 좌표에서 삼각분할한다. 분할에는 MIT 라이선스의 [delaunator-cpp](https://github.com/delfrrr/delaunator-cpp), commit `c1521f6e879881232dcddabd6c2ddb6187e8714b`의 vendored header를 사용한다. 원본 triangle 바깥은 연결하지 않고 원본 winding을 유지한다.
2. 원본 position index가 같은 corner는 공통 변위 방향과 최대 4개 texel sample/weight를 공유한다. UV·Surface·authored normal용 render vertex는 분리해 유지한다. Corner stencil은 incident triangle의 가까운 유효 샘플을 거리 제곱 역수로 가중한다. 샘플이 없는 corner는 원본 edge adjacency를 따라 stencil을 전달받으며, 전체 component가 미샘플이면 높이 0을 유지한다.
3. 원본 edge에 정확히 놓인 texel 중심은 양쪽 triangle의 샘플을 합쳐 공통 edge parameter로 분할한다. 중복 parameter는 barycentric 기준 epsilon `1e-6` 안에서 병합하고 최대 4개 해당 샘플을 균등 가중한다. 양쪽 render vertex는 같은 위치·sample/weight·변위 방향을 사용하고 각자의 UV를 보간한다. 한쪽에만 있는 edge sample도 양쪽에 삽입해 T-junction과 높이 누락을 막는다.
4. 내부 texel의 높이는 해당 compute 출력 하나를 그대로 읽는다. Corner/edge 높이는 공통 stencil로 보간하며 공통 Macro normal 방향으로 변위한다. Shading normal은 authored normal에 샘플의 Meso/높이 normal 변화량을 더해 smooth/hard normal 경계를 유지한다. Meso·Accumulation 계산과 표시 배율, State A/B 및 Solver buffer 계약은 유지한다.
5. Texel 중심이 없는 작은 원본 triangle도 표시한다. 원본 메시의 실제 열린 경계와 서로 분리된 component는 유지한다. UV 또는 공간상의 근접성으로 다른 topology를 연결하지 않는다. Source topology가 없는 합성 fixture는 기존 같은-chart grid 경로를 사용한다.
6. 삼각분할과 정적 render vertex/index 생성은 Scene의 공유 GPU geometry resource를 구성할 때 한 번 수행한다. 해당 resource가 살아 있는 동안 vertex/index buffer는 고정해 둔다. 매 프레임 compute는 재사용하는 instance별 GPU-only 출력 buffer의 texel 위치·법선만 갱신하며, Lit Mud와 Meso/Accumulation/Final Geometry의 vertex shader는 이 결과를 정적 render vertex에서 읽어 변위를 적용한다. 따라서 매 프레임 CPU에서 삼각분할하거나 새 메시 buffer를 만들지 않는다. Scene resource를 다시 구성하면 정적 buffer도 다시 만든다. Meso Displacement와 Final Geometry는 경계 UV cell에 유효 중심이 없어도 보간된 형상을 조명한다. Validity·State/height 수치 진단은 기존 sample 유효성 표시를 유지한다.
7. 변경은 렌더링 topology와 read-only 표시 보간에 한정한다. Mapping·물리 면적·Solver seam graph와 전처리 결과를 바꾸지 않으므로 기존 `.Surface` 캐시 버전은 유지하고 render mesh만 Runtime 리소스 생성 시 재구축한다.

## Alternatives Considered — 다른 방법

- 원본 메시 정점만 변위: 초기 구현에서 중앙 texel의 높이 변화가 실루엣에 반영되지 않았다.
- 같은-chart 2×2 texel 연결면: 기존 구현으로 중앙 샘플은 표시하지만 차트 경계 띠와 seam이 열린다.
- 해상도 증가: 기존 연결면의 경계 간격을 줄일 수 있지만 topology 봉합을 제공하지 않는다.

## Consequences — 결정의 영향

- State가 0인 Mud Bunny에서도 원본 표면까지 면이 생성된다. 높이 차가 있는 seam은 공통 경계 변위로 연속성을 유지한다. 원본 메시 자체의 구멍을 닫는 기능은 아니다.
- Source triangle 수와 texel 샘플 수에 따라 렌더링 삼각형 수가 달라진다. LOD·대표 Scene의 frame 성능 측정, 큰 변위에 따른 fold/self-intersection 방지, 물리적 layer 합성과 Solver 동적 형상 피드백은 후속 작업이다.
- 정적 render vertex는 네 `vec4`(위치·authored normal·UV/Surface·변위 방향), 하나의 `uvec4` sample index와 하나의 `vec4` weight다. 32-bit 원소·16-byte field 간격·96-byte stride이며 별도 padding은 없다. 정적 index는 padding 없는 `uint32_t`(4 byte)다. 동일 Surface Runtime handle의 instance들이 공유하고 in-flight frame별 복제는 없다. V개 render vertex와 I개 index의 payload는 `96 × V + 4 × I` byte이며 CPU 목록은 업로드 후 폐기한다.
- 실제 Bunny의 Surface 하나, 256×256 캐시, 원본 69,451 triangles 기준 render vertex 58,203개·triangle 109,677개로 정적 vertex payload 5,587,488 byte, index payload 1,316,124 byte다. allocator/descriptor overhead와 기존 공유 geometry는 제외한다. 기존 compute 출력은 valid/invalid를 포함한 texel당 두 `vec4`, 32-byte stride의 instance별 buffer이며 256×256에서 2 MiB다. Lit·Debug가 같은 allocation을 사용하고 프레임별로 복제하지 않는다.
- 검증: 전체 build와 CTest 8개 통과, Vulkan validation 오류 없음. CPU seam/no-seam/disconnected fixture의 전체 면적·열린 경계·비정상 연결과 차트별 높이 차의 경계 일치, 미샘플 triangle 유지, 실제 GPU source-edge 중앙 texel 실루엣과 평탄면 전체 coverage를 확인했다.
- 실제 Bunny 256·512 캐시로 원본 topology와 render topology를 비교했다. 두 해상도 모두 원본 열린 edge 223개가 유지됐고 경계 길이 차·새 non-manifold edge·차트별 시험 높이 적용 후 seam 위치 불일치는 0이었다. 512×512에서는 render vertex 118,487개·triangle 230,259개였다. 이 검증은 frame 성능 측정이나 모든 변위량의 self-intersection 검증을 뜻하지 않는다.

## Related — 관련 문서

- [[05_Decisions/0015_Texel-Geometry-Preview|Decision 0015 — Compute 표시 형상]]
- [[05_Decisions/0016_Texel-Grid-and-Demo-Lit-Effects|Decision 0016 — Grid와 데모 Lit]]
- [[05_Decisions/0014_Accumulation-Debug-and-Texel-Inspector|Decision 0014 — 적층 디버그와 Inspector]]
- [[05_Decisions/0006_Resolution-Surface-Cache|Decision 0006 — Surface 캐시]]
- [[03_Architecture/0008_Rendering|Rendering]]
- [[03_Architecture/0008_Rendering|렌더링 구현 검토]]
