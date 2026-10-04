# Decision 0000 — 설계 결정 색인

> 이 색인은 현재 `05_Decisions` 폴더에 있는 결정문만 모아 보여준다. 각 요약은 첫 문장만 읽어도 문서 주제를 알 수 있도록 쉽게 적었다. 자세한 조건과 과거 결정은 문서 안에서 확인한다.

- 분류: **Architecture**
- Status: **Index**
- Date: 2026-10-05
- 관련 문서: [[01_Project-Policy/Templates/0002_Decision|Decision 템플릿]]

## 사용하는 법

- **상태와 현재 구현**은 문서 첫 부분에서 확인한다.
- **쉽게 읽기**는 문제와 결정의 요점이다.
- 본문에는 정확한 수식, 예외, 구현 상태와 검증 근거가 있다. 후속 결정이 있으면 이전 내용을 과거 기록으로 표시한다.
- 이 색인에는 현재 폴더에 없는 과거 Decision 파일을 링크하지 않는다.

## Architecture

| Decision | 안건 | 쉬운 요약 |
| --- | --- | --- |
| [[05_Decisions/0001_GPU-Resource-Initialization-and-ABI|Decision 0001]] | GPU Resource Initialization, Descriptors, and CPU↔GPU ABI | GPU에 넘길 자료를 명시적으로 포장하고, 읽기 전에 값이 준비되어 있도록 하는 것이다. |
| [[05_Decisions/0002_InputDelta-Host-Upload-Synchronization|Decision 0002]] | InputDelta Host Upload 동기화 | CPU가 접촉 입력을 GPU에 올리는 순서를 정한다. 입력이 있을 때 이전 GPU 작업이 끝난 것을 확인하고 업로드해, CPU와 셰이더가 같은 버퍼를 동시에 건드리지 않게 한다. |
| [[05_Decisions/0007_Scene-State-Registry-and-Shared-Profile-Table|Decision 0007]] | Scene별 State Registry와 공유 Profile GPU 테이블 | 현재 Scene이 실제로 참조하는 Profile만 모아 State Registry와 GPU Profile 표를 만든다. 같은 Profile은 GPU에 한 번만 올리고, 각 instance의 State 값은 따로 둔다. |
| [[05_Decisions/0018_Application-Owned-Surface-State-System|Decision 0018]] | Application 소유 Surface State System | Application이 Surface State 시스템의 생성·수명을 맡고 Renderer는 참조만 보유한다. 렌더링과 시뮬레이션 실행은 함께 기록되지만 소유권은 분리한다. |

## Assets

| Decision | 안건 | 쉬운 요약 |
| --- | --- | --- |
| [[05_Decisions/0006_Resolution-Surface-Cache|Decision 0006]] | 해상도별 Surface 전처리 캐시 | 메시 전처리 결과를 해상도별 `.Surface` 파일로 저장해 다음 실행에서 재사용한다. 입력 내용과 알고리즘 버전이 달라지면 캐시를 다시 만들고, 손상된 파일은 사용하지 않는다. |
| [[05_Decisions/0020_Scene-Referenced-Demo-Animation|Decision 0020]] | Scene 참조형 데모 애니메이션 | Scene이 JSON 애니메이션 파일을 참조하고 공통 재생기가 오브젝트와 카메라를 움직이게 한다. 애니메이션과 Solver는 각자 재생·일시정지하며, 애니메이션 자체는 접촉 입력을 만들지 않는다. |

## Rendering

| Decision | 안건 | 쉬운 요약 |
| --- | --- | --- |
| [[05_Decisions/0014_Accumulation-Debug-and-Texel-Inspector|Decision 0014]] | 적층 디버그 뷰와 Texel Inspector | State 양과 쌓인 높이를 화면에서 확인하고, 선택한 texel의 GPU 결과를 검사하는 디버그 도구를 정한다. 미리보기는 Solver에 영향을 주지 않는다. |
| [[05_Decisions/0015_Texel-Geometry-Preview|Decision 0015]] | Texel 연결면 기반 형상 미리보기 | 시뮬레이션 texel을 잇는 표면을 만들어 높이 변화가 실제 실루엣에 나타나게 한다. 표시용 위치와 법선은 GPU에서 계산하고 Solver 상태는 바꾸지 않는다. |
| [[05_Decisions/0016_Texel-Grid-and-Demo-Lit-Effects|Decision 0016]] | Texel 묶음 Grid와 데모 Lit 반응 | texel 격자를 화면에 표시하고 Wetness·Mud·WaterFilm 등의 데모 상태를 조명에 연결한다. State 종류는 Profile Registry에서 찾으며 고정 채널 번호를 두지 않는다. |
| [[05_Decisions/0017_Source-Topology-Seam-Stitching|Decision 0017]] | 원본 topology 기반 UV seam 봉합 | 원본 삼각형의 연결 관계를 따라 texel 표면을 만들고 UV seam 양쪽의 위치와 변위를 공유한다. 그 결과 seam이 벌어지지 않으며 원본 메시의 열린 경계는 유지한다. |
| [[05_Decisions/0019_Base-Surface-and-Accumulation-Overlay|Decision 0019]] | 원본 표면과 적층 Overlay 분리 렌더링 | 원본 메시를 바닥 표면으로 그리고, 쌓인 상태는 별도 윗면과 경계 옆면으로 그린다. 현재 코드는 Mud·WaterFilm·Lava 오버레이 경로를 갖지만 실행 화면 검증 상태는 별도로 표시한다. |

## Simulation

| Decision | 안건 | 쉬운 요약 |
| --- | --- | --- |
| [[05_Decisions/0003_Normal-Map-Meso-Geometry|Decision 0003]] | Normal Map 기반 Virtual Meso Geometry 복원 | Normal Map의 기울기에서 가상의 세부 높이를 복원하는 방법을 정한다. 이 문서의 오목도 생성 방식은 후속 결정 0022로 바뀌었고, 높이 복원과 Meso 곡률 데이터는 계속 사용한다. |
| [[05_Decisions/0004_State-Overcapacity-Transport|Decision 0004]] | State A/B에 초과량을 보존하는 Transport | State가 Capacity를 넘어도 초과분을 버리지 않고 다음 계산에 남긴다. Capacity는 저장 한도가 아니라 포화도를 재는 기준이며, 실제 유출량은 기존 source 제한으로 제어한다. |
| [[05_Decisions/0005_Inactive-RawFlux-Write-Elision|Decision 0005 · superseded]] | 비활성 source의 RawFlux 쓰기 생략 | 방향별 RawFlux 캐시가 존재하던 시기의 구현 결정을 기록한다. Decision 0025에서 해당 캐시를 제거했다. |
| [[05_Decisions/0008_Normalized-Transport-Factors|Decision 0008]] | 정규화된 Transport Factor와 Solver 기준 속도 | Profile에는 0–1 범위의 이동 계수를 저장하고, 공통 기준 속도는 Solver가 곱한다. 기준 속도는 Decision 0012에서 다시 조정됐으므로 이 문서의 최초 수치는 역사적 기록이다. |
| [[05_Decisions/0009_Texel-Area-and-State-Amounts|Decision 0009]] | 텍셀 면적과 State 총량 | 각 texel에는 총 State 양을 저장하고, texel의 실제 월드 면적에 맞춰 Capacity·입력·감쇠를 계산한다. 그래서 격자 해상도만으로 총량이 불어나지 않게 한다. |
| [[05_Decisions/0010_Geometry-Transport-Mobility|Decision 0010]] | 출발 포화도에 비례하는 Geometry 전달 | 기울기에 따른 Geometry 이동량을 출발점의 State/Capacity에 비례시킨다. 포화도 차이로 퍼지는 경로는 별도로 유지한다. |
| [[05_Decisions/0011_Accumulated-Simulation-Timestep|Decision 0011]] | 실제 경과 시간을 누적하는 Solver 반복 | 실제 경과 시간을 누적해 Solver를 반복하고, 한 번에 처리하지 못한 시간은 넘기는 초기 정책을 기록한다. 현재 Fixed/Auto 및 backlog 정책은 Decision 0013의 후속 결정이 기준이다. |
| [[05_Decisions/0012_Geometry-Rate-Recalibration|Decision 0012]] | Geometry 전달 기준값 재보정 | Geometry 이동이 지나치게 느렸던 공통 기준 속도를 6000으로 올린다. Profile 계수와 Solver의 계산은 C++·GLSL 공용 상수를 사용한다. |
| [[05_Decisions/0013_Fixed-Timestep-and-Auto-Substepping|Decision 0013]] | Fixed timestep과 Auto substepping 분리 | 기본 1/60초 timestep과 자동 세분화를 서로 다른 옵션으로 둔다. Auto가 켜졌을 때만 안전 간격으로 step을 나누고, 과도한 밀린 시간은 제한해 UI에 표시한다. |
| [[05_Decisions/0021_Rotation-Invariant-Transfer-Cache|Decision 0021]] | 순수 회전에 불변인 TransferWeight 캐시 | 물체가 회전해도 변하지 않는 TransferWeight·면적 캐시를 다시 만들지 않는다. 회전에 따라 달라지는 중력 이동 계산은 매 Solver step에 최신 변환을 사용한다. |
| [[05_Decisions/0022_Macro-Meso-Concavity-Field|Decision 0022]] | Macro Mesh와 Normal Map을 반영한 텍셀 오목도 | Mesh의 큰 굴곡과 Normal Map의 작은 굴곡을 합친 표면에서 오목도를 계산한다. 같은 오목도 값을 자연 감소와 방향별 이동 억제에 제공한다. |
| [[05_Decisions/0023_Directional-Cavity-Transport-Retention|Decision 0023]] | 방향별 홈 이탈 억제와 Decay 계수 분리 | 오목한 곳에서 밖으로 나가는 흐름을 줄이는 State별 계수를 자연 감소 계수와 분리한 초기 결정을 기록한다. 현재 필드명과 곡률 감쇠 정책은 Decision 0024을 따른다. |
| [[05_Decisions/0024_Transport-Role-Names-and-Curvature-Removal|Decision 0024]] | Transport 역할별 Profile 키와 곡률 감쇠 제거 | 이동 확산·중력 이동·홈 이탈 저항·자연 감소를 이름과 설정에서 구별한다. 양방향 곡률 감쇠를 제거하고 Profile v4 키를 쓰며 v3 파일은 호환해 읽는다. |
| [[05_Decisions/0025_RawFlux-Cache-Removal|Decision 0025]] | 방향별 RawFlux 캐시 제거 | 방향별 flux scratch와 ON/OFF 경로를 없애고 Pass 2에서 재계산한다. |
