# ADR 0000 — Architecture Decision Records 색인

> **한 줄 요약:** ADR을 분류별 표로 정리해 안건과 한 줄 요약을 함께 찾을 수 있도록 한다.

- 분류: **Architecture**
- 상태: **Index**
- 날짜: 2026-09-29
- 관련 문서: [[../00_Start/Templates/0002_ADR|ADR 템플릿]]

## Context

ADR은 문서 내부의 분류 항목에 따라 Architecture, Assets, Rendering, Simulation으로 나뉜다. 기존 색인에는 ADR 링크만 있어 안건과 한 줄 요약을 표에서 함께 확인할 수 없었다.

## Decision

이 문서를 ADR 색인으로 사용한다. 분류마다 `문서 링크`, `안건`, `한 줄 요약` 열을 둔다. 각 행은 원본 ADR의 제목과 한 줄 요약을 사용한다.

## Alternatives Considered

기존 `README.md`의 분류별 링크 목록을 유지하는 방식이 사용되고 있었다. 표에서 안건과 한 줄 요약을 함께 볼 수 있도록 이 ADR 색인으로 확장한다.

## Consequences

ADR의 주제별 분류와 각 문서의 요약을 한 곳에서 살펴볼 수 있다. 분류와 요약을 바꾸면 색인 표도 함께 갱신해야 한다.

## Related

- [[../00_Start/Templates/0002_ADR|ADR 템플릿]]

## 분류별 ADR

### Architecture

| 문서 링크 | 안건 | 한 줄 요약 |
| --- | --- | --- |
| [[0005-Per-Texel-GPU-Data-Layout|ADR 0005]] | Per-Texel GPU Data Layout과 Dense InputDelta | GPU 텍셀 데이터 배치와 각 데이터의 공유·인스턴스 소유 범위를 결정한다. |
| [[0006-Dynamic-State-Registry|ADR 0006]] | SRProfile 기반 동적 State Registry | State 종류는 로드한 `.SRProfile`의 `states` key에서 Registry로 구성한다. |
| [[0010-Dynamic-State-GPU-Buffer-Layout|ADR 0010]] | Dynamic State GPU Buffer Layout | GPU State buffer는 Registry의 런타임 채널 수에 맞춘 packed texel-major 배열로 저장한다. |
| [[0011-GPU-Resource-Initialization-and-ABI|ADR 0011]] | GPU Resource Initialization, Descriptors, and CPU↔GPU ABI | GPU resource의 초기값, CPU·GLSL 데이터 ABI와 descriptor 연결 계약을 결정한다. |
| [[0013-InputDelta-Host-Upload-Synchronization|ADR 0013]] | InputDelta Host Upload 동기화 | CPU가 InputDelta buffer를 갱신할 때 필요한 host upload와 GPU 동기화 방식을 결정한다. |
| [[0014-Surface-Contact-Target-API|ADR 0014]] | Surface Contact Target API | 접촉 입력을 대상 Surface에 전달하는 공개 API와 Collider 연결 방식을 결정한다. |
| [[0027-Scene-State-Registry-and-Shared-Profile-Table|ADR 0027]] | Scene별 State Registry와 공유 Profile GPU 테이블 | Scene별 State Registry와 Profile GPU 테이블의 구성 및 공유 범위를 정한다. |

### Assets

| 문서 링크 | 안건 | 한 줄 요약 |
| --- | --- | --- |
| [[0004-Asset-Profile-Mapping|ADR 0004]] | Surface / Material / SRProfile 연결 | 하나의 Surface는 하나의 Render Material과 하나의 SRProfile을 사용한다. |
| [[0007-Surface-Preprocessed-Asset|ADR 0007]] | 정적 Surface 전처리 에셋 | Mesh당 단일 캐시를 택했던 초기 결정이며, ADR 0008을 거쳐 ADR 0026의 해상도별 캐시로 대체됐다. |
| [[0008-Runtime-Surface-Preprocessing|ADR 0008]] | Runtime Surface 전처리 | 실행 중 전처리만 유지했던 중간 결정이며, 현재는 ADR 0026의 해상도별 `.Surface` 캐시를 따른다. |
| [[0009-Texel-Profile-Index-Map|ADR 0009]] | Texel별 Profile Index Map | 시뮬레이션에 참여하는 유효 texel은 자신이 사용할 SRProfile 테이블 항목의 `ProfileIndex` 하나를 가진다. |
| [[0012-Scene-Profile-Distribution-Reference|ADR 0012]] | Scene별 Surface Profile Map 참조 | Scene에서 Surface Profile Distribution Map을 참조하고 적용하는 방식을 결정한다. |
| [[0026-Resolution-Surface-Cache|ADR 0026]] | 해상도별 Surface 전처리 캐시 | Surface 전처리 결과를 해상도별 `.Surface` 캐시로 재사용한다. |

### Rendering

| 문서 링크 | 안건 | 한 줄 요약 |
| --- | --- | --- |
| [[0028-Accumulation-Height-and-Normal-Map|ADR 0028 — Rendering]] | Accumulation Height와 Normal Map 렌더링 | Normal Map에서 복원한 Virtual Meso Geometry와 동적 `AccumulationHeight`를 한 번씩만 최종 표면 방향에 반영한다. |

| [[0035-Accumulation-Debug-and-Texel-Inspector|ADR 0035]] | 적층 디버그 뷰와 Texel Inspector | 두 적층 디버그 뷰와 선택 texel의 완료 GPU snapshot으로 상태량·높이·형상 표시를 검사한다. |
| [[0036-Texel-Geometry-Preview|ADR 0036]] | Texel 연결면 기반 형상 미리보기 | Compute의 texel별 위치·법선을 같은 UV chart의 연결 삼각형으로 표시한다. |

### Simulation

| 문서 링크 | 안건 | 한 줄 요약 |
| --- | --- | --- |
| [[0001-Capacity-and-Saturation|ADR 0001]] | Capacity와 Saturation | `stateCapacity`를 State별 SRProfile 독립 파라미터로 둔다. |
| [[0002-Transport-Drive-and-Weight|ADR 0002]] | Transport Drive와 TransferWeight 분리 | Transport에서 `SaturationDrive`, `GeometryDrive`, `TransferWeight`의 역할을 분리한다. |
| [[0003-Dynamic-Accumulation-Geometry|ADR 0003]] | Accumulation Height의 동적 형상 반영 | 적층을 `Cavity Filling + Surface Following`으로 나눈다. |
| [[0015-Geometry-Driven-Transport|ADR 0015]] | Geometry-Driven Transport의 높이·방향·거리 계약 | `GeometryDrive`를 `HeightDrive × DirectionDrive`로 계산해 `TransferWeight`와 결합한다. |
| [[0016-Transport-Transfer-Weights|ADR 0016]] | Transport TransferWeight 계산 계약 | `TransferWeight(i→j)`는 `DistanceWeight × NormalWeight × CurvatureWeight × ProfileBoundaryWeight`다. |
| [[0017-Solver-Transfer-Cache|ADR 0017]] | Solver TransferWeight 캐시와 RawOutgoing 재사용 | 인스턴스별 TransferWeight 캐시와 Pass 1 RawOutgoing 합계 재사용을 채택한다. |
| [[0018-Normal-Map-Meso-Geometry|ADR 0018]] | Normal Map 기반 Virtual Meso Geometry 복원 | Normal Map의 slope를 texel graph에서 적분해 Virtual Height와 곡률 파생값을 생성한다. |
| [[0019-Optional-Curvature-Transfer-Weight|ADR 0019]] | 선택적 사전 계산 CurvatureWeight | 선택형 CurvatureWeight는 기본 OFF이며, 활성화하면 Virtual Height에서 유도한 mean curvature 기반 가중치를 캐시에 적용한다. |
| [[0020-State-Overcapacity-Transport|ADR 0020]] | State A/B에 초과량을 보존하는 Transport | State A/B에 Capacity 초과량을 포함해 보존하고, Saturation 차이에 따른 기존 Transport 경로로 다음 Solver step부터 이동시킨다. |
| [[0021-Directional-RawFlux-Cache|ADR 0021 — Simulation]] | 방향·채널별 RawFlux 캐시와 역방향 gather | Pass 1에서 방향별 RawFlux를 저장하고 Pass 2에서 이웃 source의 역방향 값을 재사용한다. |
| [[0022-Pass1-Source-Reuse|ADR 0022]] | Pass 1 source 재사용과 가용량 0 생략 | Pass 1에서 source별 계산값을 재사용하고 가용량이 0인 source의 계산을 생략한다. |
| [[0023-Simulation-Resolution-Presets|ADR 0023]] | 시뮬레이션 해상도 프리셋 | Surface simulation 해상도를 Low, Medium, High 프리셋으로 선택한다. |
| [[0024-RawFlux-Cache-Comparison|ADR 0024]] | RawFlux 캐시 ON/OFF 비교 | 방향별 RawFlux 캐시의 ON/OFF 성능과 메모리 비용을 비교하는 기준을 정한다. |
| [[0025-Inactive-RawFlux-Write-Elision|ADR 0025]] | 비활성 source의 RawFlux 쓰기 생략 | 가용량이 없는 비활성 source는 RawFlux 슬롯을 갱신하지 않는다. |
| [[0029-Normalized-Transport-Factors|ADR 0029]] | 정규화된 Transport Factor와 Solver 기준 속도 | Profile은 `[0,1]` 계수를 저장하고 Solver가 기준 속도 `1.0`, `100.0`을 곱한다. |
| [[0030-Texel-Area-and-State-Amounts|ADR 0030]] | 텍셀 면적과 State 총량 | 총량을 저장하고 Capacity, 입력, 감쇠를 월드 texel 면적으로 환산한다. |
| [[0031-Geometry-Transport-Mobility|ADR 0031]] | 출발 포화도에 비례하는 Geometry 전달 | Geometry에 상한 없는 출발 State/Capacity를 곱하고 SaturationDrive를 유지한다. |
| [[0032-Accumulated-Simulation-Timestep|ADR 0032]] | 실제 경과 시간을 누적하는 Solver 반복 | 안전 간격으로 반복하고 처리하지 못한 시간은 이월한다. |
| [[0033-Geometry-Rate-Recalibration|ADR 0033]] | Geometry 전달 기준값 재보정 | 기준 Rate를 6000으로 높이고 C++·GLSL과 안전 시간 간격 계산이 공유한다. |
| [[0034-Fixed-Timestep-and-Auto-Substepping|ADR 0034]] | Fixed timestep과 Auto substepping 분리 | 기본 Fixed ON은 1/60초 구간을 사용하고 기본 OFF인 Auto에서만 Transport 조건에 따라 세분화한다. |
