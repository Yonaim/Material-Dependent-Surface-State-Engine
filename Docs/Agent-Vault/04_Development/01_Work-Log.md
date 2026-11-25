# Work Log — 코드 변경 및 문서 동기화

## 2026-11-26 — 격자 프리셋과 직접 입력

- Resolution에 시뮬레이션 셀과 형상 샘플을 두 줄로 배치했다. 각 프리셋의 오른쪽에 가로·세로 입력을 두고, 계산된 격자 정보는 설명에 표시한다. 각 프리셋의 `Scene Default`는 씬을 열 때 읽은 값을 초안에 복원한다.
- Debug 빌드 및 Snow on Fabric의 ImGui 입력 테스트를 통과했다. 두 메뉴로 시뮬레이션 기본값 14 × 10과 형상 기본값 56 × 40을 복원했으며, 적용 전 활성 격자와 다른 초안 설정을 보존하는 것을 확인했다.

## 2026-11-26 — 홈 자동 분할 설정 위치

- `Automatic basin splitting`을 Solver의 `Storage Regions`에 배치했다. 격자 기본값 복원과 분할 설정은 각각의 초안 값을 다루고, 공용 `Apply & Restart`로 적용한다.
- C++ 포맷 및 Debug 빌드를 통과했다. `BasinSplittingOn`을 열어 `Storage Regions`와 `Resolution`이 별도 섹션으로 표시되는 것을 확인했다.

## 2026-11-26 — 흡수수 단독 진단과 온도 수치

- Agent-Vault C++·주석 규칙과 Surface Simulation 계약을 확인했다. Snow on Fabric의 평평한 천과 요철 천 씬에 단독 흡수수 뷰를 지정했다. 온도 범례와 커서 조회는 GPU가 완료한 표시 스냅샷의 계산 셀 온도를 °C로 표시한다. 프레임 읽기 슬롯은 기존 프레임 완료 동기화를 재사용한다.
- [표시와 온도 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md) 및 [코드 맵](../02_Architecture/02_Source/03_Surface-Simulation.md)을 갱신했다.
- MSVC Debug 빌드, Snow on Fabric CPU/GPU 비교와 잘못된 뷰 상태 거부를 확인했다. 에디터에서 흡수수 기본 뷰, 온도 단독 뷰 두 경로, 24.0/24.0°C 기본 범위, 45°C 펄스의 24.0/45.0°C 범위와 45.0°C 셀 툴팁을 확인했다. 에디터 로그에 Vulkan 검증 오류가 없었다.

## 2026-11-26 — Melt Geometry 수심 진단

- GLSL·Region 규칙과 다층 표시 계약을 확인했다. Layer Thickness가 다층에서 수송용 바닥 기준의 연결 수심을 보간하도록 고쳤다. 표시 바닥은 화면에 보일 수 있는 깊이의 상한으로 사용한다.
- 독립 Debug 빌드, 기본 Build의 셰이더 빌드, Vulkan 1.2 SPIR-V 네 모듈, Ice Gate 두 조건 CPU/Vulkan를 확인했다. 전후 캡처와 네 뷰의 픽셀 비교를 검증 문서에 기록했다. 기본 실행 파일은 실행 중이라 링크 단계에서 잠겨 별도 빌드로 전체 검증했다.

## 2026-11-26 — 홈 자동 분할의 매끄러운 바닥 대조

- 기존 눈금 단면 비교에 완만한 높이장의 분할 ON/OFF를 추가했다. 격자, 급수 및 턱 높이를 맞추고 네 조건을 CPU·Vulkan에서 대조했다. [네 조건의 단면·수심 비교](../05_Experiments/04_Surface-Simulation-Validation.md#각진-바닥과-매끄러운-바닥)

## 2026-11-26 — 씬 카드와 뷰포트 표시 메뉴

- 씬 카드의 설명을 기존 한국어로 두고 재질·상태는 영어 이름을 읽도록 연결했다. 현재 비교 조건들의 중복 없는 값을 한 줄씩 불릿으로 표시하고, 굵은 Condition 라벨 오른쪽에 선택기를 놓았다. MSVC는 씬 목록 파일을 UTF-8로 읽는다.
- 뷰포트 Show에 Viewport Guides와 Simulation Overlays 제목·구분선을 넣었다. 예약 주입 위치 토글의 표시 이름을 State Injection으로 바꿨다.
- Debug 빌드 후 Heated Water Channel과 Deposition & Flow Feedback의 한국어 설명, 후자의 복수 상태 불릿과 Condition 배치, Show 팝업을 에디터 화면에서 확인했다.

## 2026-11-26 — Solver 설정 행과 해상도 표기

- `Simulation → Solver`의 Operations, Resolution, Boundary, Time Step에서 왼쪽 라벨과 오른쪽 입력을 정렬했다. Resolution에 시뮬레이션 셀과 형상 샘플의 가로·세로 해상도 및 총개수를 각각 표시한다.
- 변경 범위에 clang-format과 Region 포맷을 적용했다. 기본 실행 파일이 실행 중이어서 별도 Debug 실행 파일을 빌드했다. SnowOnFabric의 실제 에디터에서 `14 x 10 (140 cells)`, `56 x 40 (2240 samples)`, 우측 샘플 프리셋 드롭다운을 확인했다. [UI 구현](../../../Source/Application/Editor/SurfaceSimulation/SurfaceSimulationUI.cpp)

## 2026-11-26 — 홈 자동 분할 전환과 비교 씬

- Solver의 Resolution에 `Automatic basin splitting`을 추가했다. `Apply & Restart`가 저장 영역을 다시 만들고, 기존 씬은 자동 분할을 기본값으로 유지한다.
- `Transport & Boundaries`에 같은 형상·급수로 두 모드를 병렬 재생하는 비교 씬을 등록했다. 수정한 C++의 Region 검사, MSVC Debug 빌드, CPU/Vulkan 대조를 통과했다. 턱을 넘기 전후의 PBR·수심 캡처를 저장했다. [검증 기록](../05_Experiments/04_Surface-Simulation-Validation.md#홈-자동-분할-onoff)

## 2026-11-26 — SnowyCoat 온도 표시 범위

- SnowyCoat의 온도 히트맵 표시 범위를 293.15–303.15 K(20–30°C)로 좁혔다. 297.15 K(24°C)는 범위의 40%에 해당한다.
- 같은 뷰에서 12프레임을 실행해 전후 온도 색이 짙은 보라색에서 청록색으로 바뀌는 것을 확인했다. [전후 캡처](../../../Build/SnowyCoatTemperatureRange/)

## 2026-11-26 — 배수 수로의 계단형과 연속 표면 비교

- `Transport & Boundaries`에 `Drainage Geometry Comparison`을 등록했다. 같은 무유약 테라코타와 급수·기울기에서 기존 계단형 수로와 단차·벽을 곡선으로 연결한 형상을 비교한다. 곡선형 대조군은 새 16비트 높이장으로 작성했다.
- 변경 C++의 clang-format·Region 검사와 별도 MSVC Debug 빌드를 통과했다. CPU/Vulkan에서 두 조건의 배수를 확인하고 6초 PBR·수심을 캡처했다. [검증 기록](../05_Experiments/04_Surface-Simulation-Validation.md#배수-수로의-계단형과-연속-표면-비교)

## 2026-11-26 — Glacier Cut의 Snow 상태와 적설 외관

- 두 Glacier Cut 조건을 `SteelWithWaterAndSnow.SurfaceProfile`과 `Snow` 상태에 연결했다. 절차 씬도 공용 적설 표시를 쓰고, 용암과 물의 표시 좌표가 눈층 높이를 읽는다. 장면 설명과 재질 출처를 갱신했다.
- Debug 빌드, 정점 SPIR-V 및 두 조건의 CPU·Vulkan 대조를 통과했다. 초기 적설과 최종 융해수의 질량을 비교하고 PBR 세 시점을 캡처했다. [수치와 캡처](../05_Experiments/04_Surface-Simulation-Validation.md#강철-수로-위-snow-003-적설)

## 2026-11-26 — Snow 003 눈층 PBR

- SnowyCoat가 참조하는 공용 눈 표시 레이어에 ambientCG Snow 003 색·거칠기와 GL 노멀을 연결했다. 천과 눈은 각각의 맵을 읽고 뷰의 Textures와 Material Normal Maps 전환을 공유한다.
- Debug 빌드, SPIR-V 검증, 두 눈 씬의 CPU·Vulkan 대조와 PBR 비교를 통과했다. 강철 Glacier Cut의 PBR·수심 캡처 회귀도 확인했다. 외관 계약·소스 맵 및 CC0 에셋 출처를 기록했다. [검증과 캡처](../05_Experiments/04_Surface-Simulation-Validation.md#snow-003-적설-외관)

## 2026-11-26 — Glacier Cut 수로 본체의 강철 재질

- 절차 씬도 수송 프로파일과 동일하게 바닥 재질의 PBR 외관을 읽는다. Glacier Cut의 두 조건은 강철 본체와 녹을 수 있는 얼음층을 표시한다. 에디터의 씬 정보 및 자산 패널에 강철 재질을 노출한다.
- Debug 빌드, SPIR-V 4개와 두 조건의 CPU·Vulkan 대조를 통과했다. 얼음량, 고인 물, 기화 누계 및 질량 수지가 이전 측정과 일치했다. 초기 및 8초 캡처를 저장하고 계약·소스 맵을 갱신했다. [검증과 캡처](../05_Experiments/04_Surface-Simulation-Validation.md#수로-본체를-강철로-교체)

## 2026-11-26 — Glacier Cut 얼음과 금속 받침

- Glacier Cut의 두 조건에 표층 얼음 배율 1.75, 접촉 열 감쇠 4.4초와 브러시드 스틸 받침을 적용했다. 얼음 아래의 직접 표시와 굴절은 공용 강철 음영을 사용한다.
- 상태·표시 계약과 소스 맵을 동기화했다. MSVC Debug, SPIR-V 두 개, 두 조건의 CPU·Vulkan 대조 및 PBR 캡처를 확인했다. [수치 비교](../05_Experiments/04_Surface-Simulation-Validation.md#얼음-보충과-금속-받침)

## 2026-11-26 — 면적당 열용량의 두꺼운 시편 가열

- 두꺼운 강판의 열 계산 및 외형 두께를 12 mm에서 1 mm로 맞췄다. 두 판에는 같은 2.00 MJ/m² 에너지 펄스를 적용한다. 씬 조건 이름과 비교 문서를 동기화했다.
- CPU/Vulkan 대조를 통과했다. 공용 발광을 사용한 PBR에서 0.5 mm 판(1333.06 K)의 주황색과 1 mm 판(813.10 K)의 옅은 적색을 확인했다.

## 2026-11-26 — 배수 데모의 무유약 점토와 흡수 자국

- Tilt-Driven Drainage와 Outlet Drainage의 같은 홈 벽돌에 무광 테라코타 재질과 제한된 흡수를 연결했다. 젖은 자국은 바닥 흡수수로 표시한다.
- 기존 Debug 실행 파일의 CPU/Vulkan 대조에서 열린 출구 유출과 닫힌 출구의 0 g 유출을 확인했다. 6초 전후 GPU 캡처를 비교했다. [검증과 캡처](../05_Experiments/04_Surface-Simulation-Validation.md#기울기와-출구-배수의-테라코타-재질)

## 2026-11-26 — Height Map Cavities의 가장자리 급수

- 평면과 높이맵 조건의 6×8 mm 급수 영역을 차트 중심 기준 (−44, +22) mm로 맞췄다. 각 조건의 급수량은 4초 동안 9 g이다. 높이맵은 같은 홈 형상을 공유하는 별도 비교 케이스로 연결했다.
- 두 조건의 CPU/Vulkan 대조에서 9 g 주입과 질량 수지를 확인했다. 높이맵 조건에서 8초 후 바닥 높이 2.5 mm 미만의 홈 영역에 물 2.98 g이 남았다. 에디터 로드 및 가장자리 급수·홈 집수 구간의 PBR 캡처도 확인했다.

## 2026-11-26 — 흡수 비교의 예약 급수 지연

- 첫 급수가 시작되는 `pourStartSeconds`를 지원한다. 스텝과 실제 급수 구간이 겹친 시간으로 질량을 계산하고 에디터 조건 및 Scene Tracks에 시작 시각을 표시한다. 두 흡수 비교는 마른 시편에 2.0–2.25초 동안 물 20 g씩을 준다.
- 포맷·Region 처리와 MSVC Debug 빌드를 통과했다. 분수 스텝에 걸친 급수 시작·종료 검사, 두 비교 씬의 에디터 실행과 CPU/Vulkan 대조에서 주입 전 건조 상태 및 공급 누계 20 g을 확인했다.

## 2026-11-26 — 아래층 수위로 위층 바닥 복원

- CPU·Vulkan 수송과 표시에서 아래층이 채운 위치만 위층 바닥에 반영한다. 각 서브스텝의 시작 수위로 공유 턱과 수용·잔류 부피를 평가하고, 질량을 확정한 뒤 아래에서 위로 수위를 갱신한다.
- 같은 저장 영역에 3 mm 높이의 양쪽 어깨와 홈을 두고 평평할 때와 기울었을 때의 질량·수위를 비교했다. 5 mm 턱의 통과 여부와 실제 월류 기록도 CPU·Vulkan에서 확인했다. [실험 기록](../05_Experiments/04_Surface-Simulation-Validation.md#아래층이-채운-형상과-위층-수위)

## 2026-11-26 — 표면 시뮬레이션 코드 명명과 책임 분리

- 공용 도메인·초기 상태, CPU 기준 스텝, 단일 GPU 솔버, 그래픽 스냅샷을 파일 단위로 분리했다. 대표 액체의 상태와 GPU 읽기 진단의 범위를 타입에 반영했다.
- 장면·실행 옵션·GPU 타이밍과 셰이더 파일명을 갱신했다. Agent-Vault 코드 맵·계약·흐름, CMake 셰이더 목록과 C++ 호출부를 동기화했다.
- 변경 C++의 clang-format과 Region 후처리, MSVC Debug 빌드, Heated Water Channel 에디터 8프레임 실행을 확인했다. 시뮬레이션 셰이더 7개는 Vulkan 1.2 SPIR-V 검사를 통과했고 디버그 심볼 제거 후 전후 바이너리가 일치했다. 텍스처 켜기·끄기 캡처도 각각 전후 PNG가 일치했다.
- Surface Simulation의 Reference·GPU·Capture 회귀 3건을 모두 통과했다. CPU 기준 결과 35개가 이전 빌드와 정확히 일치했고 GPU 검증은 마른 격자와 국소 주입에서 희소·전체 실행의 결과 일치를 확인했다. [CTest 기록](../../../Build/SurfaceNamingVerification/ctest.log)

## 2026-11-26 — 천 비교의 정사각형 물 배치와 흡수 자국

- 초기 물을 양쪽 시편 중앙의 60×60 mm 정사각형에 배치하고 질량을 1.44 g으로 맞췄다. 비교 프로파일의 젖음 표시 기준을 2.5에서 0.35 kg/m²로 낮췄다. 씬 설명과 계약, 실험 문서를 갱신했다.
- 두 조건의 CPU/Vulkan 대조를 통과했다. 평평한 시편의 PBR 캡처에서 초기 물의 경계와 흡수 후 어두워진 자국을 확인했다.

## 2026-11-26 — 천 비교의 초기 물 영역

- Flat & Ruffled Fabric의 두 조건에서 초기 물을 중앙 약 105×66.7 mm 직사각형으로 줄였다. 면적당 물량을 보존하도록 물을 각 2.8 g으로 맞췄다. 씬 설명과 Agent-Vault 계약 및 실험 결과를 갱신했다.
- 비교 씬의 두 조건이 CPU/Vulkan 대조를 통과했다. 편집기에서 0초의 물 영역과 마른 가장자리, 0.1× 기본 배속을 확인했다.

## 2026-11-26 — 씬 비교의 저속 재생

- 타임라인 배속 메뉴에 0.05×와 0.1×를 추가했다. `initialPlaybackRate`는 메뉴가 지원하는 값만 받는다. Flat & Ruffled Fabric의 두 조건을 0.1×로 열도록 작성하고 에디터와 씬 계약을 동기화했다.
- 포맷·Region 처리와 MSVC Debug 빌드를 통과했다. 두 조건의 CPU 기준 시계열과 요약이 이전 결과와 일치했다. 실제 에디터에서 0.1× 초기 배속과 0.05×·0.1× 메뉴 항목을 확인했다.

## 2026-11-26 — 씬과 에디터의 셀당 형상 표본

- Agent-Vault C++·주석·문서 규칙과 Surface Simulation 계약을 확인했다. 단일 씬과 비교 에디터의 해상도 선택을 셀당 1×1, 2×2, 4×4, 8×8 및 Custom으로 바꾸고 전체 형상 격자를 숫자로 표시했다.
- 씬에서 `geometrySamplesPerCell`은 셀당 비율을, `geometrySamples`는 직접 지정한 전체 격자를 설정한다. 기존 크기에 정확히 맞는 23개 씬을 비율로 전환했다. 나머지 21개에는 기존 전체 격자 설정을 유지했다. [씬·에디터 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md#00-경계와-단위)과 개인 볼트의 에셋 형식 문서를 갱신했다.
- MSVC Debug 빌드, 셀당 비율 유지·상한·직접 입력 검사, 프리셋·Custom 씬의 에디터 초기 프레임을 확인했다. 두 씬 형식으로 실행한 Terraced Catchment의 CPU 결과가 일치했고 새 형식의 CPU·GPU 비교와 배포 씬 전체 CPU 기준 회귀를 통과했다. 중복 키, 미지원 비율, 비정수 비율 및 형상 범위를 위반한 입력은 로더에서 거부했다.

## 2026-11-26 — ThinFilm 프로파일의 수송 바인딩 간소화

- 프로파일 로더와 에셋 12개에서 중복 모델 지정을 제거했다. 대표 액체의 상태 ID는 transport.state에서 읽는다. Agent-Vault 계약과 개인 볼트의 현재 에셋 예시를 갱신했다.
- clang-format·Region 처리와 MSVC Debug 빌드를 통과했다. Snow on Fabric, Ice Gate, Brick Runoff의 CPU·Vulkan 검사를 실행했다. [검증 로그](../../../Build/Transport-State-Only)

## 2026-11-26 — 조건별 세 뷰와 스펀지 깊이 젖음

- 씬 요약의 여백을 16×14 px로 늘리고 제목 아래에 구분선을 넣었다. 목록에서 비교 씬을 고르면 Compare All로 열며, 각 조건을 외관·Layer States·Substrate States로 나눴다. 세 뷰는 한 조건의 솔버와 렌더 타깃을 공유한다.
- Flat & Ruffled Fabric은 두 천 전체에 물 5.6 g씩을 배분하고 0초에 정지한다. 스펀지·강철 시편은 같은 노멀 복원 형상을 수송에 쓰며 양수 두께의 바닥 메시를 닫는다. 렌더링 문서 I03-01과 그 Rationale의 평균 보존 선형 깊이 분포를 윗면·옆면·밑면에 적용했다. 8초는 깊은 시편에 표면 물이 약 1% 남은 시점에서 잡은 시연 기준이다. 두께별 감쇠 시간은 0.317 s와 7.916 s이며 확산계수는 시연 설정이다.
- 포맷·Region 처리, MSVC Debug, 그래픽 셰이더 SPIR-V, 세 비교 묶음 CPU/Vulkan 대조를 통과했다. 에디터에서 Flat & Ruffled Fabric이 목록 클릭으로 6뷰·0초 정지에 진입하는 동작과 선택된 조건의 진단 패널을 확인했다. 3·4조건 진입을 각 5프레임 실행했다. 최신 스펀지 씬은 열렸고, 깊이 분포 반영 뒤의 밑면 시각 검사는 컴퓨터 사용이 중지되어 완료하지 않았다.

## 2026-11-26 — ThinFilm 실행 통합과 씬 검색 제거

- Code Style, C++·GLSL, 주석·Region과 Surface Simulation 계약을 확인했다. 12개 수송 프로파일을 공통 ThinFilm 스키마로 묶었다. CPU·GPU는 각 층의 경계 유량과 같은 셀의 영역·바닥 규칙을 한 흐름에서 실행한다. 외투의 셀 단위 눈·흡수수와 고정 스텝당 한 번의 건조를 보존했다.
- Scenes 패널의 텍스트 검색 위젯, 검색 상태와 문자열 필터를 제거했다. 그룹과 비교 조건 선택은 유지했다. Agent-Vault 계약·코드 맵·에디터 문서 및 개인 볼트의 모델·GPU 패스와 분류 그림을 맞췄다.
- clang-format·Region 처리, MSVC Debug, Vulkan 1.2 SPIR-V, 15개 씬 CPU/Vulkan 검사, Python 6건, 비교 씬 실행과 외투 캡처를 확인했다. [측정 결과](../05_Experiments/04_Surface-Simulation-Validation.md#thinfilm-실행-경로-통합)

## 2026-11-26 — 씬 설명과 조건 제어의 강조 테두리

- `Scenes`에서 제목, 설명, 재질·상태 정보와 `Condition`을 하나의 요약 영역에 배치했다. 2 px 테마 강조색 테두리와 좌우 12 px, 상하 10 px 내부 여백을 적용했다. 에디터 배치 문서를 동기화했다.
- clang-format과 Region 후처리 후 MSVC Debug 빌드를 통과했다. 에디터에서 Sill Overflow의 설명 줄바꿈과 요약 테두리, Condition 메뉴가 열리는 동작을 확인했다.

## 2026-11-26 — 에디터 데모 분류와 조건 선택

- Features 16개와 Showcase 5개를 씬 선택기에 등록했다. 드롭다운은 전체, Features, 다섯 하위 분류와 Showcase를 제공한다. Features와 전체 목록은 현재 표시하는 분류 사이를 구분선으로 나눈다.
- 같은 기능의 조건을 한 항목에 묶었다. 개별 조건은 원본 씬을 열며, 2–4개를 나란히 띄우면 시간과 카메라를 공유한다. 형상 입력 비교는 메시, 높이 맵, 변위 맵과 노멀 맵을 실제 소스로 읽는다. 평평한 흡수 시편은 같은 면적에 초기 물 20 g을 균일하게 배분한다.
- clang-format·Region 처리와 MSVC Debug 빌드 후 비교 묶음 12개의 CPU 기준 검사를 수행했다. 형상 소스, 월류, 바닥 흡수, 저장 깊이의 Vulkan 검사를 통과했다. 4·3·2조건 비교 씬을 완성 빌드에서 각각 5프레임 실행했다. 최신 창에서 드롭다운의 다섯 하위 분류와 Geometry Inputs 필터 적용을 확인했다. 8초 흡수량은 스펀지/강철 19.84/0.00 g, 얕은/깊은 스펀지 5.00/19.84 g이다.
- 씬 비교 계약을 편집하며 잘못 덮인 인접 설명은 작업 전 출력과 실행 경로를 대조해 복구했다.

## 2026-11-26 — 층별 수송 물성과 유효 바닥

- Code Style, C++·GLSL, 주석·Region 규칙과 Surface Simulation의 수송·상태 계약을 확인했다. 단일층도 공유 재질의 안식각과 항복응력을 읽는다. CPU는 같은 면 유량 함수, GPU는 같은 이동성 함수와 면 거리 계산을 두 경로에서 사용한다. 다층의 유효 바닥은 각 수송 서브스텝의 시작 질량과 층 순서로 계산한다.
- clang-format·Region 처리 후 MSVC Debug 빌드와 Vulkan 1.2 SPIR-V 검사를 통과했다. 단일·다층의 뉴턴 극한, 마찰·항복 조건, 정지한 아래층의 턱과 월류를 검증했다. 외투, 얼음 마개, 흙이 덮인 돌길과 계단형 수조의 CPU/Vulkan 검사 및 Python 기준 검사 6건을 통과했다. 수송 계약·소스 맵·개인 볼트의 Transport 및 결합 문서를 갱신했다. [검증 결과](../05_Experiments/04_Surface-Simulation-Validation.md#층별-이동성과-정지한-아래층의-턱)

## 2026-11-26 — 흡수 시편과 요철 비교의 이름 및 분류

- Substrate Absorption을 스펀지와 강철 두 시편으로 정리했다. 시편별 면적, 격자와 초기 물 20 g을 유지했다. Wetted Contact를 Surface Relief & Wetting으로 바꾸고 이미지 목록의 Geometry Inputs로 옮겼다.
- Region 후처리와 MSVC Debug 빌드를 수행했다. 두 씬의 PBR을 캡처했고 흡수 비교의 CPU/Vulkan 검증에서 8초 흡수량 19.84 g / 0.00 g을 확인했다. 검증 문서의 씬 이름과 데이터를 갱신했다.

## 2026-11-26 — 평평한 흡수 비교 시편

- 편집기 씬 설명을 실제 에셋에 맞췄다. Substrate Absorption의 세 시편에서 동일한 요철을 제거하고 재질·시편 두께·급수량을 유지했다. 실험 문서와 소스 맵을 갱신하고, 목록 이미지에서 Interactions을 별도 분류로 복원했다.
- Region 후처리·MSVC Debug 빌드 후 PBR 캡처와 CPU/Vulkan 대조를 확인했다. 높이차는 0 mm이며 8초 후 흡수량은 천 6.50 g, 스펀지 19.84 g, 강철 0.00 g이다.

## 눈금 유리 용기와 물 수조의 돌 받침

- Code Style, C++·GLSL, 단면 표시 계약을 확인했다. 왁스에는 전면 눈금을 포함한 유리 용기 메시와 안쪽 물질 옆면을 추가했다. 물 수조는 오른쪽의 기존 12 mm 바닥 단차만 돌 표면으로 그린다. 두 표현을 저작 에셋 필드로 연결하고 계약을 기록했다.
- Region 후처리, 셰이더·MSVC Debug 빌드, 두 씬의 PBR 캡처와 CPU·Vulkan 대조를 확인했다. 이전 검증 보고서의 수치가 같으며, 일반 가열 수로의 PBR 역시 픽셀이 일치한다.

## 투명 단면 하부의 외관

- GLSL과 Surface Simulation 표시 계약을 확인했다. 공용 눈금창의 불투명 바닥 단면을 반투명한 유리색으로 바꿨다. 바닥 높이는 얇은 선으로 표시한다. 표시 계약을 갱신했다.
- Region 서식과 셰이더 빌드를 적용하고 두 씬의 초기·완료 PBR을 캡처했다. 단면창을 쓰지 않는 Heated Water Channel의 PBR은 변경 전과 픽셀이 일치한다.

## 2026-11-26 — 투명 단면 눈금과 계단 수조 평형

- Code Style, C++·GLSL, Surface Simulation 표시 계약을 확인했다. 공용 앞쪽 단면과 눈금 메시를 `.Scene.sectionWindow`에 연결했다. 왁스와 계단 수조가 같은 파이프라인과 표시 스냅샷으로 높이를 나타낸다. 에디터가 물의 초기 단차에서 멈추도록 `startPaused`를 연결했다.
- 포맷과 Region 후처리, 셰이더·MSVC Debug 빌드를 수행했다. 두 씬의 CPU·Vulkan 검증, 초기·최종 PBR 비교, 물의 네 가지 보기 캡처를 확인했다. 기존 가열 수로 31프레임 PBR은 이전 캡처와 RGB 픽셀이 일치한다. [계측 및 캡처](../05_Experiments/05_Surface-Physics-Comparison.md#바닥-높이가-다른-두-구역의-수면-평형)

## 2026-11-26 — 같은 질량의 왁스 상별 부피

- 엔진의 상별 밀도와 층 높이 재구성 계약을 확인했다. 닫힌 평저 트레이의 같은 시편을 동일 구도의 고체·액체 단계로 시연하고, 층별 실험 시계열에 질량과 밀도로 산출한 부피를 담았다. 형상 입력과 검증 기록을 갱신했다.
- 포맷과 Region 적용, 별도 Debug 빌드, CPU·Vulkan 대조와 PBR 캡처를 확인했다. 12 g 고체와 액체의 부피는 각각 13.636 mL와 15.385 mL다. [검증 기록](../05_Experiments/05_Surface-Physics-Comparison.md#같은-질량의-왁스-상별-부피)

## 2026-11-26 — 공유 열발광과 왁스 경사로 표기

- Code Style·C++·주석 규칙과 Surface Simulation 계약을 확인했다. 발광 활성 조건을 강철 재질과 온도 상태 연결로 옮겼다. 건식 강판과 물 접촉 냉각이 같은 PBR 발광 곡선을 사용한다. 왁스의 가열·냉각부는 기존 chart albedo 슬롯에서 읽는 표식이다.
- 포맷·Region 처리와 별도 Debug 빌드 뒤, 강판 두 조건·가열 수로·왁스·흡수 시편의 CPU·Vulkan 대조를 통과했다. 가열 수로의 31프레임 PBR은 변경 전후 픽셀 단위로 같고, 왁스 상별 질량 시계열도 일치했다. 공용 렌더 계약, 텍스처 목록과 검증 기록을 갱신했다. [비교 기록](../05_Experiments/05_Surface-Physics-Comparison.md#가열냉각의-시각적-단서)

## 2026-11-26 — 기능 데모의 건식 가열과 왁스 상변화

- Code Style·C++·GLSL과 Surface Simulation 계약을 확인했다. 강판 비교에 면적당 에너지 입력을 연결하고 액체 상태를 걷었다. 액체를 선언한 장면은 그 상태명을 입력 도구에 표시한다. 가열 수로의 접촉 냉각은 기존 모델을 사용한다.
- 공유 박막 상변화에 평형점 이하 응고를 추가했다. Wax Melt & Set은 지정 온도장을 사용해 용융, 수송 및 낮은 쪽의 응고를 관찰한다. 씬 선택기에서는 왁스를 대표 상변화 장면으로 열고 물의 동결 계측은 파일과 검증 경로를 사용한다. 기존 건조·사전 적심 발화 대조를 Moisture Suppression of Ignition으로 표기했다.
- 포맷·Region 처리와 셰이더·MSVC Debug 빌드를 수행했다. 두 강판과 왁스의 CPU/Vulkan을 대조하고 PBR 캡처를 검토했다. Ice Gate, Rewetting Delta, Heated Water Channel 및 Freeze and Thaw 회귀를 확인했다. 현재 열려 있는 에디터와 별도 실행 파일에서 검증했다. [계측과 의도](../05_Experiments/05_Surface-Physics-Comparison.md#기능-단위-시연과-복합-시연)

## 2026-11-26 — 평평한 강판의 열용량 비교

- 열용량 비교 에셋의 상면을 같은 직사각형 평판으로 바꿨다. 공통 평면 Height Map과 중앙 급수를 사용하고 경계는 두 조건 모두 닫았다. 바닥 형상 및 온도 계약을 확인하고 형상 에셋 설명과 비교 기록을 갱신했다.
- 기존 Debug 실행 파일에서 시편별 CPU·Vulkan 대조 검사와 세 시점의 PBR 캡처를 수행했다. 두 시편의 바닥 높이 편차는 0 mm이며 에셋의 차이는 시편 ID, 열용량 프로파일과 몸체 두께다. [검증 기록](../05_Experiments/05_Surface-Physics-Comparison.md#평평한-강판-시편-비교)

## 2026-11-26 — 열용량 편차와 동결 부피 시연

- Code Style·C++·주석 및 표면 온도·상변화 계약을 확인했다. 열용량 비교의 유효 두께와 표시용 몸체 두께를 0.5 mm 및 12 mm로 바꾸고 씬 및 에디터 표기를 동기화했다. 표준 동결 씬의 표시 제목과 설명을 부피 변화에 맞췄다.
- 포맷과 Region 적용 후 별도 MSVC Debug 실행 파일을 만들었다. 강판 두 조건과 표준 트레이의 CPU·Vulkan 대조 검사가 통과했다. 동일 시각 강판 PBR과 고정 카메라 액상·동결·해빙 PBR을 캡처했다. 기존 실행 중인 에디터의 메뉴 클릭 검증은 수행하지 않았다. [비교 측정](../05_Experiments/05_Surface-Physics-Comparison.md#면적당-열용량과-동결해빙)

## 2026-11-26 — 씬의 활성 상태 범례

- Code Style·C++·Source Comments와 Surface Simulation 상태 계약을 확인했다. 순수 유동, 흡수 시편, 진흙 퇴적과 재습윤의 프로파일에서 사용하지 않는 온도 상태를 제거했다. `WaterRunoff`는 유동 상태만 선언하고, 가열 수로의 온도·열교환 연결은 기존 프로파일에서 유지한다.
- 에디터는 온도가 선언된 경우에만 온도 뷰를 제공하고, 상태 그룹이 비어 있으면 범례를 생략한다. CPU 검증은 주변 온도와 다른 온도를 사용하는 경우에 바탕 상태 선언을 확인한다. [상태 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md)
- 실행 중인 에디터가 기본 실행 파일을 사용하고 있어 별도 경로에 MSVC Debug를 링크했다. CPU 기준 35건과 벽돌 유출·흡수 시편·퇴적·가열 수로의 Vulkan 검사 4건을 통과했다. 새 PBR 비교 캡처를 확인했다. 실행 중인 편집기의 메뉴 클릭 검증은 수행하지 않았다. [검증 로그](../../../Build/SceneReview-20261126/PBR/reference.log)

## 2026-11-26 — 단일 물층의 얕은 물가 복원

- Code Style·GLSL·Region과 Surface Simulation 표시 계약을 확인했다. 완만한 접촉에서 한쪽 수위가 턱을 넘고 양쪽 저장 영역에 물이 있을 때 표시 수위를 연결한다. 수면 메시에 있는 정점과 프래그먼트의 수심 절단을 재사용하고 표시 계약을 갱신했다.
- Shader 빌드, SPIR-V 네 모듈, CPU/Vulkan 검사와 텍스처 캡처 회귀를 통과했다. 초원 280스텝의 저장 영역 경계에서 젖음과 마름이 바뀌는 픽셀 쌍이 834개에서 278개로 줄었다. 전후 뷰포트 평균은 1.8668 → 1.8743 ms였다. [전후 캡처와 측정 조건](../05_Experiments/04_Surface-Simulation-Validation.md#단일-물층의-얕은-물가)

## 2026-11-26 — 연결된 단일 물층의 수면 표시

- Code Style·GLSL·Region과 Surface Simulation 표시 계약을 확인했다. 단일 물층의 수면 정점과 수직 연결 면에 같은 연결 수위 복원을 적용했다. 한쪽만 잠긴 턱에서는 양쪽 수위가 독립적으로 유지된다. 표시 계약과 검증 문서를 갱신했다.
- Region 점검, Debug 셰이더 빌드, SPIR-V 네 모듈, 초원·쌍둥이 홈의 CPU/Vulkan 검사와 텍스처 캡처 스크립트가 통과했다. 초원 경계 평균 수심차는 0.3451 → 0.0454 mm였다. 쌍둥이 홈의 반대쪽 중심은 마른 상태를 유지했고 원시 수심은 픽셀로 일치했다. [전후 그림과 표시 부피](../05_Experiments/04_Surface-Simulation-Validation.md#단일-물층의-수면-경계-연결)

## 2026-11-26 — 비교 씬 해상도 조절

- 단일 씬과 비교 씬이 형상 샘플과 계산 셀 해상도 입력을 공유한다. 비교 씬을 처음 열면 각 원본 해상도를 사용한다. 해상도 변경을 적용하거나 되돌릴 때 두 시뮬레이션과 렌더 리소스를 함께 교체한다.
- clang-format과 Region 정리 후 MSVC Debug 빌드를 통과했다. Meadow Firebreak의 원본과 두 해상도 변경 조합이 CPU/Vulkan 검사를 통과했다. 다른 한 조합의 GPU 질량 수지 한계 초과를 실험 문서에 기록했다. 에디터 검증 창이 최소화된 상태에서 활성화가 막혀 적용 버튼의 클릭 검증은 완료하지 못했다. [좌우 비교 검증](../05_Experiments/04_Surface-Simulation-Validation.md#기존-씬의-좌우-비교)

## 2026-11-26 — 씬의 직접 Height Map 입력

- Code Style·C++·주석 규칙과 Surface Simulation 입력 및 실험 계약을 확인했다. 배포 씬의 외부 형상 입력을 씬 상대 PNG 경로와 미터 범위로 통일했다. 카탈로그와 그 전용 UI·CLI를 제거하고 기존 형상 세트는 세 개의 Cavity 씬으로 이관했다. 원본 실험 파일과 재질용 노멀 맵을 보존했다.
- 기존 용암 장면의 256×256 바닥을 16비트 PNG로 베이크했다. Cavity의 기존 Height Map 실행 두 건과 새 직접 참조 실행은 CPU/Vulkan 보고서가 일치했다. 단일 포켓, 쌍둥이 홈, 계단형 수로와 벽돌 용암의 CPU/Vulkan 검사를 통과했다. 용암의 기본 격자 높이 최대 차이는 0.0000505 mm였고, 저장 영역 수는 8,454개에서 8,448개로 바뀌었다.
- clang-format·Region 처리 후 MSVC Debug를 빌드하고 Cavity PBR과 액체 두께를 150프레임으로 캡처했다. 캡처에 기존 프레젠테이션 레이아웃 검증 경고가 나타났다. Agent-Vault 실행 계약, ADR 및 개인 볼트의 형상 근거를 동기화했다. [ADR-0003](../03_Decisions/00_Records/ADR-0003-Scene-Height-Maps.md) · [캡처](../../../Build/HeightMapSceneInput/Cavity-HeightMap.png)

## 2026-11-26 — YAML 저작 에셋과 프로파일 상태 정의

- Code Style, C++와 주석 규칙, Surface Simulation 에셋 및 로더 계약을 확인했다. 용융·흡수·건조 규칙 파일 네 개와 일반 프로파일 여덟 개를 `.Interaction` 및 `interactions[].use` 형식으로 변경했다. Scene Setup의 분류·파일명을 실제 에셋과 맞췄다. 구형 규칙 에셋과 프로파일 바인딩 로더를 제거했다.
- `.PhysicalMaterial`, `.Interaction`, `.SurfaceProfile`의 본문을 YAML로 변환했다. 파일 확장자로 종류를 구분하고 버전과 ID를 보존했다. 상태 정의는 프로파일의 `states.layer`와 `states.substrate`로 옮겼다. 절차형 씬 16개는 상태 구성을 공유하는 프로파일 4개를 참조한다. 별도 `States/` 파일과 참조 로더를 제거하고, 에셋 검사기는 상태의 출처를 해당 프로파일로 표시한다.
- 재질·규칙·프로파일 33개에 값의 쓰임새와 단위를 설명하는 한국어 주석을 추가했다. 에셋 계약과 소스 지도를 갱신했다.
- clang-format과 Region 포맷터 적용 후 MSVC Debug 빌드를 통과했다. 배포 씬 33개의 에셋 참조, 구형 규칙 바인딩 거부와 중복 YAML 키 거부를 확인했다. 최종 CPU 기준과 Vulkan 검사에서 각각 장면 결과 40건 및 월류 계약 보고서 1건이 비교 기준과 일치했다. [CPU 검사](../../../Build/InteractionAssetMigration/inline-reference.log) · [Vulkan 검사](../../../Build/InteractionAssetMigration/inline-vulkan.log)

## 2026-11-25 — 표면 접촉·퇴적·열용량과 수송 세분화

- Code Style, C++·GLSL 규칙, 상태·형상·GPU 계약을 확인했다. 형상 샘플에서 접촉 면적을 얻고, 진흙의 혼합수 질량·건조·재습윤을 CPU와 Vulkan에 연결했다. 퇴적이 만드는 유효 바닥과 확정 월류를 표시한다. 고정 스텝 내부의 수송 횟수 선택과 바닥 온도 상태의 면적당 열용량 설정을 추가했다.
- Wetted Contact, Rewetting Delta, Thin vs Thick Steel 비교 씬과 수심·접촉·유효 바닥·월류·흡수 충전율·진흙 성분 진단을 추가했다. 실행 계약과 소스 지도를 갱신하고 ADR-0002에 결정 근거를 적었다.
- 수정 파일은 clang-format과 Region 포맷터를 거쳐 Debug로 빌드했다. 최종 CPU/Vulkan 회귀 40건과 별도 64×64 수송 1회·8회, 고정 턱·접촉 시험, SPIR-V 모듈 7개를 통과했다. 세 비교 씬의 좌우 캡처도 확인했다. 같은 카메라의 전후 캡처와 12개 해상도·수송 구성의 GPU 시간을 기록했다. [결과와 증거](../05_Experiments/05_Surface-Physics-Comparison.md)

## 2026-11-25 — 천·스펀지·강철 흡수 비교

- 코드와 GLSL 작성 규칙, 표면 상태 및 표시 계약을 확인했다. 같은 형상과 물 20 g을 갖는 시편을 세 개로 확장했다. 천은 Fabric 080, 스펀지는 Sponge 001, 강철은 Metal 010을 쓴다. [시편 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md#바닥-저장-용량과-재질-비교)
- Debug 빌드와 SPIR-V를 검사하고 재질 비교, 기존 천 및 가열 수로 CPU/GPU 검사와 구형 두 시편 자산 검사를 통과했다. 에디터 실행 및 시작·8초 후 렌더와 계산·형상 격자를 확인했다. [측정값과 캡처](../05_Experiments/04_Surface-Simulation-Validation.md#천스펀지강철의-흡수-용량-비교)

## 2026-11-25 — Fabric 수심을 형상 표본에서 복원

형상 표본을 56×40으로 고정하고 계산 격자만 14×10, 28×20, 56×40으로 변경해 6초 프레임을 비교했다. 이전 수심 보간에서는 계산 격자를 촘촘하게 할수록 경사면에 줄이 반복됐다. 단일 액체 두께 뷰에서 먼저 형상 표본의 수심을 얻고, 연결된 수심·수위와 바닥에 같은 선형 가중치를 적용했다. 완만한 접촉 양쪽에 물이 있고 수위가 턱을 넘는 구간은 연결하도록 했다. 변경 후 56×40에서 반복 줄이 크게 줄었고, 14×10에는 웅덩이 가장자리에 옅은 셀 자국이 남았다. 셰이더 빌드, 원시 14×10 캡처의 픽셀 일치, 두 홈의 전후 캡처, CPU·GPU 월류 시험을 확인했다. [같은 프레임 비교](../05_Experiments/Artifacts/SurfaceSimulation/FabricSampleDepthReconstruction.png)

## 2026-11-25 — 주름진 천의 두께 뷰 경계 보정

단일 액체의 보간 두께 뷰는 저장 수위에서 보간한 형상 표본의 바닥 높이를 뺀 뒤, 연결되지 않은 영역이 보간에서 차지하는 비중에 따라 경계 수심을 낮춘다. 같은 카메라와 색상 범위의 6초 Fabric 프레임에서 직전 보간과 비교했다. 표시한 125×32 픽셀 확대 구간에서 건조 바탕보다 녹색 채널이 9단계 이상 높은 푸른색 픽셀은 162개에서 81개로 감소했다. 이 수치는 그림의 국소 비교이며 전체 표면의 수심 오차를 나타내지 않는다. 세 캡처에서 보간을 끈 원시 뷰는 최초 기준과 픽셀이 일치했고, CPU·GPU 턱 월류 시험도 통과했다. [확대 비교](../05_Experiments/Artifacts/SurfaceSimulation/FabricWaterDepthBoundaryComparison.png)

## 2026-11-25 — 단일 액체 두께 진단의 연결 보간

`Layer Thickness`에서 `Smooth liquid boundary`를 켰을 때 연결된 수위만 보간한다. 근처 계산 형상 표본이 마른 곳에서는 렌더 바닥의 깊이만으로 생긴 물가 자국을 줄인다. Debug 빌드와 셰이더 컴파일 후 Fabric 6초와 두 홈의 1.5초·3.67초를 비교했다. 보간을 끈 캡처 세 장은 변경 전과 픽셀이 일치했다. 보간을 켜면 두 홈이 이어진 수로의 계단과 Fabric의 일부 얇은 띠가 줄었다. Fabric에는 마른 경계에 닿는 가는 자국이 남는다. CPU·GPU 월류 기준 시험에서 턱 아래 수신 영역의 액체량은 0이었고, 턱을 넘긴 뒤 수신 영역에 물이 도달했다. [전후 캡처](../05_Experiments/Artifacts/SurfaceSimulation/WaterThicknessInterpolationComparison.png)

## 2026-11-25 — 원본 씬의 좌우 비교

- 코드 규칙과 Surface Simulation·에디터 계약을 확인했다. 기존 변형 가운데 13개를 참조형 비교 씬으로 등록했다. 독립 시뮬레이션 두 개가 고정 스텝, 재생 조작과 카메라를 공유한다. [좌우 비교 씬 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md#좌우-비교-씬)
- MSVC Debug 빌드, 비교 파일 13개의 CPU 기준과 GPU 검사, 에디터의 목록·두 씬 전환·동기 재생 및 좌우 PNG 출력을 확인했다. [검증과 캡처](../05_Experiments/04_Surface-Simulation-Validation.md#기존-씬의-좌우-비교)

## 2026-11-25 — 천과 스펀지 시편의 간격과 요철

- 시편을 26 mm 떨어뜨려 표시하고 25 mm 몸체와 안쪽 옆면을 그린다. 두 시편에 같은 2.77 mm 높이차를 형상 샘플에 넣고 양쪽 수송 경계를 닫았다. [비교 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md#바닥-저장-용량과-재질-비교)
- Debug 빌드와 표면 셰이더 검증, 비교 씬과 기존 천 씬의 CPU/GPU 검사에 통과했다. 시작·8초 후 상태 및 계산 격자와 형상 격자 캡처를 확인했다. [실험 결과](../05_Experiments/04_Surface-Simulation-Validation.md#천과-스펀지의-흡수-용량-비교)

## 2026-11-25 — 흡수 용량과 씬 비교 분류

- 코드 작성 규칙, 표면 상태 계약과 에디터 맵을 읽었다. `SurfaceProfile`이 면적당 흡수 상한을 읽도록 하고 흡수 속도는 `LocalUpdate`에서 공유한다. Fabric vs Sponge를 한 차트의 닫힌 두 시편으로 구성했다. [바닥 저장 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md#바닥-저장-용량과-재질-비교)
- 씬 브라우저를 Showcase, Phenomena, Comparisons, Experiments로 분류하고 변형은 같은 씬 계열에 뒀다. [에디터 배치](../02_Architecture/02_Source/00_CPP/07_Scene-Input-DebugUI.md#에디터-배치)
- MSVC Debug 빌드, 비교 씬과 기존 천 씬의 CPU/GPU 검증을 통과했다. 시작과 8초 프레임을 확인했고, 에디터의 Comparisons 카테고리에서 Fabric vs Sponge가 나오는 것을 확인했다. [비교 데이터](../05_Experiments/04_Surface-Simulation-Validation.md#천과-스펀지의-흡수-용량-비교)

## 2026-11-25 — 상태 범례의 공통 자동 팔레트

- 다층 표시 계약과 일반·다층 상태 등록 경로를 확인했다. 씬별 진단색 지정 대신 공통 함수로 보색 쌍을 생성한다. 개별 프로파일에는 피복용 색을 보관한다. [표시 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md#다층-박막-프로파일)과 [코드 맵](../02_Architecture/02_Source/03_Surface-Simulation.md)을 갱신했다.
- MSVC Debug 및 Soil Covered Stone Path·Snow on Fabric의 전후 GPU 캡처를 확인했다. 두 씬의 PBR과 개별 두께에는 픽셀 차이가 없었고 상태 색만 바뀌었다. 비교 자료는 `Documents/MDSSP-Previews/StateLegendPalette-2026-11-25`에 보관했다.

## 2026-11-25 — 마른 흙·진흙 범례 대비

- 다층 표시 계약과 장면 로더를 확인했다. `SoilCoveredStonePath` 프로파일의 `displayColor`로 마른 흙은 밝은 황금색, 진흙은 밝은 청록색을 지정했다. [표시 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md#다층-박막-프로파일)에 기본값과 적용 범위를 추가했다.
- MSVC Debug 빌드, 150프레임 Vulkan 캡처 비교 및 Soil Covered Stone Path CPU/GPU 검사를 통과했다. 상태 진단은 51,209픽셀이 바뀌었고 PBR과 두 개별 두께 뷰는 0픽셀 차이였다. 비교 이미지는 `Documents/MDSSP-Previews/SoilMudLegend-2026-11-25`에 보관했다.

## 2026-11-25 — Fabric 젖음의 색과 반사

기본·주름 Fabric의 젖은 부분에서 바탕색과 거칠기를 더 낮추고, PBR 반사에 쓸 직조 법선의 영향도 젖음에 따라 줄였다. 표면 물의 표시용 광학 경로 배율을 6으로 낮춰 물막 아래의 섬유색이 드러나게 했다. 셰이더를 빌드하고 두 씬의 첫 프레임이 전후 같은 픽셀인지 확인했다. 360번째 프레임에서 젖은 영역의 어두운 색과 각도에 따른 밝은 반사를 비교했다. Textile CPU·GPU 검증의 질량 장부, 시간 계열, 최종 상태값은 전후 일치했다. 캡처의 Vulkan present 레이아웃 경고는 변경 전후 모두 기록됐다. [동일 프레임 PBR 비교](../05_Experiments/Artifacts/SurfaceSimulation/Fabric080_WetnessComparison.png)

## 2026-11-25 — ambientCG 재질과 HDR 환경 반사

- C++ 주석·포맷 규칙, 표면 표시 계약과 씬 코드 맵을 확인했다. 씬에서 반복 알베도·거칠기와 노멀을 지정하고 HDR 파노라마의 반사, 노출, 수평 회전을 설정할 수 있게 했다. 바닥 재질 세 종과 HDRI 세 종의 배포 경로 및 CC0 출처를 동기화했다. [표시 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md#03-재질-표시와-캡처) · [에셋 출처](../../../Assets/Textures/SurfaceSimulation/README.md)
- MSVC Debug, 표시 셰이더 SPIR-V 여섯 개, 실제 Vulkan 캡처, Brick Runoff CPU/GPU 검사와 텍스처 캡처 회귀를 통과했다. 두 쌍의 두께 뷰에서 픽셀 차이가 0이었다. [검증](../05_Experiments/04_Surface-Simulation-Validation.md#ambientcg-데모-재질과-hdr-환경-반사)

## 2026-11-25 — 다층 표시 상태를 프로파일에서 등록

Surface Simulation 계약과 C++ 규칙을 확인했다. 다층 프로파일의 모든 층을 수송과 표시에서 함께 읽고 최상단 액체의 수심 뷰를 자동 선택한다. 얼음·돌길의 기본 씬과 대조군에서 중복 상태 배열을 제거했다. 기본 Debug 빌드, 다층 씬 다섯 건의 액체 및 아래층 두께 캡처, 이전 다층 형식과 기존 벽돌 두께 캡처를 확인했다. 두 기본 씬은 CPU와 Vulkan의 질량·수위 비교도 통과했다.

## 2026-11-25 — Snow on Fabric의 Fabric080 재질

기본·주름 씬의 이름과 파일명을 Fabric으로 통일했다. 두 씬이 공유하는 Fabric 재질에 ambientCG 1K 원본 비율의 색·거칠기와 GL 노멀·높이를 연결했다. 높이 채널은 0.25 mm 표시용 시차에 사용한다. Debug 빌드 후 같은 75번째 프레임을 1200×780 PBR로 캡처했다. 평평한 천에서 전체 텍스처를 끈 경우와 재질 노멀만 끈 경우 모두 픽셀 차이를 확인했다. [기본 씬](../05_Experiments/Artifacts/SurfaceSimulation/SnowOnFabric_Fabric080.png) · [주름 씬](../05_Experiments/Artifacts/SurfaceSimulation/SnowOnFabric_Ruffled_Fabric080.png)

## 2026-11-25 — 다층 두께 뷰와 씬 재질

다층 수송의 질량·수위 슬롯을 표시 스냅샷에 연결했다. 두께 뷰는 물의 저장 수위와 해당 형상 표본, 또는 선택한 아래층의 질량·밀도로 값을 구한다. Ice Gate는 공용 얼음 피복 셰이더를 사용한다. 돌길은 Poly Haven의 회색 돌 디퓨즈와 노말을 사용하며, 흙과 진흙이 덮인 부분에는 돌의 노말 강도를 감쇠한다. Agent-Vault C++·주석 규칙과 표면 표시 계약을 확인했다. 기본 Debug 빌드 및 돌길·얼음·벽돌 진단 캡처를 확인했다.

## 2026-11-25 — 엔진 다층 박막과 갈림길 데모

공통 마찰·항복 수송을 C++와 Vulkan에 연결하고 다층 프로파일, 얼음 마개, 흙 덮인 돌길 및 대조 씬을 추가했다. 코드 작성 전 Agent-Vault C++·주석 규칙, Surface Simulation 계약, 소스 맵을 읽었다. 코드·계약·검증 문서를 동기화했다. Debug는 실행 중인 기본 바이너리의 잠금 때문에 별도 출력 경로에 링크했다. 수송 계약, 기존 벽돌, 두 기본 데모와 대조군의 CPU/GPU 검증 및 캡처를 수행했다. [[05_Experiments/04_Surface-Simulation-Validation#다층 박막과 두 갈림길]]

## 2026-11-25 — 다층 박막 CPU 기준 계산

공통 마찰·항복식을 독립 기준 계산에 추가했다. 설계 볼트의 재질 표와 수학식, 프로파일 예시를 동기화했다. Agent-Vault의 Python·주석 규칙과 Surface Simulation 계약을 확인했다. 닫힌 직교 격자와 유량 함수에 대한 단위 시험 여섯 건을 통과했다. [[05_Experiments/04_Surface-Simulation-Validation#다층 박막 CPU 기준 계산]]

> **모든 코드 변경 작업에서 작업 단위 최소 1건 기록 필수.** 상세 설계는 Architecture/ADR, 실제 테스트 결과는 Experiments에 기록한다.

| 날짜 | 작업 / 코드 범위 | 읽은 주요 문서 | 갱신 문서 또는 영향 없음 근거 | 검증 상태 |
|---|---|---|---|---|
| `YYYY-MM-DD` | `{{TASK_AND_CODE_PATHS}}` | `{{READ_DOCS}}` | `{{UPDATED_DOCS_OR_NO_IMPACT_REASON}}` | `{{COMMAND_AND_RESULT_OR_NOT_RUN}}` |

## 2026-11-26 — Assets를 열 때 발생한 시작 충돌

- 시작 흐름과 에셋 편집기를 확인했다. `run.ps1`은 빌드를 마쳤고 `MDSS.exe`는 시작 프레임의 `ImGui::TableNextRow`에서 `0xC0000005`로 종료했다. 인라인 상태 설명 행을 `Bindings` 테이블 안에서 그리도록 수정했다. 변경 범위는 해당 행의 ImGui 테이블 생성 순서다.
- MSVC Debug 빌드를 통과했다. 인수 없는 `run.ps1`에서 창이 유지되고 정상 닫기가 되는 것을 확인했으며, `run.ps1 -Configuration Debug --frames 12`도 종료 코드 0으로 완료했다.

## 2026-11-25 — 수면 출렁임 표시 두 가지

- C++·GLSL 작성과 접기 규칙, 표시 스냅샷과 수위 계약을 확인했다. 활성 뷰에서 반사·굴절과 높이 변위를 독립적으로 켜도록 하고, 기울기 변화의 감쇠 진동을 표시 슬롯으로 전달했다. [표시 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md#수면-출렁임-표시)과 [코드 맵](../02_Architecture/02_Source/03_Surface-Simulation.md#출렁임의-광학과-수면-높이)을 갱신했다.
- MSVC와 셰이더 Debug 빌드, SPIR-V 검사, CPU 기준, Brick Runoff CPU/GPU, 텍스처 캡처 회귀를 통과했다. 네 가지 설정의 PBR 캡처, 위에서 본 GPU 수심·젖은 영역 비교 두 건, 정지 시점 이미지 동일성, 교차 GPU 표시 시간 측정을 남겼다. [검증 결과](../05_Experiments/04_Surface-Simulation-Validation.md#수면-출렁임-표시-검증)

## 2026-11-25 — 주입 브러시 프리셋과 발광 안내

- 코드 작성·주석·포맷 규칙, 수동 주입 계약과 에디터 배치를 확인했다. 주입 브러시를 두 열로 배치하고 수치와 단계가 있는 선택창, 크기 슬라이더, 입력 설명을 추가했다. 가열 수로의 온도 범위와 셰이더 발광 안내를 [에디터 문서](../02_Architecture/02_Source/00_CPP/07_Scene-Input-DebugUI.md#에디터-배치)에 동기화했다.
- MSVC Debug 빌드와 가열 수로 CPU·GPU 검사를 통과했다. 새 에디터 창에서 물 0.35 mL 프리셋, 1200°C 프리셋과 주황색 설명, 원 반경 12.0–22.2 mm 변경, 일시정지 중 온도 주입으로 바닥 상태 뷰가 갱신되는 것을 확인했다.

## 2026-11-25 — 매끄러운 바닥 위 수면의 내부 틈

- C++·GLSL 작성과 접기 규칙, 표면 표시 계약을 확인했다. 굴절과 바닥 가림을 따로 끈 캡처에서 수면 자체의 틈을 재현했다. 매끄러운 바닥에서는 두 수위가 공유 턱을 덮은 모서리에 수직 연결 면을 그린다. [표시 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md)과 [코드 맵](../02_Architecture/02_Source/03_Surface-Simulation.md)을 갱신했다.
- Debug 셰이더와 별도 실행 파일을 빌드했다. Twin Pockets 캡처의 내부 빈 픽셀은 449개에서 8개로 줄었고 위에서 본 수심·젖음·질량은 같았다. Brick Runoff PBR, Twin Pockets CPU/GPU 검사와 텍스처 캡처 회귀를 확인했다. [검증 결과](../05_Experiments/04_Surface-Simulation-Validation.md#비스듬한-시점의-수면-내부-틈)

## 2026-11-25 — 물가 스무딩과 표시 수심 분리

- GLSL 작성·접기 규칙과 표면 표시 계약을 확인했다. 저장 수위로 수면 정점, 가시 경계, 두께 뷰를 계산하고, 주변 수심 보간은 바닥 젖음에만 적용한다. [표시 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md)과 [코드 맵](../02_Architecture/02_Source/03_Surface-Simulation.md)을 갱신했다.
- Debug 셰이더 빌드와 SPIR-V 검증을 통과했다. 다섯 Vulkan 스냅샷에서 스무딩 ON/OFF 수심과 젖은 영역을 비교했고, 세 씬의 PBR·두께 뷰를 캡처했다. 기본 CTest 실행 파일은 바닥 온도 상태를 읽는 구형 로더에서 실패했다. 현재 씬 형식을 지원하는 실행 파일로 전체 CPU 기준, Brick Runoff·Twin Pockets GPU 검사와 텍스처 캡처 회귀를 통과했다. [측정 결과](../05_Experiments/04_Surface-Simulation-Validation.md#물가-스무딩과-표시-수심-분리)

## 2026-11-25 — 공통 바닥 온도 상태

- 코드 작성·주석·포맷 규칙을 확인했다. 씬 로더가 `SubstrateTemperature`를 공통 등록하고, CPU·GPU가 물 반응 전에 셀의 바닥 온도를 갱신한다. Snow on Canvas의 용융 규칙을 이 상태에 연결했다. [상태 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md)과 [코드 맵](../02_Architecture/02_Source/03_Surface-Simulation.md)을 갱신했다.
- 표준 실행 파일을 사용 중인 에디터와 분리해 MSVC Debug와 계산 셰이더를 빌드했다. 배포 씬과 변형의 `--reference-surface` 및 `--check-surface`를 통과했다. CPU·GPU에서 물 반응 없이 선택한 셀의 온도 입력과 입력 종료를 확인했다. 최종 실행 파일의 Snow on Canvas 검사와 벽돌 바닥 상태 뷰 캡처를 확인했다. 로그와 캡처는 `Build/SubstrateTemperature`에 보관했다.

## 2026-11-25 — 뷰포트 헤더, 수동 주입, 로그 조작

- 코드 작성 규칙과 [에디터 배치](../02_Architecture/02_Source/00_CPP/07_Scene-Input-DebugUI.md#에디터-배치), [예약 주입과 경계 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md#자동-급수와-물-출처-표시)을 확인했다. 타임라인과 씬별 상태를 뷰포트 위로 옮기고 Layer & Substrate를 기본으로 적용했다. 선택 뷰의 외곽 여백에 테두리를 표시하고 푸른 옥색 강조를 적용했다. Log의 검색·레벨·동작은 패널 상단에 펼쳐 둔다.
- 씬을 바꿀 때 수동 주입이 꺼지는 기존 동작을 확인했다. 브러시 활성 상태를 유지하도록 변경하고 에디터 문서, 주입 계약, 출구 ADR을 갱신했다.
- MSVC Debug 빌드와 벽돌 CPU/GPU 검증을 통과했다. 에디터에서 시작 4분할, Exit Compare, 하단·오른쪽 선택 테두리, 한 줄 Log 조작을 확인했다. 벽돌과 Meadow Firebreak Dry에서 일시정지 중 Space로 0.150 mL를 주입해 다음 스텝에 들어가는 것을 확인했다. 화면 캡처는 `Build/ViewportLayout-20261125/Viewport_and_Log.jpg`에 저장했다.

## 2026-11-25 — 옥색 에디터 테마와 예약 주입 표시

- 코드 작성 규칙, 에디터 배치와 예약 주입 계약을 확인했다. ImGui 공통 강조색과 적용 버튼을 옥색으로 통일하고, 예약 주입을 받는 저장 영역의 윗면과 바깥 테두리를 붉게 채운다. [에디터 배치](../02_Architecture/02_Source/00_CPP/07_Scene-Input-DebugUI.md#에디터-배치) · [예약 주입 표시](../02_Architecture/01_Contracts/08_Surface-Simulation.md#자동-급수와-물-출처-표시)
- MSVC Debug 빌드와 벽돌 CPU/GPU 검증을 통과했다. 에디터에서 옥색 탭·체크 표시와 벽돌 위 붉은 반투명 주입 영역을 확인했다.

## 2026-11-25 — 도킹 패널과 솔버 연산 전환

- 코드 작성·주석·빌드 규칙, 표면 시뮬레이션 계약과 에디터 코드 맵을 읽었다. [에디터 배치](../02_Architecture/02_Source/00_CPP/07_Scene-Input-DebugUI.md#에디터-배치)에 좌하단 Scene Setup, 우측 솔버·렌더링, 중앙 타임라인과 하단 실험 패널의 조작을 기록했다.
- CPU 기준과 Vulkan에서 내부 수송·외부 교환을 독립적으로 전환한다. GPU 헤더 플래그와 바닥 표시 자원의 재사용에 맞춰 [상태 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md)과 [코드 맵](../02_Architecture/02_Source/03_Surface-Simulation.md)을 갱신했다.
- Debug 빌드, CPU 기준, 벽돌 GPU 비교, 계산 SPIR-V와 텍스처 캡처를 확인했다. 실제 에디터에서 일시 정지 상태의 표시 변경과 급수를 끈 실험을 확인했다. [실행 검증](../05_Experiments/04_Surface-Simulation-Validation.md#도킹-배치와-솔버-연산-전환)

## 2026-11-25 — 마른 턱과 물가의 표시 조건 분리

- GLSL 작성 규칙과 표시 계약을 확인했다. 고인 수면과 물가의 연결 조건을 분리하고, 보간한 높이차의 0점에서 물가를 표시했다. [표시 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md)
- Debug 셰이더와 SPIR-V 네 개, 텍스처 캡처 회귀를 확인했다. Cavity와 두 포켓 초기·후반, 벽돌 용암의 PBR·두께를 고정 카메라에서 비교했다. [수로 윤곽과 마른 턱 검사](../05_Experiments/04_Surface-Simulation-Validation.md#마른-턱과-수로-물가의-표시-분리)

## 2026-11-25 — 에셋 속성의 용도별 섹션

- 시뮬레이션·표시 계약과 밀도 필드의 소비 경로를 확인했다. `Assets` 속성에 `Physics`, `Appearance`, `Bindings`, `Inputs` 섹션을 적용하고 해당 속성이 없는 섹션은 그리지 않는다. [Surface Simulation 코드 맵](../02_Architecture/02_Source/03_Surface-Simulation.md)
- MSVC Debug 빌드를 통과했다. 실제 에디터에서 `Water`의 Physics·Appearance와 `SnowyCoat`의 Bindings·Appearance 배치를 확인했다. 다른 에셋의 섹션은 코드에서 확인했다.

## 2026-11-25 — 에셋 패널의 한국어 도움말

- 에셋 및 표시 계약과 에디터 코드 맵을 확인했다. `Assets`에서 분류·에셋·속성에 붙은 번역 표기를 없애고 각 항목에 한국어 도움말을 달았다. 값의 단위와 편집 경로는 유지한다. 배포 씬은 기존 씬 설명을 재사용한다. [Surface Simulation 코드 맵](../02_Architecture/02_Source/03_Surface-Simulation.md)
- MSVC Debug 빌드와 실제 에디터의 영문 분류·제목·속성 표시를 확인했다. ImGui 입력 검사에서 속성 이름과 값 위의 툴팁 및 기존 값 편집을 확인했다.

## 2026-11-25 — 연결된 액체 수면만 표시 보간

- GLSL 규칙과 표면 표시 계약을 확인했다. 양쪽 수위가 턱을 덮은 접촉만 연결하고 정점에서 받은 보간 전 수심으로 마른 영역의 표시를 제한했다. PBR과 두께 뷰에 같은 표시 수심을 사용한다. [표시 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md)
- Debug 셰이더 빌드, 네 SPIR-V, 벽돌과 cavity GPU 비교, 텍스처 캡처 회귀 및 같은 구도의 전후 PNG를 확인했다. [비교 수치와 캡처](../05_Experiments/04_Surface-Simulation-Validation.md#연결된-수면의-표시-보간)

## 2026-11-25 — PBR에서 용암 줄눈과 얇은 막 표시

- GLSL 규칙과 표면 표시 계약을 확인했다. 수면 경계 보간을 켠 PBR에서 얕은 발광막을 표시 두께로 바닥색에 합성한다. 벽돌 프로파일에 표시 전환 깊이 0.9 mm를 지정했다. [표시 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md)
- 셰이더 빌드, 두 SPIR-V 모듈, 12초·32초 Vulkan 캡처 및 두께·레이어 상태의 픽셀 비교를 확인했다. [확대 비교](../../../Build/BrickCubeMortarFlow/Mortar-Rendering-Comparison.png)

## 2026-11-25 — PBR 수면 경계 기본값

- 표시 계약과 C++·서식 규칙을 확인했다. 씬 시작, 뷰 모드 선택, 비교 프리셋 및 캡처의 PBR 기본값으로 수면 경계 보간을 켰다. 뷰의 체크박스와 캡처의 명시적 옵션도 적용된다. [표시 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md)
- Debug를 빌드한 뒤 `run.ps1`로 기존 에디터를 열었다. BrickCube Lava의 PBR에서 기본 체크 상태를 확인하고, 15.888초에 멈춘 화면을 확대했다. 두꺼운 용암 내부는 이어져 보였고 얕은 가장자리에는 바닥이 드러나는 자리가 남았다. [에디터 확대 캡처](../../../Build/PbrBoundaryEditor/Pbr-Lava-Closeup.png)

## 2026-11-25 — 독립 해상도 및 씬별 GPU 저장

- Surface Simulation 계약과 C++·GLSL·빌드 전 서식 규칙을 확인했다. 노멀을 원본 크기 또는 지정 격자에서 적분하고, 같은 높이장을 형상 격자로 면적 재표본화한다. 에디터에서 적분·형상·수송 해상도를 각각 조정한다. 계산·정적 표시·표시 스냅샷의 SSBO는 실제 요소 수로 할당한다. [형상·GPU 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md)
- MSVC Debug, 변경 SPIR-V 검사, `run.ps1` 편집기 시작, 512²·250×190 적분, 128² 최대 수송 격자와 두 BrickCube 조건의 CPU·GPU 수지를 확인했다. [정량 비교와 캡처](../05_Experiments/04_Surface-Simulation-Validation.md#독립-적분형상수송-해상도)

## 2026-11-25 — 노멀 형상 강도와 액상 용암 색

- 형상·표시 계약과 C++·GLSL·빌드 전 서식 규칙을 확인했다. Geometry 탭에서 노멀 형상 강도와 적용 방식을 명시하고 BrickCube의 씬 기본 배율을 1.5로 조정했다. 발광 액상의 외관에 주황색 바탕과 흐름 무늬를 적용했다. [형상·표시 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md)
- Debug 빌드, SPIR-V 검사, BrickCube CPU·GPU 수치와 동일 구도 전후 캡처, 빙하 PBR 캡처를 확인했다. [검증 결과](../05_Experiments/04_Surface-Simulation-Validation.md#줄눈을-따라-흐르는-용암과-pbr-노멀)

## 2026-11-25 — 비교 프리셋의 2×2 배치

- 표시 계약과 뷰포트 슬롯 순서를 확인했다. `Layer & Substrate`의 우측에 바탕 상태와 표면층 두께를 두고, `Elevation & Grids`에는 등고선과 계산 셀·형상 샘플을 배치했다. README와 표시 계약 및 [비교 프리셋 코드 맵](../02_Architecture/02_Source/03_Surface-Simulation.md)을 갱신했다.
- MSVC Debug 빌드, 두 프리셋의 슬롯 순서와 비교 종료 ImGui 입력 검사, Heated Water Channel의 두 배치 렌더 캡처를 확인했다.

## 2026-11-25 — BrickCube 줄눈 수송과 PBR 노멀 연결

- Surface Simulation의 형상·표시 계약과 C++·서식 규칙을 확인했다. `BrickCube Lava` 급수 영역을 노멀에서 복원한 줄눈 쪽으로 좁혔다. `chartNormal`을 통해 같은 UV의 BrickCube 노멀을 PBR에도 사용한다.
- [에셋·표시 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md)과 [평탄 대조군·PBR 토글 실험](../05_Experiments/04_Surface-Simulation-Validation.md#줄눈을-따라-흐르는-용암과-pbr-노멀)을 갱신했다.
- Debug 빌드, 에디터 12프레임 실행, 두 형상의 CPU–GPU 검사, PBR 토글의 픽셀 비교를 통과했다. 토글한 두께 뷰의 표본은 일치했다.

## 2026-11-25 — 고정 가열 수로와 초원의 젖은 띠

- 코드 작성 규칙과 Surface Simulation 계약을 검토했다. 가열 수로의 연료 패드와 탄화 규칙을 제거했다. 고정 경사와 면적 급수로 물길을 만들었다. 초원과 건조 비교 씬을 추가하고, 이전 스텝의 불꽃 활성도가 이웃의 온도를 구동하게 했다. [Surface Simulation 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md)
- MSVC Debug, 강철 수로와 초원 두 조건의 CPU·GPU 비교, 질량 수지 및 RGB 캡처 검사를 통과했다. [시연 검증](../05_Experiments/04_Surface-Simulation-Validation.md#고정-가열-수로와-초원의-젖은-띠)

## 2026-11-25 — Scene Setup 에셋 분류와 속성 행

- 에디터 배치, Surface Simulation 코드 맵과 에셋 설정 계약을 확인했다. `Assets`를 `Scene Setup`의 첫 탭에 배치하고 에셋 선택 메뉴를 종류별로 나눴다. 선택한 에셋의 한영 제목은 크게 표시하고 속성 이름과 편집기를 같은 행에 둔다. [에디터 배치](../02_Architecture/02_Source/00_CPP/07_Scene-Input-DebugUI.md#에디터-배치) · [Surface Simulation 코드 맵](../02_Architecture/02_Source/03_Surface-Simulation.md)
- MSVC Debug 빌드를 통과했다. 실제 에디터에서 `Assets`의 기본 선택, 재질 제목과 한영 속성 행을 확인했다. ImGui 입력 검사에서 밀도 값의 편집이 초안에 반영되는 것을 확인했다.

## 2026-11-25 — 뷰포트 도구 모음 정리

- 에디터 배치와 예약 급수 표시 계약을 확인했다. 렌더 모드와 `Show`는 왼쪽에 두고, 재생·한 스텝·재시작 아이콘과 배속·시간·급수 상태를 중앙에 배치했다. 뷰포트가 좁으면 재생 줄을 분리한다.
- 오른쪽 `Layout`의 뷰 개수 선택을 제거하고 비교 프리셋과 `Exit Compare`를 남겼다. `Profiling`은 시작 시 표시하며 `Window` 메뉴로 다시 열 수 있다. [에디터 배치](../02_Architecture/02_Source/00_CPP/07_Scene-Input-DebugUI.md#에디터-배치)
- MSVC Debug 빌드를 통과했다. 실제 에디터에서 중앙 배치, 기본 프로파일링 표시와 일시정지를 확인했다. 한 스텝, 재시작, 두 표면 비교 프리셋과 비교 종료는 ImGui 입력 검사로 확인했다.

## 2026-11-25 — 씬 목록의 한국어 툴팁

- 에디터 코드 맵과 씬 에셋 및 변형 비교 기록을 확인했다. `Scenes`에 기본 9개와 변형 9개의 한국어 제목과 설명을 추가했다. 변형에는 기본 씬과 다른 설정과 관찰할 현상을 적었다. [에디터 배치](../02_Architecture/02_Source/00_CPP/07_Scene-Input-DebugUI.md#에디터-배치)
- MSVC Debug 빌드, 기본·변형 에셋 18개의 설명 대응, 실행 파일의 UTF-8 문구를 확인했다. 실제 에디터에서 가열 수로의 출구 닫힘 변형에 커서를 올려 제목과 한글 설명의 줄바꿈을 확인했다.

## 2026-11-24 — Experiments PNG 미리보기와 확대 창

- 에디터 배치와 Surface Simulation 상태·표시 수명 계약을 확인했다. `Run`과 `Capture State Plot`의 기본 결과를 PNG로 저장하고 패널에 미리보기를 표시한다. `Capture → State Data & Plot`은 JSON·SVG도 저장한다. [에디터 배치](../02_Architecture/02_Source/00_CPP/07_Scene-Input-DebugUI.md#에디터-배치)
- 실험과 PNG 생성은 백그라운드에서 진행한다. 비교에 별도 장치·큐를 사용하며, 텍스처는 에디터에서 안전한 프레임 경계에 교체한다. [코드 맵](../02_Architecture/02_Source/03_Surface-Simulation.md#기존-에디터-연결) · [표시 수명 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md#기존-에디터에서의-표시-수명)
- MSVC Debug 빌드, CPU 기준 CTest, Cascade GPU 검사, PNG·SVG 생성 모드를 통과했다. 실제 에디터에서 상태 캡처·PNG 단독 저장·인라인 표시·확대를 확인했고, `Close`와 `Esc`는 ImGui 입력 검사로 확인했다. CPU/GPU 비교의 UI 버튼은 직접 확인하지 않았다.

## 2026-11-24 — 기본 씬의 변형 8개

- Surface Simulation 에셋·표시·입력 계약, 검증 문서와 C++·서식 규칙을 확인했다. 출구, 사전 적심, 트레이 너비, 홈 형상, 급수 폭과 시간, 접촉 열 감소 시간, 최대 기울기를 각 씬 에셋에 설정했다. 파일 접두어에 따라 씬 목록과 전체 검사가 변형을 찾는다.
- README, [표시·입력 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md), [코드 맵](../02_Architecture/02_Source/03_Surface-Simulation.md), [8쌍 비교 실험](../05_Experiments/04_Surface-Simulation-Validation.md#기본-씬과-변형-8쌍)을 갱신했다.
- Debug 빌드와 전체 Surface Simulation CPU–GPU 검사를 통과했다. 8쌍의 입력 차이와 지표를 확인하고, 각 장면의 같은 시각 PBR·진단 뷰 PNG 16장을 저장했다. 쌍둥이 홈 씬을 에디터에서 열어 12프레임을 실행했다.

## 2026-11-24 — Profiling 기본 도킹과 표시 전환

- C++ 규칙과 에디터 배치를 확인했다. `Profiling`을 좌측 도구 창과 뷰포트 사이에 기본 표시하고, 상단 토글의 표시 상태를 색으로 구분한다. 저장된 도킹 위치를 유지하며 위치가 없는 작업공간에 기본 도킹을 추가한다. [에디터 배치](../02_Architecture/02_Source/00_CPP/07_Scene-Input-DebugUI.md#에디터-배치)
- MSVC Debug 빌드를 통과했다. 실제 에디터에서 기본 표시와 버튼으로 숨길 때 뷰포트가 넓어지는 동작을 확인했다. ImGui 도킹 검사에서 새 레이아웃, 저장 위치 유지, 이전 레이아웃에 도킹 추가, 다시 켤 때 같은 위치와 폭 복원을 확인했다.

## 2026-11-24 — 씬 변형의 이름 맞추기

- Surface Simulation 에셋·표시 계약과 C++·서식 규칙을 확인했다. 변형 에셋 파일과 ID를 `SnowOnCanvas_Ruffled`로 맞췄다. 씬 목록에서 `Snow on Canvas_Ruffled`를 기본 씬과 같은 `Demos`에 배치했다.
- README, [표시 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md)과 [대조 실험](../05_Experiments/04_Surface-Simulation-Validation.md#평면과-요철-바닥-대조)의 링크를 갱신했다.
- Debug 빌드, 새 경로로 에디터 12프레임 실행, GPU 씬 검사를 통과했다.

## 2026-11-24 — 시뮬레이션 작업공간과 표면 주입 브러시

- C++ 작성 규칙과 에디터 흐름, Surface Simulation의 주입·수송 및 GPU 배치 계약을 확인했다.
- 좌측 도킹과 상단 재생 도구를 정리했다. 우측은 `Simulation`과 `Scene Setup`으로 나누고 설정 초안·적용·되돌리기를 공유한다. 새 레이아웃은 `EditorWorkspace-v2.ini`에 저장한다.
- 점·원·회전 사각형의 물·고체층 주입과 셀 온도 펄스를 연결했다. 면적 입력의 총량은 차트 안 표본 겹침에 따라 보존하며, CPU와 GPU가 고정 스텝 입력을 공유한다.
- [에디터 코드 맵](../02_Architecture/02_Source/00_CPP/07_Scene-Input-DebugUI.md), [실행 흐름](../02_Architecture/00_Flows/00_Main-Flow.md), [Surface Simulation 코드 맵](../02_Architecture/02_Source/03_Surface-Simulation.md), [상태·GPU 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md)을 갱신했다.
- MSVC Debug 빌드, CPU 기준 검사, Textile GPU 검사와 SPIR-V 검증을 통과했다. [검증 기록](../05_Experiments/04_Surface-Simulation-Validation.md#에디터-국소-주입-검증)에 실제 화면 확인과 미실행 조작을 기록했다.

## 2026-11-24 — 기울어진 액체 저장 부피 계산

- C++·GLSL 작성 규칙과 Surface Simulation의 저장량·수송 계약을 확인했다.
- CPU와 계산 셰이더에서 표본별 부분 잠김을 적분하고 수위 복원과 공유 면의 질량 제한에 적용했다. 중심 높이가 같은 영역에는 닫힌 형태의 역함수를 사용했다. [상태·부피 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md)
- Debug 빌드, 계산 SPIR-V, CPU·GPU·캡처 CTest 3건이 통과했다. 36×20 벽돌의 최대 동일 바닥 경계 수심 차이가 0.553에서 0.227 mm로 줄었다. [검증 기록](../05_Experiments/04_Surface-Simulation-Validation.md#기울어진-표본에서-잠긴-부피-적분)

## 2026-11-24 — 평면과 요철 천의 대조 씬

- C++·주석·서식 규칙과 Surface Simulation의 에셋·표시 계약을 확인했다.
- 기본 눈 맞은 천의 바닥 표본을 평평하게 유지하고, 같은 프로파일을 쓰는 별도 에셋에 저작 가능한 요철을 추가했다. 씬 목록은 에셋 파일을 구별하며, 캡처와 검증에서 `--scene`으로 비교 에셋을 직접 연다.
- [표시 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md), [코드 맵](../02_Architecture/02_Source/03_Surface-Simulation.md), README와 [대조 실험](../05_Experiments/04_Surface-Simulation-Validation.md#평면과-요철-바닥-대조)을 갱신했다.
- MSVC Debug 빌드, 두 씬의 CPU–GPU 검사와 같은 카메라 캡처를 통과했다. `.Scene`의 아이디와 형상 설정 외 값이 일치하고, 높이차가 각각 0 및 3.571 mm임을 확인했다. 새 씬을 에디터 경로로 열고 12프레임을 실행했다.

## 2026-11-24 — 눈 맞은 천의 용융수와 젖은 자국

- C++·GLSL·서식 규칙과 Surface Simulation의 상태·표시 계약을 확인했다.
- `SnowOnCanvas.Scene`에 눈 중심을 받는 얕은 홈을 저작하고 양쪽 건조율을 맞췄다. `FabricAbsorption`의 흡수율을 낮췄다. `SnowyCoat`의 광학 경로 배율은 얇은 유막의 굴절색에만 적용한다.
- [표시 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md)과 [전후 비교](../05_Experiments/04_Surface-Simulation-Validation.md#용융수-가시성과-젖은-자국-비교)를 갱신했다.
- MSVC Debug 빌드, 표시 SPIR-V 3종 검증, 천 CPU–GPU 검사, 중심부 면밀도 비교를 통과했다. 그리드와 축을 끈 전후 4뷰 PNG와 면밀도·질량 2D 그림을 저장했다.

## 2026-11-24 — 에셋별 편집 폼

- C++·주석·서식 규칙과 Surface Simulation의 실행 설정 및 에셋 계약을 확인했다.
- 우측 `Simulation → Assets`에서 현재 씬의 파일을 종류별 드롭다운으로 고른다. 선택한 파일의 폼에서 물성, 표시 범위, 규칙 계수, 외관 값 또는 씬 필드값을 세션 초안에 편집한다. 적용과 에셋 기본값 복원은 기존 실행 경로를 사용한다.
- README와 [메인 흐름](../02_Architecture/00_Flows/00_Main-Flow.md), [에디터 코드 맵](../02_Architecture/02_Source/00_CPP/07_Scene-Input-DebugUI.md), [Surface Simulation 코드 맵](../02_Architecture/02_Source/03_Surface-Simulation.md)을 갱신했다.
- MSVC Debug 빌드와 `--check-surface --surface-scene textile`이 통과했다. `run.ps1`로 눈 맞은 천을 열어 `Assets` 탭과 `SnowyCoat.SurfaceProfile` 폼을 확인했다. 드롭다운 전환과 값 적용의 직접 조작 검증은 데스크톱 입력 충돌로 완료하지 못했다.

## 2026-11-24 — 상태 소스를 고르는 두께·면밀도 뷰

- C++·GLSL과 Surface Simulation의 상태·표시·GPU 계약을 확인했다.
- `Layer Thickness`와 `Areal Mass`는 활성 뷰의 `Source`를 사용한다. 측정 항목과 재질 표시 이름을 분리하고 상태 채널의 지원 범위로 소스를 제한했다. `SurfaceDiagnostics.glsl`은 액체 수면·고체 밀도·영역 면적을 각각 읽고, 범례와 같은 에셋 기준으로 색을 정규화한다.
- [상태·표시 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md), [코드 맵](../02_Architecture/02_Source/03_Surface-Simulation.md), README를 갱신했다.
- Debug 빌드와 정점·프래그먼트 SPIR-V 검증을 통과했다. 눈 맞은 천의 물·눈 두께와 흡수수 면밀도, 동결 트레이의 얼음, 벽돌의 기존 `depth` 키를 GPU로 캡처했다. 눈의 단위 환산과 `depth` 호환은 각각 동일한 색상 범위를 준 좌우 뷰의 RGB가 모든 픽셀에서 일치했다. 흡수수 두께와 탄화율 면밀도는 명령행에서 거부했다. 실제 에디터에서 두 측정 항목과 두께 범례를 확인했다. `Source`를 메뉴 바로 아래로 옮긴 최종 빌드는 데스크톱 입력 충돌로 드롭다운 선택을 재검증하지 못했다.

## 2026-11-24 — 4뷰 상태 비교와 오버레이 기본값

- C++·GLSL 서식과 Surface Simulation의 상태·표시 계약을 확인했다.
- 차트와 메쉬에서 월드 그리드와 축의 시작값을 껐다. `Layer & Substrate` 비교는 PBR, 바닥 등고선, 표면층 히트맵, 바닥 상태 히트맵을 같은 카메라로 배치한다. GLSL에서 위치별 상태색을 합성하고 개별 뷰와 저장 영역의 액체 면밀도 계산을 공유한다.
- [상태·표시 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md), [코드 맵](../02_Architecture/02_Source/03_Surface-Simulation.md), [에디터 코드 맵](../02_Architecture/02_Source/00_CPP/07_Scene-Input-DebugUI.md), README를 갱신했다.
- MSVC Debug 빌드와 그래픽 SPIR-V 검증을 통과했다. 눈 맞은 천과 벽돌을 `pbr,elevation,layer-states,substrate-states`로 캡처하여 두 그룹이 채워진 경우와 바닥 상태 그룹이 빈 경우를 확인했다. 에디터 비교 메뉴 조작은 검증용 창을 전면으로 전환하지 못해 완료하지 못했다.

## 2026-11-24 — 홈 벽면의 표시 수심

- GLSL·코드 스타일과 Surface Simulation의 렌더 계약을 확인했다.
- 수면 정점 복원에서 현재 면의 바닥 정점 높이를 사용해 이웃 벽의 높이가 낮은 홈의 수심을 높이지 않게 했다. [렌더 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md)
- Debug 셰이더 빌드, 정점 셰이더 SPIR-V 검증, 캡처 CTest를 통과했다. 벽돌 6초 전후의 Smooth 캡처를 비교했고, 원시 캡처의 PNG 해시 일치를 확인했다. [검증](../05_Experiments/04_Surface-Simulation-Validation.md#홈의-벽면-곁에서-밝아진-수심)

## 2026-11-24 — 저장 영역을 표시하는 스페이스 주입

- C++·주석·빌드 서식 규칙과 Surface Simulation의 화면·입력 계약을 확인했다.
- `SurfaceSimulation`은 뷰포트 커서 아래 저장 영역을 표시하고 스페이스의 최초 키 입력에만 한 회분을 큐에 넣는다. `SurfaceSimulationDisplay`는 마지막 표시 자세를 기준으로 상면의 선택과 영역 투영을 계산한다. 에디터에서 예약 급수의 실행 여부를 별도로 조절한다.
- [상태·GPU 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md#수동-주입과-범위-표시), [에디터 코드 맵](../02_Architecture/02_Source/00_CPP/07_Scene-Input-DebugUI.md), README를 갱신했다.
- MSVC Debug 빌드, `MDSS_SurfaceSimulationReference`, `--check-surface --surface-scene runoff`가 통과했다. GPU 테스트는 수동 주입량과 예약 급수량이 지정된 두 영역에 따로 한 번씩 반영되는지 확인한다. 벽돌 에디터는 실제로 열었으며 데스크톱 입력 충돌로 커서 표시와 키 반복 조작은 직접 검증하지 못했다.

## 2026-11-24 — 벽돌 외곽의 물 줄무늬

- Surface Simulation 상태·렌더 계약과 GLSL, C++ 및 빌드 서식 규칙을 확인했다.
- 벽돌의 급수 사각형을 5 mm 테두리에서 1 mm 홈 입구로 옮겼다. 벽돌 검증에 양의 수심을 가진 테두리 표본 수를 추가했고, 최대 급수 수심의 기준 높이는 씬의 급수 위치에서 읽는다.
- Debug 빌드와 벽돌 CPU·GPU 검사, RGB 캡처 CTest가 통과했다. Smooth를 끈 6초 캡처를 대조하고 8초 수심에서 젖은 테두리 표본 108 → 0개를 확인했다. [검증 기록](../05_Experiments/04_Surface-Simulation-Validation.md#벽돌-테두리의-줄무늬)

## 2026-11-24 — 벽돌의 고정 면적 자동 급수

- Agent-Vault의 C++·GLSL·주석·빌드 서식 규칙과 Surface Simulation 계약을 확인했다.
- `BrickRunoff.Scene`에서 자동 급수의 물리 면적을 지정한다. `SceneAsset`이 읽은 사각형을 액체 저장 영역별 배분율로 바꾸고 CPU 기준 단계와 GPU 준비 패스에서 공유한다.
- [상태·GPU 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md), [코드 맵](../02_Architecture/02_Source/03_Surface-Simulation.md), [검증 결과](../05_Experiments/04_Surface-Simulation-Validation.md#고정-면적-자동-급수)를 갱신했다. Obsidian의 구현 Rationale에 비교 그림과 입력·수위 복원 문서의 링크를 추가했다.
- MSVC Debug 빌드, CPU 기준·Vulkan GPU·캡처 CTest와 새 계산 SPIR-V 검증이 통과했다. 벽돌 기본 격자의 2초 시점을 네 뷰로 캡처했다.

## 2026-11-24 — Glacier Cut의 굳은 용암 외관

- C++·GLSL 규칙과 Surface Simulation의 재질·표시 계약을 확인했다.
- 용암의 높이 표본을 별도 불투명 레이어로 그려 얼음과의 경계가 보이게 했다. 굳은 표면의 음영은 높이 기울기와 암석 색 편차를 사용한다. [표시 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md) · [코드 맵](../02_Architecture/02_Source/03_Surface-Simulation.md)
- Debug GPU에서 18·112·370프레임을 확인하고 냉각 상태의 동일 카메라 전후를 기록했다. [캡처 비교](../05_Experiments/04_Surface-Simulation-Validation.md#굳은-용암-외관-비교)

## 2026-11-24 — 씬 제목·로그 탭과 Profile 설정

- C++·문서 작업 규칙과 에디터 흐름, Surface Simulation 재질·상태 계약을 확인했다.
- 뷰포트는 중앙에 씬 이름을 표시하고 Log 탭은 검색·필터로 시작한다. 설정 섹션은 양쪽 구분선 제목을 사용한다.
- Profile 탭에서 씬의 상태를 확인하고 계산에 쓰는 액체 물성과 국소 갱신 계수를 세션 동안 조정할 수 있다. 적용하거나 기본값을 복원하면 시뮬레이션이 다시 시작된다.
- [에디터 코드 맵](../02_Architecture/02_Source/00_CPP/07_Scene-Input-DebugUI.md), [Surface Simulation 코드 맵](../02_Architecture/02_Source/03_Surface-Simulation.md), [상태 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md), 에디터 실행 흐름과 README를 갱신했다.
- MSVC Debug 빌드, `MDSS_SurfaceSimulationReference`, `--check-surface --surface-scene kiln`과 `textile` 검증이 통과했다. 실제 에디터에서 씬 제목과 Log 탭을 확인했다. Kiln Spillway의 탄화율을 0으로 적용해 재시작한 뒤 기본값 0.53을 복원했다. Snow on Canvas의 용융·흡수·건조 계수와 상태 목록, 에셋 기본값 복원 시 재시작을 확인했다.

## 2026-11-24 — Surface Lab 상태 비교 뷰의 범례와 셀 격자

- 문서 워크플로, C++·GLSL 규칙, Surface Lab의 렌더 흐름과 상태·표시 계약을 확인했다.
- 에디터의 `By view` 팔레트와 면밀도 범례를 추가했다. `Cell state bars`가 그리는 네 채널을 같은 2×2 순서의 범례로 표시한다. `Cell Grid`는 활성 히트맵 위에 계산 셀 경계를 옅게 합성한다.
- [상태·표시 계약](../02_Architecture/01_Contracts/08_Surface-Lab.md)과 [코드 맵](../02_Architecture/02_Source/03_Surface-Lab.md)을 갱신했다. 상태 계산, 그래픽 버퍼 배치와 CLI 뷰 키에는 변경이 없다.
- 별도 MSVC Debug 빌드, Surface Lab CPU·GPU·캡처 검사 3건과 프래그먼트 SPIR-V 검증 2건이 통과했다. SnowOnCanvas 에디터에서 적설 초기와 융해 중의 색과 범례를 확인하고, 상단 `Cell Grid`로 활성 뷰의 격자를 켰다가 껐다.

## 2026-11-24 — Glacier Cut의 얼음 용융과 응결 구름

- C++·GLSL 규칙과 Surface Lab의 재질·표시 계약을 확인했다.
- 빙하 수로에 지정 접촉 열원, 유한 표층 얼음, 닫힌 하류 홈을 설정했다. 표시 정점은 남은 얼음 두께에 따라 낮아지고 용암은 시간과 지정 온도에 따라 전진·냉각된다.
- 셀별 수증기 스프라이트를 면적당 증발률 기반의 체적 적분 표시로 교체했다. 관련 [상태·표시 계약](../02_Architecture/01_Contracts/08_Surface-Lab.md)과 [코드 맵](../02_Architecture/02_Source/03_Surface-Lab.md)을 갱신했다.
- Debug 빌드, CPU·GPU 물질 수지, Glacier Cut·Kiln·Charring Wood 캡처를 확인했다. [결과와 캡처](../05_Experiments/04_Surface-Simulation-Validation.md#glacier-cut-용융과-응결-구름)

## 2026-11-24 — Surface Lab 계산 격자의 축당 상한 통합

- C++·GLSL 작성 규칙과 Surface Lab 상태·GPU 계약을 확인했다. CPU, 계산 셰이더와 표시 셰이더가 형상 및 계산 격자에 축당 64의 같은 상한을 사용한다. 그래픽 버퍼 오프셋과 Scene·UI의 기존 상한 경로를 맞췄다.
- 빌드 중 확인한 타임스탬프 인터페이스 변경에 맞춰 포켓 벤치마크는 Solver 통계, 오프라인 캡처 로그는 Draw 통계를 읽도록 연결했다.
- 별도 MSVC Debug 빌드, SurfaceLabReference·SurfaceLabGpu·SurfaceLabCapture, SPIR-V 6개가 통과했다. 64×64에서는 마지막 셀에서 수송·흡수·유출을 검사하고 CPU와 GPU의 최대 질량 차이 3.41061×10⁻¹³ kg을 확인했다. 벽돌의 64×64 4뷰를 RGB로 캡처했다.
- [Surface Lab 계약](../02_Architecture/01_Contracts/08_Surface-Lab.md), [코드 맵](../02_Architecture/02_Source/03_Surface-Lab.md), [검증 기록](../05_Experiments/04_Surface-Simulation-Validation.md)과 Obsidian의 GPU 패스 문서를 갱신했다.

## 2026-11-24 — 데모 격자와 수면 경계

- C++·GLSL 규칙, Surface Lab 상태·표시 계약과 기존 월류 실험을 확인했다.
- 벽돌, 천 및 공용 데모의 기본 계산 격자를 올렸다. 포켓의 2×1 월류 기준은 별도 실험으로 보존했다.
- PBR과 수심에 저장 영역 연결을 따르는 표시 수위 보간을 적용했다. 두 뷰 설정에서 보간 전후를 고를 수 있다.
- Debug 빌드, 기본 씬별 4뷰 캡처, 포켓 입력 4종의 CPU·GPU 물 수지 및 동일 프레임 비교를 확인했다. [검증과 캡처](../05_Experiments/04_Surface-Simulation-Validation.md#데모-격자와-수면-경계)

## 2026-11-24 — 공통 제목과 구분선 정렬

- Code Style·C++·Document Workflow와 에디터 배치 문서를 확인했다. 공통 제목 위젯과 씬 제목에 중앙 정렬된 ImGui 구분선을 적용해 선을 제목 양옆에 표시한다. 에디터 소스 맵에 이를 기록했다.

- C++ 포맷 후 Debug 빌드가 통과했다. 실제 창의 `Rules & Boundaries`와 씬 제목을 비롯한 공통 제목에서 양옆 선의 중앙 정렬을 확인했다.

## 2026-11-24 — 주황 테마와 프로파일러 판독성

- Code Style·C++·Document Workflow, Scene/Input/Debug UI 소스 맵, 성능 측정 문서와 Epic의 에디터 인터페이스·뷰포트 도구줄 개요를 확인했다. 씬 이름을 도구줄에서 제목 줄로 옮기고 주요 제목의 글자 크기·중앙 정렬·여백과 주황 포인트를 공유했다. 우측 조작을 Playback, Solver, Input, Geometry, Profile로 나누고 Solver 아래에는 Discretization과 Rules & Boundaries를 둔다.
- 프로파일러 표시값을 1초 간격으로 게시하고 Hold readings를 추가했다. 같은 제목 규칙과 폭에 맞춰 늘어나는 열을 GPU·CPU 시간 표에 사용한다. HEAD의 메쉬 타이밍 39개 항목을 모두 유지하고 Other overlay draw를 추가했다. 메쉬의 주요 시간 그룹을 기본으로 펼친다.
- 차트에 굴절 prepass·색상 패스 시간, CPU 1초 평균·최댓값, FPS·프레임 간격과 격자·표본·영역·면·스텝 작업량을 표시한다. Scene/Input/Debug UI 소스 맵과 성능 측정 문서를 동기화했다.

- Debug 빌드, runoff CPU/GPU와 basin GPU 계측 검증, 오프라인 PNG 회귀를 통과했다. 실창에서 주황 테마, 가운데 씬 제목, 좁게 도킹한 시간 표의 열과 약 2.7초간 진행 중에도 고정된 계측값을 확인했다.

## 2026-11-24 — 뷰포트 상단 바, 프로파일링과 에디터 테마

- Code Style·C++·Document Workflow, Scene/Input/Debug UI의 소스 맵과 성능 기록을 검토했다. 뷰포트 상단에 씬 이름·뷰 모드·비교·프로파일러를 배치했다. 파일 열기는 File 메뉴에서 한다. 하단은 Experiments와 Log의 개별 도킹 탭으로 바꿨다.
- 기존 메쉬의 타이밍·메모리·렌더 상태 표와 성능 옵션을 프로파일러 창으로 옮겼다. 차트에 GPU 계산·복사·그리기 통계와 CPU 프레임 시간을 표시하고, 차트의 상태·형상·렌더 버퍼를 GPU 메모리 집계에 분류했다. Scene/Input/Debug UI 소스 맵과 성능 측정 문서를 갱신했다.
- 차콜·청록 테마와 Simulation 안쪽 배경을 맞췄다. 캡처·실험 출력 경로는 프로젝트의 `SurfaceLabCaptures/`를 사용한다. Surface Lab 소스 맵에 경로를 반영했다.
- 포맷 후 Debug 빌드, runoff CPU/GPU와 basin GPU 계측 검증, 오프라인 PNG 회귀를 통과했다. 실제 창에서 상단 바·하단 탭·테마와 차트의 단계별 시간·GPU 메모리 탭을 확인했다. 에디터 Capture 메뉴의 저장 동작은 실창에서 끝까지 확인하지 못했다.

## 2026-11-24 — 공통 에디터 배치와 표면 입력 복원

- Code Style·C++·Document Workflow, Scene/Input/Debug UI 소스 맵, Contact 흐름과 Surface Lab 계약을 확인했다.
- 좌측 상단에 씬·카메라·애니메이션, 좌측 하단에 렌더 옵션, 우측에 타임라인과 시뮬레이션, 중앙 하단에 실험·계측·로그를 배치했다. Chart 씬의 카메라 입력과 월드 기준선에 메쉬 에디터의 조작과 표시를 연결했다. 독립 인터랙티브 창을 제거하고 캡처·검증 CLI를 유지했다.
- 차트는 바닥을 클릭한 영역에 물을 주입한다. 메쉬 씬의 파일 열기와 십자선·Space 입력 경로를 연결했다. Scene/Input/Debug UI 소스 맵, Surface Lab 소스 맵과 GPU 계약을 갱신했다.
- 변경된 C++ 포맷 후 Debug `MDSS` 빌드, CPU/GPU runoff 검증과 오프라인 캡처 회귀를 통과했다. GPU 회귀는 예약 급수와 수동 주입이 서로 다른 영역에 각각 한 번 들어가는지 검사한다. CTest는 10개 중 9개 통과했고 `MDSS_SurfaceMapping`의 준비된 UV seam fixture 경고 검사 1개가 실패했다.
- 실행 화면에서 패널 배치와 월드 그리드·축 표시를 확인했다. 카메라 이동과 뷰포트 클릭 주입의 실창 검증은 사용자 데스크톱 조작과 겹쳐 완료하지 못했다.

## 2026-11-24 — 적설 높이, 젖은 천, 물막 지속 시간

- C++·GLSL 규칙과 Surface Lab의 재질·렌더링 계약을 확인했다.
- 눈을 별도 불투명 윗면으로 그렸다. 정점 변위와 음영은 고체 질량의 연속 높이를 읽고, 눈이 녹으면 같은 높이장이 낮아진다. 천의 흡수수 면밀도는 보간한 뒤 표시 전용 응답에 연결했다.
- 박막이 움직이는 시간을 확보하도록 천의 흡수 계수를 1.80에서 0.60 s⁻¹로 조정했다. 수송 후 용융과 흡수를 적용하는 순서를 유지했다.
- Debug 빌드, Surface Lab GPU 검사 및 같은 시점의 상태별 캡처를 확인했다. [수치와 캡처](../05_Experiments/04_Surface-Simulation-Validation.md#적설-높이와-용융수-지속-시간)

## 2026-11-24 — 천 위 적설 외관

- C++·GLSL 포맷 및 주석 규칙, Surface Lab 에셋과 표시 계약을 확인했다.
- `SnowyCoat.SurfaceProfile`의 `appearance.snow`가 고체 물 상태와 표시용 벌크 밀도를 연결한다. 셀 중심 사이의 보간, 로컬 좌표의 알갱이 패턴과 픽셀 폭 기반 경계 완화를 적용했다.
- Debug 엔진을 빌드하고 적설이 줄어 젖은 천으로 드러나는 세 시점을 캡처했다. 기존 검증의 천 JSON과 새 천 JSON을 비교했다. [적설 캡처와 검증](../05_Experiments/04_Surface-Simulation-Validation.md#천-위-적설)

## 2026-11-24 — 물리 에셋 확장자와 씬 프로파일 이관

- AGENTS·Document Map·C++/주석/포맷 규칙, 에셋 계약과 Surface Lab 흐름을 확인했다.
- 벽돌·천에 `SurfaceLabScene` 로더를 연결했다. 재질·상태·국소 갱신·프로파일의 버전과 참조를 확인한 뒤 현재 박막 솔버의 슬롯에 연결한다. 얼음 밀도를 CPU·GPU·표시에 전달한다.
- 기존 메시 씬 17개와 40개 오브젝트의 배치를 직접 참조로 이관했다. 기존 버전 씬을 읽을 수 있고 v2는 기본값 및 표면별 오버라이드를 저장한다. 배포 에셋에서 구형 맵·프로파일·액체 JSON과 참조가 없는 회전 클립을 내렸다. 기존 수치 실험 프로파일은 연구·테스트 픽스처에 남겼다.
- 전체 씬 19개를 열어 프레임을 실행했다. 배포 파일 정리 후 Surface Lab GPU 13케이스, 캡처 회귀, 벽돌·산 벤치마크 스모크와 커플링 plate·mud 스모크를 실행했다. 상세 결과는 [에셋 이관 검증](../05_Experiments/04_Surface-Simulation-Validation.md#에셋-이관-검증)에 기록했다.

## 2026-11-24 — 공통 계산 격자의 분리된 액체 홈과 형상 세트

- C++·GLSL·Python, Code Style, Surface Lab 상태·GPU 계약, 검증과 성능 기록 절차를 확인했다.
- 형상 전처리에서 계산 셀 내부의 홈과 접촉을 찾고, CPU 및 Vulkan 수송이 각 홈의 질량과 수위를 계산한다. 표시 경로는 형상 표본에 속한 홈의 수위를 읽는다. 부모 셀 단위 반응을 유지하고, 벽돌 전용 격자 절단을 제거했다.
- `Assets/Geometry` 바로 아래에 원본 OBJ, 노멀, 높이, 변위 맵을 세 형상별로 배치했다. Scene 탭과 CLI에서 교체한다. 빌드를 막던 장면 v2 버전 전달, JSON 타입 비교, 캐시 해시 문자열 인자를 바로잡았다.
- [상태 계약](../02_Architecture/01_Contracts/08_Surface-Lab.md), [코드 맵](../02_Architecture/02_Source/03_Surface-Lab.md), [검증과 캡처](../05_Experiments/04_Surface-Simulation-Validation.md), Obsidian의 Implementation Rationale과 실험 기록을 갱신했다.
- MSVC Debug 빌드와 SurfaceLabReference·SurfaceLabGpu·SurfaceLabCapture 세 테스트, SPIR-V 6개, 형상 3세트 × 입력 4종을 검증했다. 3 mm 고개와 6 mm 턱의 수평·기울임 전달을 확인하고, RX 6700 XT에서 두 형상의 GPU 타임스탬프를 수집했다.

## 2026-11-24 — VS Code Python 진단 정리

- AGENTS·Document Map, Code Style, Python 규칙, Document Workflow, Source Map 및 Surface Lab 검증 절차를 확인했다.
- B1과 연결 표면 연구의 고정 의존성을 프로젝트의 `Build/` 아래에 설치했다. Pyright에서 연결 표면 연구의 기존 `Prior` 모듈과 별도 NumPy·SciPy 버전을 해석하도록 설정했다.
- B1·연결 표면 연구 및 Surface Lab 도구에서 고정 길이 튜플, 반복 뒤 사용하는 값, 재생 입력 버퍼와 그래프 저장 경로의 타입을 수정했다. 의존성 설치 명령을 [Validation & Tests](../05_Experiments/01_Validation-and-Tests.md)에 기록했다. 기존 진입점과 입력·출력 형식은 유지되어 Source Map·Flow·Contract 변경은 필요하지 않다.
- Pyright는 Python 파일 65개에서 오류·경고·정보 0건을 보고했고 VS Code Problems도 0건을 확인했다. 임시 경로에서 B1 계산과 PNG, Surface Lab 형상 및 PNG·SVG, 연결 표면 `N24_T7_83` 1회 반복의 26개 실행과 기존 결과 복사본의 집계·그래프 생성을 검증했다. C++ 빌드와 연결 표면 연구 전체 재실행은 수행하지 않았다.

## 2026-11-24 — Surface Lab 액체 재질과 수송 일반화

- AGENTS·Document Map, C++·GLSL 작성 및 포맷 규칙, 기존 Asset·State 계약, Surface Lab 수송·GPU 계약과 코드 맵을 확인했다.
- 수위·턱·유량을 `LiquidRunoff` CPU 및 Compute 경로에서 계산한다. 기본 `Water.json` 또는 CLI에서 지정한 재질 JSON의 밀도·동적 점도·국소 규칙을 읽고 GPU에 전달한다. 용융·동결과 기존 바닥 반응은 `WaterMaterial`의 선택 규칙으로 옮겼다.
- 범용 표시 경로의 파일명과 셰이더 경로를 `SurfaceLab`으로 정리하고 Material, Surface Lab, Shader 소스 경로와 GPU SSBO 오프셋을 Vault에 동기화했다. 기존 CLI 뷰 키와 장면 입력을 보존했다.
- 영향 파일 42개에 포맷 파이프라인을 적용하고 2회 출력 일치를 확인했다. 별도 Debug 빌드와 SurfaceLabReference·SurfaceLabGpu·SurfaceLabCapture 3개를 통과했다. CPU와 GPU의 파일 기반 점성 액체 수송량·수위·질량 보존을 검사했고, CLI RGB 캡처와 잘못된 물성 필드 거부, SPIR-V 6개와 추가 SSBO 오프셋을 검증했다.
- 이전 `Water` 경로의 파일과 참조는 모두 옮겼다. 자동 승인 검토가 빈 디렉터리 삭제 명령을 차단해 로컬에는 빈 경로 4개가 남아 있다.

## 2026-11-24 — Surface Lab 표면 외관 설정 정리

- AGENTS, Document Map, Code Style·C++·Doxygen, Surface Lab 상태·GPU 계약과 기존 엔진의 State 등록·렌더 흐름을 확인했다.
- 바탕색·텍스처 경로·미터당 반복 횟수를 `Rendering/Surface/SurfaceAppearance.h`로 옮겼다. 표시 두께, 얼음 굴절률·바닥 옵션, 가열 표시를 `SurfaceLabAppearance.h`에 두고 장면 구성, 표시 메시, 렌더러, 실험 리포트의 참조를 갱신했다.
- Document Map과 Rendering·Surface Lab 소스 맵을 동기화했다. 이번 변경은 CPU 외관 입력을 구성하는 위치와 타입에 한정해 셰이더 인터페이스와 실행 순서의 계약을 유지한다.
- 영향 C++ 파일 9개에서 `clang-format`과 Region 포맷이 반복해도 동일한 출력을 내는 것을 확인했다. 별도 MSVC x64 Debug 빌드 및 `MDSS_SurfaceLabReference`, `MDSS_SurfaceLabCapture`가 통과했다. 이전 세션의 PNG는 재빌드된 셰이더 구성이 달라 해시가 달랐으며, 이번 검증은 두 텍스처 모드의 RGB 캡처 회귀 조건을 확인했다.

## 2026-11-24 — 나무 예열과 공통 얼음 광학

- C++·GLSL 주석·접기 규칙과 Surface Lab의 ABI·표시 계약을 읽었다. Charring Wood는 주변 온도에서 시작하고 지정 열원을 시간에 따라 올리도록 CPU·GPU 입력을 맞췄다. 빙하의 얼음 광학을 얇은 얼음층과 동결 수면에서 공유한다.
- [Surface Lab 계약](../02_Architecture/01_Contracts/08_Surface-Lab.md), [소스 맵](../02_Architecture/02_Source/03_Surface-Lab.md), [검증과 캡처](../05_Experiments/04_Surface-Simulation-Validation.md), Obsidian의 Rendering과 얼음 Rationale을 갱신했다.
- Debug 빌드, SPIR-V 6개, 장면 5개의 CPU·GPU 비교, RGB 캡처를 확인했다. 전체 CTest는 기존 SurfaceMapping 이음매 실패를 포함해 9/10이다.

## 2026-11-24 — Surface Lab 주석과 코드 접기 보완

- AGENTS와 Document Map, Code Style, Local Numbering, C++·GLSL, Source Comments, Doxygen, 포맷·검증 절차를 읽었다. Surface Lab의 상태·GPU 계약 및 Surface Mapping의 흐름·계약·소스 맵을 대조했다.
- Surface Lab C++·GLSL과 열린 매핑 소스 37개에 책임별 섹션, 접기 마커, 한국어 API 설명을 보완했다. C++ 익명 네임스페이스 전체를 접을 수 있게 하고 GLSL 진입 파일에도 주석 마커를 배치했다. 현재 체크아웃의 수송 코드는 `WaterRunoff` 경로에서 확인했다.
- 루트와 볼트의 VS Code 설정에 C++·GLSL 마커 인식을 연결하고 [Shader Source Index](../02_Architecture/02_Source/01_Shaders/00_Index.md)를 갱신했다. 작업 중 들어온 에디터·온도 입력 변경을 보존했다. 이 작업의 변경은 설명·탐색 범위이므로 계약과 실행 흐름 문서의 추가 변경은 필요하지 않았다.
- `clang-format` 후 Region 포맷의 반복 안정성을 확인했다. 설치된 Maptz 공급자에서 LF와 CRLF의 Region 인식을 검증했고, 셰이더 진입점 6개의 Debug·Release SPIR-V가 포맷 전후 바이트 단위로 일치했다. SimulationTransport 및 NormalMapTransfer 통과. SurfaceMapping의 UV seam 검사 실패는 수정 전 바이너리에서도 재현했다.
- 기존 `MDSS.exe`가 사용 중이어서 `Build/Validation/style-regions-20261124`에 별도 MSVC x64 Debug 빌드를 완료했다. SurfaceLabReference, SurfaceLabGpu, SurfaceLabCapture를 모두 통과했다. IDE 확인은 확장 공급자의 접기 범위 검사로 수행했다.

## 2026-11-24 — Surface Lab 동결·용융과 데모 카메라

- **읽은 문서**는 Agent Vault 진입 규칙, C++·GLSL 포맷 규칙, Surface Lab의 물량·GPU 레이아웃·표시 계약이다.
- **구현**은 동결과 온도 입력, 밀도 환산 표시 높이, 트레이 눈금, 프레임별 카메라 연출이다.
- **문서와 검증**은 상태 계약, 코드 맵, [동결·용융 실험](../05_Experiments/04_Surface-Simulation-Validation.md), Obsidian Implementation과 Rationale에 기록했다. Debug, SPIR-V 6개, CPU/GPU의 8초 상변화, 31프레임과 4뷰를 확인했다. 전체 CTest는 기존 SurfaceMapping 실패를 포함하여 9/10이다.

## 2026-11-24 — Surface Lab 격자·상태 진단 뷰

- **읽은 문서**는 Agent Vault 진입 규칙, C++·GLSL 포맷 규칙과 Surface Lab 상태·렌더 계약이다.
- **구현**은 계산 셀과 형상 샘플 진단, 표면층의 질량 히트맵, 네 상태 타일, 비교 프리셋과 CLI 뷰 목록이다. 수치 계산과 표시 스냅샷을 재사용했다.
- **문서와 검증**은 상태 계약, 코드 맵, [캡처 기록](../05_Experiments/04_Surface-Simulation-Validation.md), Obsidian Rationale에 기록했다. Debug, SPIR-V 5개, CTest 3개, 상태 타일의 색 구분, 비균일 격자, 다중 뷰와 RGB 회귀 캡처를 확인했다.

## 2026-11-24 — Glacier cut 맑은 얼음

- **읽은 문서**는 Agent Vault 진입 규칙, C++·GLSL 포맷 규칙, Surface Lab 상태·렌더 계약, 얼음 PBR의 Rationale이다.
- **구현**은 높이장의 입사·출사 굴절, 한 번의 내부 반사, 투과색 확인용 받침과 약한 감쇠·산란이다.
- **문서와 검증**은 상태 계약, [전후 캡처와 검증](../05_Experiments/04_Surface-Simulation-Validation.md), Obsidian 렌더링과 Rationale에 기록했다. Debug, 셰이더 5개, CTest 3개, 기존 RGB 캡처, 빙하 PBR·4뷰, Fresnel과 CPU/GPU 결과의 일치를 확인했다.

## 2026-11-24 — Surface Lab 수면 경계 표시

- **읽은 문서**는 Agent Vault 진입 규칙, C++·GLSL 포맷 규칙, Surface Lab의 표시 스냅샷과 형상 계약이다.
- **구현**은 뷰별 수면 경계 보간, 얕은 물 색의 합성, 포켓의 기본값과 캡처용 CLI다. 계산 수위와 표시 수위는 별도로 읽는다.
- **문서와 검증**은 상태 계약의 그래픽 ABI와 [검증 결과](../05_Experiments/04_Surface-Simulation-Validation.md), Obsidian Rationale에 기록했다. Debug, SPIR-V 다섯 개, CTest 세 개, 캡처 회귀, 포켓의 건조·넘기 전·넘긴 후 화면과 원본 OBJ CPU/GPU 결과를 확인했다.

## 2026-11-24 — Surface Lab 메조형상 포켓과 입력 비교

- **읽은 문서**는 Agent Vault 진입 규칙, Document Map, C++·Python 포맷 규칙, Surface Lab 계약과 형상 소스 근거다.
- **구현**은 두 셀에서 채워지는 포켓 장면, 원본 OBJ 및 세 맵의 전환, 저장량과 넘침 시점의 CPU/GPU 실험, Python 비교 그림이다. 기존 형상 입력과 물 수송을 재사용했다.
- **문서와 검증**은 [상태 계약](../02_Architecture/01_Contracts/08_Surface-Lab.md)과 [검증 결과](../05_Experiments/04_Surface-Simulation-Validation.md), Obsidian 실험 및 Rationale에 기록했다. Debug, 네 입력의 CPU/GPU 비교와 캡처를 검증했다. 전체 CTest에서 기존 SurfaceMapping 이음매 테스트가 실패했다.

## 2026-11-24 — Surface Lab 높이와 얼음 재질

- **읽은 문서**는 Agent Vault 진입 규칙, Document Map, C++·GLSL 포맷 규칙, Surface Lab 계약과 기존 렌더링 선택 근거다.
- **구현**은 세 단차의 계단형 수로, 고체 얼음 바닥의 빙하 수로, 비교 뷰의 PBR·등고선과 카메라 각도 설정이다.
- **문서와 검증**은 Surface Lab 계약, 코드 배치, [장면 수치와 캡처](../05_Experiments/04_Surface-Simulation-Validation.md), Obsidian Rationale에 기록했다. Debug 빌드, SPIR-V 5개, CTest 3개, 두 장면의 8초 CPU/GPU 비교, PBR·등고선·Fresnel RGB 캡처 및 기존 캡처 회귀 검사를 통과했다.

## 2026-11-24 — Surface Lab 불꽃 표현

- **읽은 문서:** Agent Vault 진입 규칙, Document Map, C++/GLSL·포맷 규칙, Surface Lab 계약과 효과 결정.
- **구현:** 불꽃 카드를 제거했다. 표시 스냅샷에서 활성도를 보간해 난류가 상승하는 빛을 적분한다. 주변 바닥에는 깜박이는 불빛과 보간한 그을음을 표현한다.
- **문서·검증:** 상태 계약, 효과 Rationale, Agent Vault와 Obsidian의 검증을 갱신했다. Debug, SPIR-V 5개, 기준·수송 CTest 3개, 복합 수로 CPU/GPU, 최대 4뷰와 RGB 캡처를 확인했다.

## 2026-11-24 — Surface Lab 텍스처와 캡처 수정

- **읽은 문서:** Agent Vault 진입 규칙, Document Map, 00 Engineering Standards의 C++·GLSL·포맷 규칙, Surface Lab GPU 계약과 검증 문서.
- **구현:** CC0 albedo/roughness를 장면별로 연결하고 표시 옵션에서 즉시 끄고 켤 수 있게 했다. 수면과 효과의 합성 색을 RGB PNG로 저장하고 캡처 회귀 검사를 추가했다.
- **문서:** Surface Lab 계약·소스 맵·실험 결과와 Obsidian 렌더링 문서를 갱신했다. 텍스처 저장 위치에 원본과 가공 내역을 기록했다.
- **검증:** Debug 빌드, SPIR-V 세 개, 기준 CTest 세 개, Vulkan 캡처 회귀 검사, 다섯 장면과 UI 캡처 재생성을 완료했다. 게시 이미지 19장의 RGB 포맷과 투명도 청크 부재를 확인했다.

## 2026-11-24 — Surface Lab 시연 경로와 실험 UI

- **읽은 문서:** Agent Vault 진입 규칙, Document Map, 00 Engineering Standards의 C++/GLSL/Python·포맷·문서·검증 규칙, Geometry/Solver/GPU 레이아웃 계약과 렌더 흐름, 검증 이력.
- **구현:** 범용 `SurfaceState/Water`, `SurfaceState/Geometry`, `Rendering/Water`, `Application/SurfaceLab`으로 책임을 나누었다. 네 방향 수송과 국소 변화, 수위 복원과 이중 표시 슬롯, 4뷰와 물 표현·팔레트, 장면별 수치 실험, PNG/SVG/JSON을 연결했다.
- **문서:** [구현 지도](../02_Architecture/02_Source/03_Surface-Lab.md), [상태 계약](../02_Architecture/01_Contracts/08_Surface-Lab.md), [수증기와 불의 선택](../03_Decisions/06_Surface-Emissions.md), [실험·캡처](../05_Experiments/04_Surface-Simulation-Validation.md), Obsidian Implementation과 Rationale을 갱신했다.
- **검증:** Debug 빌드, SPIR-V 검증 4개, 5장면 및 4변형 CPU/GPU 비교, 4종 형상 비교, UI·캡처·타임스탬프를 확인했다. 전체 CTest는 9/10이었다. SurfaceMapping seam fixture의 기존 실패를 별도로 기록했다.

## 2026-11-16 — Clear Python and IntelliSense Problems

- **Read:** Agent Vault instructions and Document Map; Python conventions, Source Map, Validation & Tests, Verification & Review, and existing VS Code/CMake configuration.
- **Changed:** Added root `pyrightconfig.json` paths for the repository's generated Python runtime packages while excluding `Build/**` generated/dependency sources. Corrected type narrowing and result annotations across the affected performance/research scripts, including optional image/replay results, JSON manifests and plans, SciPy outputs, Matplotlib axes/paths, and plotting handles. Added `.vscode/c_cpp_properties.json` using `${env:VULKAN_SDK}/Include` and the repository's C++20/MSVC settings so the installed Vulkan SDK header is found by IntelliSense.
- **Docs:** No contracts, flows, or runtime behavior changed; this is editor analysis configuration and static typing cleanup.
- **Validation:** `npx --yes pyright --project pyrightconfig.json Scripts` reported 0 errors, 0 warnings. Python AST parsing and `pyrightconfig.json` parsing passed. VS Code Problems showed “No problems have been detected in the workspace” after the IntelliSense configuration refreshed. Research scripts and C++ builds/tests were not run.

## 2026-11-16 — Agent Vault Explorer settings

- **읽은 문서:** Agent Entry·Document Map·Markdown·Document Workflow·Commit Style 및 기존 편집기 설정.
- **설정:** 볼트 `.vscode/settings.json`에서 관리 파일과 `.obsidian`을 Explorer 숨김 대상으로 추가했다.
- **문서 영향:** 편집기 표시 설정으로 엔진 동작·계약·흐름 영향 없음.
- **검증:** JSON 파싱 통과. 커밋 분리 작업에서 엔진 빌드·테스트는 재실행하지 않았다.

## 2026-11-16 — Scene 패널 선택·동작 영역 재배치

- **읽은 문서:** AGENTS·Document Map·Code Style·C++·Document Workflow·Pre-Build Formatting·Scene/Input/DebugUI Source·Validation 안내, 기존 UI 레이아웃·style·Scene 교체 경로.
- **코드:** `DrawSceneWindow`를 현재 Scene/동일 폭 파일 작업 버튼 → 고정 라벨 열의 Category/Scene 두 dropdown → Restart 주 버튼 순서로 재배치. `DrawSceneBrowser`로 목록·선택·교체를 분리하고 Category 탐색 상태를 UI가 소유한다. 카테고리만 바꿀 때는 로드하지 않으며 Scene 선택 시 popup/table 종료 후 교체한다. 숨겨진 창은 즉시 반환하고 긴 상태 메시지는 줄바꿈한다.
- **문서 동기화:** Scene/Input/DebugUI Source 갱신. 파일 형식·GPU ABI·Solver/재시작 의미 변경이 없어 Contracts/Flows 추가 수정은 불필요.
- **검증:** 변경 C++ 범위에 clang-format → Region 후처리 적용, Region 및 diff 공백 검사 통과. MSVC Debug `cmake --build Build --config Debug --target MDSS -- /m /p:OutDir=C:\Users\yonai\Documents\MDSS_Engine\Build\UIReview\Debug\ /verbosity:minimal` 성공. 기존 실행 파일과 충돌하지 않게 별도 출력 경로를 사용했다. 실제 엔진 1280×720 화면에서 Scene 패널의 제목·동일 폭 파일 버튼·Category/Scene 라벨과 선택값·Restart 버튼이 잘리지 않고 정렬됨을 확인했다. 이어진 클릭 시 확인용 창이 도구에서 사라져 dropdown 선택/Scene 교체 조작은 미검증. 기존 getenv/미사용 변수/정수 변환 경고가 있으며 전체 테스트·성능/다른 화면 크기 검증은 하지 않았다. 원자료: `Build/Validation/20261116-scene-ui/build.log`, `runtime.log`, `runtime-error.log`.

## 2026-11-16 — Scene 패널 정리 및 Restart 재생 복구
- **읽은 문서:** Agent-Vault AGENTS·Document Map·Code Style·Document Workflow·Verification, C++ 규칙, Main Flow·Contact Flow·Scene/Input/DebugUI Source, Validation 안내. 현재 `RestartScene`/`FScene::RestoreInitialState`/frame animation 경로와 autoplay Scene asset을 대조했다.
- **코드:** Scene 선택 목록을 Starter·Midterm·Showcase·Performance 카테고리별 dropdown으로 정리하고 현재 Scene 이름/Restart 버튼의 시각적 우선순위를 높였다. `Restart Scene`은 기존 camera 유지와 GPU simulation reset을 보존하면서 animation Scene을 재생 상태로 복귀시킨다. 이 경우 simulation pause를 해제하고 animation/simulation time scale을 1.0으로 초기화한다.
- **문서:** Scene/Input/DebugUI Source에 패널 분류 및 restart/play 동작을 반영했다. GPU/직렬화 계약과 일반 frame step 순서는 바뀌지 않아 Contract·Flow 본문은 추가 변경하지 않았다.
- **검증:** `python Tools/indent_regions.py --check Source/DebugUI/Editor/DebugUIEditor.cpp` 및 diff/제어 흐름 정적 검토. 빌드·테스트·실행 UI 확인은 하지 않았다.

## 2026-11-16 — Python 분석용 빌드 산출물 제외
- **읽은 문서:** Agent-Vault AGENTS·Document Map·Python·Source Map·Validation & Tests·Document Workflow, Project Brief·Constraints, 기존 VS Code 설정.
- **설정/코드:** 루트 `pyrightconfig.json`에서 생성물·외부 의존성·레거시 보조 스크립트가 포함된 `Build/**` 전체를 `exclude` 및 `ignore`한다. Problems에서 확인한 `Build/_deps/imgui-src`와 `Build/_deps/tinyobjloader-src`의 Python 2용 스크립트 진단도 이 경로에 포함된다. 이어 `Scripts/`의 `sys.path.insert`와 일치하도록 `Build/PerformanceLab/PythonPackages` 및 `Build/ResearchPython`을 `extraPaths`로 추가해 실행 의존성을 찾게 한다. 프로젝트 스크립트는 캡처 부재/None 검사, Matplotlib 저장 경로 타입과 플롯 데이터, SciPy 인덱스 반환, 결과 레코드 타입을 명시적으로 보강했다. showcase import 도구는 문서화된 `uv --with trimesh` 의존성을 import 단위로 표시했다. 프로젝트 `Scripts/`와 `Tools/`는 제외하지 않는다.
- **문서 동기화:** 편집기 진단 범위 조정으로 엔진 코드·API/ABI·요구사항·실행 흐름 영향 없음.
- **검증:** JSON 파싱, Python AST 파싱, 제외·추가 경로가 현재 Build/실행 경로와 일치하는지 확인. UI 캡처의 Problems는 `Build/**` 확대 적용 및 코드 수정 전 상태이므로 적용 후 Pylance 결과는 미확인; 실험 스크립트 실행은 하지 않음.

## 2026-11-16 — Universal-Agent-Vault v6.4 동기화 및 체크아웃 대조

- **읽은 문서:** Universal-Agent-Vault v6.4 CHANGELOG·AGENTS·Document Map·C++·Source Comments·Setup 기준, 현재 볼트의 Markdown/Workflow, Project Brief·Constraints·Contracts·Flows·Source·Current Work·Validation·README.
- **도구 코드:** `Tools/indent_regions.py`와 `Assets/Formatting/indent_regions.py`를 Universal v6.3 타입/정규식 진단 수정본으로 동기화. 엔진 코드와 GPU 인터페이스는 변경하지 않음.
- **포맷 진입점:** `Tools/format_before_build.py`를 추가했다. 실제 CMake 타깃 연결은 Current Work에 적힌 미완료 상태를 유지한다.
- **문서 동기화:** C++ 익명 namespace Region 규칙, v6.4 버전 기록, CMake·프레임 경로, 과거 검증 범위 및 계약 등록부 갱신. 결정 요약을 Universal 구조인 `03_Decisions/`로 이동하고 ADR 색인과 내부 링크를 갱신. 루트 AGENTS의 CMake 부재 주장을 제거. CMake 타깃에 자동 포맷 단계가 없는 현행 상태를 Current Work에 미완료 사항으로 기록.
- **검증:** 파일·함수 경로와 CMake/README/빌드 스크립트를 정적으로 대조. 빌드·테스트·포맷터 실행은 하지 않음; 이번 작업은 문서 동기화 및 포맷 도구 복사이며, 실행 결과를 주장하지 않는다.
- **커밋 준비 검증:** Python 도구 3개 AST, 포맷 도구 두 사본의 바이트 일치, 새 포맷 진입점 `--help`, 변경 C++ 두 파일의 Region 검사 및 diff 공백 검사 통과. 이번 커밋·푸시 작업에서는 엔진 빌드·테스트를 재실행하지 않았다.

## 2026-11-11 — VS Code Explorer 잡음 파일 숨김
- **읽은 문서:** Agent-Vault AGENTS·Document Map·Code Style·Document Workflow·Local Numbering, 기존 `.vscode/settings.json` 및 `.gitignore`.
- **설정:** `.vscode/settings.json`의 `files.exclude`에 Python 캐시·컴파일 산출물·Build 전체·Cache·ThirdParty·로컬 Universal-Agent-Vault를 추가. 프로젝트 소스·문서는 계속 표시한다.
- **문서 동기화:** 편집기 전용 보기 설정으로 요구사항·계약·흐름·Source Map 영향 없음. Work Log에 반영.
- **검증:** JSON 파싱 및 설정 패턴/실제 대상 경로를 확인. 빌드·테스트 미실행(Explorer 표시 설정 변경).

## 작성 원칙

- 소규모 작업은 한 줄이면 충분하다. 여러 파일의 동일 작업은 하나로 묶을 수 있다.
- 문서 갱신이 불필요해도 **왜 영향이 없는지** 간결히 기록한다.
- `미실행`, `미검증`, `실패`, `통과`를 구분한다. 실행하지 않은 명령을 성공 처리하지 않는다.
- 길어지면 월별 로그로 분리하고 여기에는 짧은 링크 인덱스를 남긴다.


## 2026-11-09 — 범용 볼트 이관 및 소스 구획 정리
- **읽은 문서:** 기존 프로젝트 Policy·Glossary·Architecture(Flows/Contracts/Source)·Development·Experiments 및 Universal Agent Vault 규칙·언어·GLSL·주석 문서.
- **코드 변경:** 기존 `#pragma region`에 번호·섹션 구분선 추가, 주요 엔트리 포인트의 구획 정리, GLSL 코드의 접기용 섹션 주석 추가. **알고리즘·상수·API·호출 순서·GPU 동기화는 의도적으로 변경하지 않음.**
- **문서 동기화:** 새 볼트의 Context/Architecture/Decisions/Development/Experiments/Research로 기존 문서 보존·재배치. 새 Document Map·Project Brief·Requirements·Constraints·Home 작성.
- **검증:** ZIP 무결성, 이전/이후 코드 토큰 대조(공백·주석·`#pragma region`을 제외), 파일 목록·링크 탐색, 셸/Python 문법 검사를 수행할 실행함 (C/C++/GLSL 원본 비주석 코드 줄 동일, 44개 Python AST 검사, 4개 Shell 문법 검사, Region 균형 및 문서 링크). 실제 C++ 빌드/셰이더 컴파일/런타임·GPU 테스트는 프로젝트 루트 `CMakeLists.txt` 등 누락으로 **미실행**.


## 2026-11-09 — Engineering Standards 준수 감사 및 네이밍 프로필 확정
- **읽은 문서:** AGENTS·Document Map, Code Style·Local Numbering·Architecture Organization, C++·GLSL·Python·Shell, Source Comments·Doxygen·Markdown, Workflow·Verification·Commit Style, Project Brief·Constraints, Source Map·CPU–GPU Interface 및 기존 Work Log.
- **코드 변경:** 없음. 사용자 결정에 따라 목표 프로필을 Unreal-Inspired로 문서화하고 현재 소스의 미준수 항목을 감사했다. 소스 리네이밍·자동 포맷 미수행.
- **문서 동기화:** Project Brief의 활성 네이밍·빌드/포맷 설정, Constraints의 실제 파일 존재 여부 갱신. 준수 감사 보고서 (과거 운영 기록: 원본 `Archive.zip`의 `04_Development/10_Engineering-Standards-Audit.md`)에 근거·전수 패턴 검사와 수동 검토 한계·전환 순서 기록. API·ABI·실행 흐름이 바뀌지 않아 Architecture 변경 없음.
- **검증:** Python AST 44/44, Bash 구문 5/5 통과. clang-format 20.1.8: C++ 195개 중 35개 불일치. 실제 Manifest의 셰이더 75개 엔트리·변형을 검증 매크로 0/1로 컴파일하고 spirv-val 검사: 150/150 통과. C++ 빌드·CTest·GPU 실행·성능·ABI Reflection은 미실행. 명명/주석 집계는 정적 패턴 검사이며 완전한 컴파일러 AST/의미 검증이 아님.


## 2026-11-09 — Code Style 구조·의미 체크리스트 후속 리뷰
- **읽은 문서:** AGENTS·Document Map·Code Style, Scene/Input/Debug UI 및 Application/Frame Source 문서. 기존 규칙 감사 결과와 실제 Scene/Camera/DemoAnimation/Application/Renderer 흐름 대조.
- **코드 변경:** 없음. 접두어와 별개로 상위 흐름·추상화 수준·책임·오류 경로를 리뷰했다.
- **문서 동기화:** 규칙 감사 보고서 (과거 운영 기록: 원본 `Archive.zip`의 `04_Development/10_Engineering-Standards-Audit.md`)에 §05의 6개 체크리스트 판정, 구체적 미준수와 준수 사례, 보존할 순서/계약 및 검토 범위를 추가. API/ABI/동작 변경이 없어 Architecture 변경 없음.
- **검증:** 정적 소스 수동 리뷰만 수행. 이번 후속 작업에서 빌드·테스트·GPU 실행·성능 검증 미실행. 기존 셰이더 검사 결과와 구분한다.


## 2026-11-09 — GLSL 규칙 후속 리뷰
- **읽은 문서:** AGENTS·Document Map·GLSL, CPU–GPU Interface 및 Debug/Shared Shader Source 문서. 기존 Code Style 감사 기준과 함께 Debug/Solver/Overlay 및 CPU pipeline/draw 경로 대조.
- **코드 변경:** 없음. Shader 구조 패턴 전수 검색, 주요 entry/helper의 책임·로컬 계약·경계/barrier 조건을 수동 리뷰했다.
- **문서 동기화:** 감사 보고서 (과거 운영 기록: 원본 `Archive.zip`의 `04_Development/10_Engineering-Standards-Audit.md`)에 GLSL 규칙별 근거, SurfaceDebug.vert/frag 개선 대상, bounds 전제조건 확인 과제, 정상 사례 및 검토 한계 추가. API/ABI/실행 변경이 없어 Architecture 변경 없음.
- **검증:** 이번 작업은 정적 리뷰만 수행. 새 shader 컴파일·Reflection·Pipeline 생성·GPU 실행·성능 측정 미실행. 이전 150개 compile/spirv-val 결과와 구조·안전성 리뷰를 구분함.


## 2026-11-09 — 변경 전 최소 빌드·테스트 기준선 확보
- **읽은 문서:** AGENTS·Document Map·Verification and Review, Validation and Tests의 테스트/fixture·실행 기준, 기존 build script 및 CMake 테스트 설정.
- **코드 변경:** 없음. 스냅샷 808개·SHA-256·HEAD·미커밋 patch 저장 후 별도 Debug build/CTest 및 Manifest shader 검증 실행. 보조 runner/로그는 결과 디렉터리에만 저장.
- **문서 동기화:** Validation and Tests 상단에 이번 환경의 실제 결과·한계·[증거 위치](../../../Build/Validation/20261109-210554-pre-refactor/summary.md) 추가. API/ABI/실행 흐름 변경이 없어 Architecture 변경 없음.
- **검증:** configure/전체 build 성공. CTest 14개: 7 통과·7 실패·0 skip(종료 코드 8). 셰이더 150개 glslc/spirv-val 통과. 실행 중 입력 해시 변경 0·ZIP CRC 정상·기존 Build cache 유지. 실패는 assertion 5개/Scene 입력 누락 1개/GPU 한도 1개로 구분하고 실제 failed 상태 보존. 성능/대규모 시각·물리 동등성은 미실행.


## 2026-11-09 — Universal 규칙 동기화·전 언어 folding 표기·Shader 접기 반영
- **읽은 문서:** 양쪽 AGENTS·Document Map, Universal Code Style·Local Numbering·GLSL·Python·Shell·JS/TS·Source Comments·README, 현재 Workflow·Verification·Shader Source Index·Current Work. 설치된 Custom Folding README/schema 및 Shader 확장 language ID도 확인.
- **코드/설정 변경:** Shader 50개에 84개 comment region, 작은 19개 유지. 기존 Source/Tests 수정 없음. root/vault VS Code에 glsl/shellscript folding 정규식과 shader 언어 연결. 원본 Universal 볼트 수정 없음.
- **문서 동기화:** 공통 규칙 7개 및 AGENTS/README/공통 라우팅 병합, Shader Source Index·Current Work의 후속 순서 갱신. 프로젝트 Context·네이밍·기존 실험 보존. 계산/API/ABI/호출/barrier가 동일해 Contract/Flow 본문 변경 없음.
- **검증:** 69개 주석 제외 코드 동일·84개 region 균형/번호·기존 개행 유지. shader 150개 glslc/spirv-val 통과 및 직전 기준선 SPIR-V와 모두 바이트 동일. 전체 C++/CTest/GPU 실행은 주석 전용 변경이라 반복하지 않음. UI folding 화면은 미확인. [상세 증거](../../../Build/Validation/20261109-211718-vault-region-folding/summary.md).


## 2026-11-09 — 현재 상태 분할 커밋 및 다음 작업 확정
- **읽은 문서:** 루트/볼트 AGENTS·Document Map·Commit Style와 기존 감사/기준선/Current Work 기록.
- **작업:** refactor/engineering-standards에서 기존 미커밋 상태를 기능/연구/실행 도구/공통 소스 정리/GLSL folding/볼트 이관으로 분리해 보존했다. 기능 변경과 섹션·개행 변경이 같은 파일에 있던 부분은 index에서 분리하고 작업 폴더 내용은 유지했다.
- **커밋 단위:**
  - `405a5cc Feat(Rendering): Burn·Scorched 시각화와 반응 비교 씬 생성기 추가`
  - `6a979d3 Chore(Research): 결합·모듈 기준선과 저작 검증 도구 보존`
  - `d16e1a2 Build(Scripts): 기존 빌드 산출물 실행 진입점 추가`
  - `49390b0 Style(Code): 기존 섹션 번호와 주석·소스 개행 정리`
  - `a73ac04 Style(Shaders): 책임별 주석 Region folding 적용`
  - `Docs(AgentVault): 공통 규칙 이관과 감사·기준선 기록 보존` — 이 작업 기록을 포함하는 마지막 커밋.
- **문서 동기화:** 현재 Agent-Vault, JS/TS 및 언어별 folding 규칙, Source/Contracts/Flows, 감사·검증·연구 기록을 기존 볼트 삭제와 함께 이관. 독립 Git 저장소인 로컬 Universal-Agent-Vault는 .gitignore로 제외하고 실제 반영된 규칙은 본체 볼트에 보존했다. 루트 .vscode 및 Build 검증 원자료는 기존 ignore 정책에 따라 로컬 유지하며, 공유 가능한 볼트 .vscode 설정은 이관 문서에 포함한다.
- **검증:** 분할 전 작업 파일 531개 존재/해시 검사 일치. 공통 정리 staged 297개는 C++/GLSL의 주석·Region·공백을 제외한 코드 동일, 다른 설정/fixture/스크립트는 CRLF/LF 정규화 후 동일. 연구 Python 27개 AST·계획 JSON 확인. 기존 전체 Debug build 성공, CTest 7 통과/7 실패/skip 0, shader 150개 검사·기준선 바이트 동일 결과를 보존. 이번 분할 작업에서 전체 빌드/CTest/실험을 재실행하지 않았다.
- **남은 제한:** 비교 Scene 생성기는 보존했으나 생성된 ReactionTransitionComparison.Scene·Profile·Mesh는 현재 checkout에 없어 실행 전 생성이 필요하다. 기존 assertion 실패·누락 입력·GPU 한도 문제는 수정하지 않았다.
- **다음 구현:** 기존 3번 C++ 구조 정리 — LoadDemoAnimation → MainLoop → RecordCommandBuffer. 이번 작업에서 해당 리팩터링이나 push는 수행하지 않는다.


## 2026-11-09 — C++ 상위 흐름 정리 (기존 3번)
- **읽은 문서:** AGENTS·Document Map·Code Style·C++·Local Numbering·Source Comments·Workflow·Verification·Constraints, Application/Scene/Rendering Source, Render Flow·CPU–GPU Interface 및 기존 기준선 기록.
- **코드 변경:** LoadDemoAnimation을 파일/설정/weather/tracks/contact 단계로, MainLoop를 입력·시간·demo contact·GPU wait·마무리 단계로, RecordCommandBuffer를 simulation/inspection·display preparation·graphics/viewport·UI/end 단계로 추출. Renderer private frame/view 상태는 자원을 장기 소유하지 않으며 기존 vector를 move. 원래 값 계산·검증·예외·Vulkan call/query/barrier 순서 유지. 오류 정책·타입명 일괄 변경·GLSL 구조 변경은 후속 범위.
- **문서 동기화:** Application/Scene/Rendering Source·Render Flow·Current Work·Validation 갱신. Contract/ABI 데이터와 수용 기준은 변경되지 않아 계약 본문 변경 없음. 감사의 원래 관찰은 역사 기록으로 유지하고 Current Work에서 완료 상태 안내.
- **검증:** Debug 전체 build 성공. CTest 7 통과/7 실패/skip 0이며 테스트별 상태/이유가 기준선과 동일. 파서 35개 old/new 비교 동일, Vulkan call 지점 81개 정적 순서/인자 동일, render-only old/new 5프레임 PPM/settings/validation 동일. simulation Scene은 양쪽 모두 기존 33-buffer GPU 한도로 초기화 실패. overlay 전체 동등성·성능·장시간·UI suspend 조작은 미검증. clang-format 5개 통과 및 CRLF-aware diff check. [실행 증거](../../../Build/Validation/20261109-213530-cpp-top-down/summary.md).
- **커밋:** 이번 구현 변경은 미커밋, push 없음. 다음 작업은 기존 4번 GLSL 계산 구조 정리.


## 2026-11-09 — Shader 누락 Region 및 CRLF 자동 접기 수정
- **읽은 문서:** AGENTS·Document Map·Code Style·GLSL·Verification·Shader Source Index·Project Brief. 설치된 Maptz/Fold on Open/Custom Folding 확장 구현과 VS Code Fold All Regions 처리도 로컬 코드로 확인.
- **코드/설정:** 짧아서 제외했던 shader 19개도 사용자 요청에 따라 comment Region 추가(전체 69개·103개). root/vault 설정에서 CRLF 대응 regex·defaultFoldStartRegex·자동 접기 지정. 사용자 전역 settings와 기존 C++ 리팩터링은 수정하지 않음.
- **원인:** 이전 시작 regex가 확장 parser에 남은 CR을 소비하지 못함을 실제 설치 parser에서 재현. AllMarkerRegions 값 자체는 유효하며 이미 인식된 영역만 접는 명령임. 초기 언어 provider 등록을 위해 Reload Window가 필요할 수 있음.
- **문서 동기화:** Project Brief에 모든 shader 적용 정책을 명시하고 README·Shader Source Index·Current Work·Validation 갱신. 계산/리소스/ABI/패스 순서 변경이 없어 Flow/Contract 본문 변경 없음.
- **검증:** 실제 설치 provider를 mock 값 API와 실행해 69개·103개 typed/default Region 확인. LF/CRLF 회귀 통과, 69개 주석 제외 코드·개행 동일. 150개 glslc/spirv-val 통과 및 기준선 SPIR-V 모두 바이트 동일. CUA native pipe 연결 실패로 UI 화면은 미확인. 전체 C++/CTest/GPU 실행 반복 없음. [증거](../../../Build/Validation/20261109-215915-shader-folding-fix/summary.md). 커밋/push 없음.

## 2026-11-09 — GLSL 상위 흐름 정리 (기존 4번)
- **읽은 문서:** AGENTS·Document Map·Code Style·GLSL·Source Comments·Verification, Solver/CPU–GPU 계약, Debug/Overlay/Solver Source와 기존 검증 기록 및 CPU 바인딩·dispatch 구현.
- **코드 변경:** Debug의 조회·변위/미분·진단 출력, Overlay의 dirty scan·예약·baseline/삼각형 compaction·segment 출력/combined sampling·normal·layer 합성, Solver의 raw outgoing·gather·state 갱신·reaction 적분을 책임별 helper로 추출. Geometry/Coverage helper 정의를 main 아래 배치. 총 13개 파일이며 기존 layout·수식·접근/분기 우선순위·barrier 참여·variant scope 보존. Shader folding은 전체 69개·116개 영역. 기존 C++ 5개 및 테스트 기대값 유지.
- **문서 동기화:** Debug/Overlay/Solver Source·Shader Index·README·Current Work·Validation 갱신. Contracts/Flows 검토 결과 pass 순서·수용 기준·ABI가 같아 계약/흐름 본문 변경 불필요.
- **검증:** 최종 build 성공, shader 150개 컴파일/spirv-val 통과 및 reflection 동일. CTest 7 통과/7 실패·skip 0, 상태/실패 원인 문구 기준선 동일. 별도 debug helper는 전후 동일한 기존 assertion 실패이고 도달한 GPU 출력 16개 동일, validation ERROR 없음. Region provider 116개 인식, CRLF-aware whitespace 확인. Overlay 전체·모든 mode/variant GPU 출력·성능/장시간/다른 장치는 미검증. [증거](../../../Build/Validation/20261109-221158-glsl-top-down/summary.md).
- **커밋:** 미커밋·push 없음. 다음 단계는 기존 5번 Unreal-Inspired 네이밍.

## 2026-11-09 — Unreal-Inspired 네이밍 전환 (기존 5번)
- **읽은 문서:** AGENTS·Document Map·Code Style·C++·Verification·Source Comments·Python, Project Brief·Constraints, Source Map·C++ Index·CPU–GPU Interface·GPU Layout/ABI Source 및 직전 검증 기록.
- **코드 변경:** 일반 타입/비템플릿 별칭 F·enum E·scalar bool b로 선언/정의/호출자/테스트/Research를 동기화(186개 C++ 파일). libclang 선언·참조 기반 bool 치환 후 generic lambda 및 기존 오류가 있는 실험 식의 누락 참조를 보완. TScopeExit/외부 타입, 파일명, 문자열/Scene schema, GLSL 선언·리소스/byte layout 유지. Python source marker 1개와 shader CPU 대응 주석 3개 동기화.
- **문서 동기화:** 현재 Architecture/Source/Contracts/Flows의 C++ 심벌 참조·GPU CPU/GLSL 명칭 구분, Project Brief·Current Work·Validation 갱신. 과거 실험/Work Log/ADR의 당시 이름은 보존. 계산·수명·pass/barrier·저장 형식 변경 없음.
- **검증:** 최종 Debug build 성공. CTest 7 통과/7 실패·skip 0, 상태/실패 원인 문구 동일. AST 100 TU 오류 0·scalar bool 553개/타입 규칙 적합. 계획 외 코드 토큰 변경 없음·문자열/개행 유지. shader 150개 통과·SPIR-V 바이트 동일, render-only 전후 화면/settings/validation 동일·ERROR 0. Source/Tests format dry-run 181개 통과. Research syntax 진단은 전후 동일한 기존 8개이며 별도 연구 실행/전체 overlay·성능·다른 플랫폼은 미검증. [증거](../../../Build/Validation/20261109-222214-unreal-naming/summary.md).
- **커밋:** 미커밋·push 없음. 다음 단계는 기존 6번 문서·스타일 정리.

## 2026-11-09 — Universal 5.5 규칙 반영 및 FDebugUI 1단계 구획
- **문서 먼저 반영:** 로컬 Universal `93dbbed`의 공통 2단계 지침, Code Style·언어 6개·검증·워크플로·구현 템플릿·AGENTS를 반영. Document Map은 공통 행만 추가하고 프로젝트 라우팅/규칙/설정 보존. 현재 우선순위와 후속 후보를 `11_Section-and-Responsibility-Plan.md`에 기록.
- **1단계 코드:** DebugUI.h에 Public API/기능별 메서드/관련 상태의 14개 섹션과 Region 추가. 멤버 선언·초기화·접근 지정자·코드 토큰 순서 유지.
- **검증:** 주석/Region 제외 토큰 동일, CRLF 및 Region 균형/지역 번호 확인. MDSS Debug target build 성공. 별도 stage1 헤더/검증 파일 보존. 이 구획만으로 책임 분리가 완료됐다고 보지 않음.
- **다음 별도 작업:** private profiling 상태와 Reset/Accumulate/Publish 동작 추출·검증. 커밋/push 없음.

## 2026-11-09 — FDebugUI 2단계 Profiling 책임 추출
- **읽은 문서:** 반영된 5.5 공통 지침·Code Style·C++·Verification·Document Workflow, Debug UI Source와 현재 초기화/리셋/metric 소비자, 기존 검증/CPU–GPU 경계 기록.
- **별도 코드 변경:** 1단계 보존 후 ProfilingUIState.h/.cpp의 FProfilingUIState에 값 소유 필드 13개·sample record·Reset/Accumulate/PublishWindow 추출. FDebugUI는 기존 API/metric 수집/시간·mode guard/ImGui 출력·reset 호출을 유지한다. 새 할당/간접 소유권·settings schema·GPU ABI 변경 없음. private layout 변화로 소비자를 함께 빌드.
- **검증:** 원래 transition 본문 정규화 및 UI inline expansion 토큰 대조 일치. reset 호출자 6개 파일 동일. 기존 코드 reference와 2,000프레임·4,023회 상태 비트 일치. 전체 build 성공, CTest 7 통과/7 실패·skip 0와 원인 문구 동일, render-only 전후 화면/settings/validation 동일·ERROR 0. format/Region/whitespace 통과. 실제 모든 UI/장시간·성능·다른 장치 및 기존 GPU 한도 경로 미검증.
- **동기화:** Debug UI Source·5.5 계획·Current Work·Validation 갱신. API/직렬화/GPU·pass 흐름 의미가 같아 Contracts/Flows 본문 추가 수정 불필요. [증거](../../../Build/Validation/20261109-224136-vault55-debugui/summary.md).
- **상태:** FDebugUI 첫 적용만 완료. 나머지 상태/대형 파일은 후보별 검토 후 별도 두 단계로 수행. 기존 미커밋 작업 보존·이번 커밋/push 없음.

## 2026-11-09 — Universal 5.5 프로젝트 전체 1단계
- 사용자 지시로 FDebugUI 첫 적용에서 Source/Shaders/Scripts/Tests/루트 실행 도구 전체(319개 파일)로 확대. 외부/생성물/중첩 템플릿 볼트/자산은 제외. 150줄은 후보 탐색용이며 작은 단일 책임 파일은 무리하게 분리하지 않는다.
- 먼저 42개 파일의 누락 구획 정리: Renderer/SurfaceStateSystem 등 헤더·테스트 시나리오 및 Python의 imports/config/helper/main/execution 구획. 기존 GPU shader 구획과 작은 실행 진입점은 유지.
- C++ 코드 토큰·선언/접근 순서 동일, Python AST 동일, 전체 Debug build 성공. stage1 스냅샷/검증을 보존한 뒤 실제 책임 추출을 별도 작업으로 진행한다. 기존 작업/실패 정책 보존·커밋 없음.

## 2026-11-09 — Universal 5.5 프로젝트 전체 2단계 및 전수 대조 완료
- **읽은 문서:** AGENTS·Map·Code Style·공통 2단계·활성 C++/GLSL/Python/Shell·Verification·현재 Source/CPU–GPU·Solver·Rendering/UI 경계와 기존 검증.
- **코드:** UI·Renderer·Solver 8개 값 소유 상태(97개 필드), frame timing/solver preparation·view popup/설명·world geometry 모듈 추출, Python 7개 처리 함수와 Shell preflight/cache 분리. GLSL 나머지 6개 entry의 helper 구현 배치를 main 아래로 정리. 자원/초기값/생성·리셋·종료/공개 API·문자열/직렬화·GPU record/dispatch/barrier 의미 유지.
- **전수 판단:** 323개 구현 + CMake 2개를 대조. 작은 파일·이미 단일 책임 또는 고정 ABI/공유 수명 단위에는 불필요한 추출을 하지 않고 파일별 이유를 기록. 구획만 추가한 파일을 책임 분리 완료라고 대체하지 않음.
- **검증:** 실제 추출 상태·동작 역전개 시 C++ 197개 토큰/5개 Python AST 동일. build 성공, CTest 7 통과/7 실패·원인 문구 동일. shader 150개 통과·reflection 동일. render-only 전후 출력/설정/validation 동일, geometry 3개 비트 동일, Python 정상/오류 보고서 및 Bash 6개 fixture 비교 동일. 구문/format/Region/링크/whitespace 통과. 연구 도구 기존 8개 진단 유지. native PowerShell·전체 연구/UI/장시간/성능/다른 플랫폼·기존 GPU 한도 경로 미검증.
- **동기화:** 전체 대조표·5.5 계획·Current Work·Rendering/Solver/UI/Shader Source·CPU–GPU 경계·Validation 갱신. Flow 의미와 pass 계약은 검토 후 유지. [원자료](../../../Build/Validation/20261109-230833-vault55-project-wide/summary.md).
- **상태:** 전체 적용 작업 완료, 미커밋·push 없음. 다음 기존 6번 문서·스타일 정리로 진행 가능.

## 2026-11-09 — 누적 변경 분할 커밋

- 사용자 요청에 따라 기존 변경을 의미 단위로 분리한다. 소스 작업 폴더를 유지하고 보존된 단계별 스냅샷으로 인덱스를 구성했다.
- 검증: 기존 단계별 빌드·테스트·동등성 검증을 근거로 사용한다. 각 중간 커밋을 별도로 빌드한 것으로 간주하지 않는다. 커밋마다 인덱스 공백 검사를 수행한다.
- 분리 항목:
  - Refactor(CPP): 데모 로딩·프레임·명령 기록의 상위 흐름 분리
  - Fix(Shaders): 누락 Region과 CRLF 자동 접기 설정 보완
  - Refactor(Shaders): 디버그·오버레이·솔버 진입점과 helper 정리
  - Refactor(Naming): C++ 타입과 bool에 Unreal-Inspired 접두어 적용
  - Docs(Standards): Universal 5.5의 2단계 리팩터링 규칙 반영
  - Style(DebugUI): 클래스 선언을 책임별 14개 섹션으로 구획
  - Refactor(DebugUI): 프로파일링 누적 상태와 계산 책임 추출
  - Style(Code): 프로젝트 전반의 섹션과 탐색 구조 정리
  - Refactor(Shaders): 나머지 렌더·기하·스케줄링 진입점 정리
  - Refactor(DebugUI): UI 상태 묶음과 뷰 설정 패널 책임 분리
  - Refactor(Rendering): 벤치마크 상태·타이밍·기준 기하 책임 분리
  - Refactor(Simulation): 솔버 실행 상태와 step 준비 흐름 분리
  - Refactor(Tools): 분석 보고서와 빌드 스크립트의 책임 분리
  - Docs(AgentVault): 전수 적용 범위·검증 결과와 분할 커밋 기록 동기화

# 2026-11-10 — 공유 Windows MSVC CMake 프리셋

- **읽은 문서:** Agent Vault 지침·Document Map·Code Style·Document Workflow·Verification and Review, 기존 CMakeLists/빌드 스크립트/README와 Build 캐시.
- **변경:** `Visual Studio 18 2026` + x64 구성 및 Debug/Release 빌드 프리셋을 루트 `CMakePresets.json`에 추가. 기존 `Build` 출력 경로를 사용하고 로컬 컴파일러 경로는 고정하지 않는다.
- **문서 동기화:** README에 프리셋 사용 안내와 CMake 4.2 최소 버전을 추가. Architecture/Contracts/Flows 영향 없음.
- **검증:** `cmake --list-presets=all`에서 configure/build 프리셋 3개 인식 확인. 실제 빌드 미실행.

## 2026-11-10 — Universal v5.8 소스 가독성 및 설정 이관

- **읽은 문서:** 프로젝트/최신 AGENTS·Document Map, v5.8 CHANGELOG·Code Style·Local Numbering·단계적 구조화·C++/GLSL/Python/Shell·Source Comments·Workflow·Verification, Project Brief·Constraints·Source Map·Rendering Flow·GPU Layout·CPU–GPU 계약과 이전 v5.5 작업 기록.
- **코드:** 추적 구현 323개 대조, Source 126·Shaders 58·Scripts 49·Tests 9 = 242개 정리. 의미 단위 공백·긴 조건/조기 반환 서식과 GLSL 중괄호를 보완하고, 조명·솔버·Renderer API·View Settings의 6개 큰 영역에만 선택적 하위 Region 추가. 새 타입/함수 추출·명명 전환·GPU 인터페이스 변경 없음. 미추적 test.glsl 보존.
- **호환성:** 현재 MSVC가 원본의 08/09 Region 라벨을 C2041로 거부해 해당 파일과 새 하위 Region C++ 파일의 이름을 인용했다. GLSL에는 Shaders/.clang-format 추가. 사용자 전역 설정은 변경하지 않고 root/vault 접기 설정을 v5.8 LF/CRLF 정규식으로 병합했다. root .vscode는 기존 ignore 정책 유지.
- **문서 동기화:** 공통 규칙 변경 6개·AGENTS·Document Map·Changelog, Project Brief·Constraints·README·Current Work·Source Map·Validation 및 v5.8 파일별 적용표 (과거 운영 기록: 원본 `Archive.zip`의 `04_Development/13_Vault-5.8-Readability-Coverage.md`) 갱신. upstream은 HEAD 93dbbed 위의 v5.8 작업 트리를 기준으로 함. Contracts/Flows는 실행/데이터 의미가 같아 정의 변경 불필요. 검증 문서의 표를 끊던 설명 위치도 보완.
- **검증:** C++ 201개 토큰·Python 44개 AST 대조 통과, GLSL은 제어문 중괄호 추가 외 기존 토큰 유지. 최종 Debug 전체 build 성공. shader 150개 컴파일/spirv-val 통과·SPIR-V 바이트 동일. CTest 변경 전/후 7 통과·7 기존 실패. 실제 Maptz parser 74개 LF/CRLF 148회 통과(133 shader·2 shell Region). Python formatter check 및 CRLF 대응 공백/Region 검사 수행. [증거](../../../Build/Validation/20261110-vault58/summary.md).
- **한계:** 기존 테스트 실패 수정, 별도 성능·전체 렌더 캡처·다른 GPU/OS·실제 편집기 접힘 UI 검증은 범위에 포함하지 않았다. commit/push 없음.

## 2026-11-10 — Shader 중첩 Region 전수 조사·추가

- **읽은 문서:** AGENTS·Document Map·Code Style·Local Numbering·GLSL·Source Comments·Verification, Project Brief·Shader Source Index·CPU–GPU 경계 및 실제 shader의 함수·resource·진입점·전처리/동기화 구조.
- **검토/변경:** 디스크와 Git shader 목록 69개 일치 확인. 35개 파일에 하위 Region 115개 추가, 기존 4개 중첩 파일 유지, 나머지 30개는 파일별 미추가 근거 기록. Debug record/적층/normal·뷰 종류, Overlay coverage/geometry/compositing, scalar/vector sampling, solver layout/geometry/transport/scheduling을 실제 책임으로 구획. 최상위 유지·한 줄 하위 제목·연결 번호·논리적 들여쓰기·총 2단계 준수. 함수별·검증문별 기계적 생성은 하지 않았다.
- **보존:** 기존 미커밋 변경, 계산 토큰·선언/함수 순서·layout/binding·매크로/오류 경로·barrier 호출 순서·줄바꿈 유지. shared halo/schedule reset의 barrier는 진입점 구획 사이에 보이며, synchronization helper 구획은 제목에 명시.
- **문서:** 69개 파일별 검토표 (과거 운영 기록: 원본 `Archive.zip`의 `04_Development/14_Shader-Nested-Region-Coverage.md`), README·Project Brief·Shader Source Index·Current Work·최초 v5.8 적용표의 후속 안내·Validation 갱신. Contracts/Flows 검토 결과 ABI·데이터/실행 의미의 변경이 없어 본문 변경 불필요.
- **검증:** 69개 비주석 토큰 동일. Region 짝/포함/상하위 번호/들여쓰기·최대 2단계 및 새 범위의 brace/전처리 경계 검사 통과. shader 150개 glslc/spirv-val 통과·전후 SPIR-V 바이트 동일. 실제 Maptz parser LF/CRLF 138회 통과(각 248영역). CRLF-aware diff check 통과. [원자료](../../../Build/Validation/20261110-shader-regions/summary.md).
- **미실행:** 실제 편집기 접힘 UI, 전체 C++ build/CTest·별도 GPU 성능/렌더 캡처는 주석만 추가한 이번 작업에서 반복하지 않았다. commit/push 없음.

## 2026-11-10 — 현재 변경 의미 단위 분할 커밋

- **근거:** 사용자 요청 및 AGENTS·Document Map·Commit Style에 따라 누적 변경을 분리했다. 작업 파일은 유지하고 보존된 셰이더 스냅샷으로 서식 변경과 후속 중첩 Region 추가를 구분했다.
- **커밋:** `f07410e` CMake 프리셋·사용 안내, `97ce059` v5.8 공통 규칙·접기 설정, `b96bbe1` C++ 서식·Region·MSVC 라벨, `3d1901e` Python 서식, `d34fc77` 셰이더 v5.8 가독성 정리, `50cd941` 셰이더 중첩 Region 전수 적용. 적용표·프로젝트 현황·검증 결과와 이 기록은 마지막 문서 커밋으로 묶는다.
- **검증:** 각 커밋의 인덱스에 CRLF 대응 공백 검사 수행. 코드·설정 커밋 전후 작업 파일의 SHA-256 동일 확인. CMake 프리셋 3개 인식 재확인. 앞선 최종 Debug build 성공·shader 150개 검증 및 SPIR-V 동일·CTest 7 통과/7 기존 실패 결과를 사용하며, 각 중간 커밋을 별도로 빌드한 것으로 간주하지 않는다.
- **문서 검토:** 변경 내용과 두 적용표·Validation·Current Work·Source Index의 일치 여부 확인. 실행 의미·GPU 인터페이스 추가 변경이 없어 Contracts/Flows 수정 불필요. 원자료는 `Build/Validation/20261110-split-commits/`에 보존하며 push는 수행하지 않는다.

## 2026-11-10 — af8085be 누락 자산·실험 기록 복원

- **읽은 문서:** AGENTS·Document Map·Verification·Constraints, Reaction 계약·비교 씬 안내·기존 Work Log 및 성능 기록 인덱스.
- **변경:** 사용자 요청으로 `af8085be762bdf3a17d487ee6fe4a1d563f219a0`에는 있고 현재 없는 파일 159개를 원본 바이트 그대로 복원: Assets 13개, 이전 볼트 Docs 20개, Performance-Data 126개. 기존 파일을 덮어쓰지 않았으며 Reaction 소스·GPU ABI·계산 규칙은 변경하지 않았다. 판 Mesh 두 개의 `.gitignore` 예외 복구.
- **문서 동기화:** Current Work·성능 자료 인덱스 갱신. 구 볼트에 역사 보관본 안내 추가, 현재 Agent-Vault의 운영 규칙 유지. Contracts/Flows는 기존 Scene 참조가 다시 충족되어 정의 변경 불필요. 과거의 누락 기록은 당시 상태로 보존한다.
- **검증:** 복원 159개 파일 모두 원본과 SHA-256/바이트 일치. 현재 Debug 실행 파일로 비교 씬 120 warmup + 1 measurement 및 drain 실행, 종료 코드 0·총 123 Solver step. 3개 instance 모두 negative/nonfinite·Reaction budget/arithmetic 진단 0, 로그 ERROR/Validation Error 없음. CRLF 대응 diff check 통과. 원자료: `Build/Validation/20261110-af8085-restore/`.
- **한계:** 자산·보관 기록 복원이므로 전체 build/CTest·성능 재측정·구 버전과 런타임 동등성 비교는 반복하지 않았다. 소스 커밋에서 추적하지 않았던 raw 자료는 이번 복원에 포함되지 않는다. commit/push 없음.

## 2026-11-10 — 31a38c4 WIP 누락 기능·수명 수정 이식

- **읽은 문서:** AGENTS·Map·Code Style·C++/GLSL·Source Comments·Verification, Reaction·GPU Layout·CPU–GPU 계약, Simulation Flow·GPU/Rendering Source와 WIP 첫 부모 대비 코드.
- **코드:** 기존 F/E 명명·private 상태 추출·중첩 Region을 보존하며 descriptor 재연결, opt-in Reaction ledger와 추가 scratch, frozen/full geometry 제어, 초기 입력 observer·동일 step 이미지/State/geometry 캡처, geometry_feedback 설정 반영. 연구 CMake 타깃·shader 의존성 및 생성 소스 타입명 복구. descriptor 수명 회귀·compact vertex 기준 기하 테스트 수정, Bash 빌드/앱 인자 분리·실행 복원. 기존 자산 복원 작업은 유지.
- **문서:** Reaction·GPU Layout 계약, Simulation Flow, GPU/Rendering Source, README·Current Work·Validation 및 이식 범위·검증 보고서 (과거 운영 기록: 원본 `Archive.zip`의 `04_Development/15_WIP-Recovery.md`) 동기화. 기존 descriptor binding·push constant 크기·제품 Reaction 수식은 유지하고 연구용 추가 allocation/flags·readback 수명만 명시했다.
- **검증:** 최종 전체 Debug build 및 연구 executable 2개 성공, shader 150개 compile/spirv-val 통과. CTest 15개 중 9 통과/6 기존 실패: 새 descriptor 수명 검사와 수정한 SharedSurfaceGeometry 통과. headless 4조건 각 15 step 성공·ledger ON/OFF State 비트 동일·extent 누적 확인. AuthoringProbe 2사례 통과. 일반 비교 씬 123 step의 State 3개가 이식 전과 비트 동일, 0/15/60 checkpoint 및 geometry_feedback=false 반영 확인. Bash 구문·LF fixture 인자 전달, CRLF 대응 diff check 통과. [원자료](../../../Build/Validation/20261110-wip-port/runtime-summary.json).
- **한계:** 기존 6개 실패 수정·장시간/다른 GPU·OS·독립 시각 품질·성능 재측정·과거 WIP 바이너리와 전체 동등성 검증은 미수행. commit/push 없음.

## 2026-11-10 — WIP 이식 코드의 Engineering Standards 보완

- **읽은 문서:** AGENTS·Map·Code Style·단계적 책임 분리·Local Numbering·C++/Shell·Source Comments·Doxygen, 기존 GPU/Reaction 계약·Simulation Flow·Rendering Source 및 이식 검증 기록.
- **코드:** checkpoint의 버퍼/instance 저장·진단 집계·이미지·metadata 출력을 익명 namespace 내부 함수로 추출, 상위 함수는 GPU 대기·중복/대상 검사·저장·완료 갱신 순서 유지. 책임별 2단계 Region·bBGRA·의미적 공백·명시적 분기 블록 보완. 새 API의 한국어 호출 조건/참조 수명/오류 문서와 중복 주석 정리. Bash 인자 해석·실행 helper 분리. 공개 API·멤버 순서·GPU ABI·출력 schema 유지.
- **문서 동기화:** Rendering Source·Current Work·WIP 보고서·Validation 갱신. Contracts/Flows 검토 결과 실행/데이터 의미는 같아 추가 정의 변경 불필요.
- **검증:** 전체 Debug build 성공. 관련 CTest 3개(ScratchDescriptorLifetime·SharedSurfaceGeometry·LocalReactions) 통과. step 0/15/60 State·metadata 및 최종 State 3개가 보완 전과 바이트 동일. 캡처 이미지 차이는 실시간 timing 숫자 영역에 한정, 나머지 픽셀 동일. 잘못된 serial·누락 target·기존 checkpoint 보호 오류 경로 3개와 Bash 구문/LF fixture 인자 전달 통과. Region·CRLF 대응 diff check 확인. [원자료](../../../Build/Validation/20261110-wip-standards/summary.json).
- **미실행:** 전체 CTest·headless 연구 4조건·별도 성능·다른 장치 검증은 영향 범위를 벗어나 반복하지 않았다. 최초 이식의 전체 CTest 9 통과/6 실패와 구분한다. commit/push 없음.

## 2026-11-10 — 구 볼트 재폐기 및 Universal v5.9 주석 전수 반영

- **읽은 문서:** 현재/최신 AGENTS·Document Map, v5.9 CHANGELOG·Code Style·Local Numbering·책임 분리·Source Comments·Doxygen·활성 언어·Verification, Project Brief·Source Map과 기존 GPU/Reaction 계약·검증·이관 기록.
- **정정:** 앞선 복원에서 폐기된 MDSSP-Agent-Vault 문서까지 누락 파일로 취급한 판단을 바로잡았다. 원본 일치를 확인한 구 문서 20개·보관 안내 1개를 Docs에서 제거하고 Git 제외 검증 경로에만 보관한다. 운영 볼트는 Docs/Agent-Vault 하나다. 데모 에셋 13개·실험 자료 126개 복원은 유지한다. 연구/보고 Python 11개와 Raw Archive 안내의 활성 경로를 실제 재번호화된 현재 문서로 이관했다.
- **코드:** 구현 323개·Region 656개 전수 대조. 246개 파일에서 역할·설명을 보완, 머리말 171개 갱신. Region 설명은 시작 바로 아래에 654개 추가·기존 1개 유지·실행 없는 예약 구획 1개는 근거를 두고 생략. CMake 구성 4개에도 역할 머리말 추가. C++ @file 불일치 5개 수정. 기존 EOL·Region 경계·함수/선언 순서·ABI·알고리즘과 이전 WIP 기능은 유지한다.
- **문서:** 공통 Standards 12개·AGENTS·Map·CHANGELOG·README와 Context·Source Map·Current Work·Validation 및 v5.9 적용표 (과거 운영 기록: 원본 `Archive.zip`의 `04_Development/16_Vault-5.9-Comment-Coverage.md`) 동기화. 프로젝트 네이밍/MSVC 예외 유지. Contracts/Flows 검토 결과 실행·데이터 정의 추가 변경 불필요. 과거 판정·manifest는 역사 원본으로 보존한다.
- **검증:** 최종 configure/전체 Debug build 성공. C++ 201·GLSL 69 비주석 토큰 동일. Python 44 AST는 모듈 docstring·승인된 문서 경로 외 동일, Bash 5 구문/토큰·PowerShell 4 parser/비주석 토큰 통과. shader 150개 compile/spirv-val·이전 WIP SPIR-V와 바이트 동일. 실제 Maptz LF/CRLF 148회 통과(각 252영역). Region 짝/깊이 2·설명 인접성/들여쓰기·문서 대상 존재·CRLF 대응 diff check 확인. [원자료](../../../Build/Validation/20261110-vault59/summary.json).
- **미실행:** 주석·문서 라우팅 변경이므로 전체 CTest·렌더·성능·전체 연구 파이프라인은 반복하지 않았다. Python __doc__/help는 한국어 역할 설명을 반영한다. IDE 실제 접힘 UI·다른 GPU/OS는 미검증. commit/push 없음.

## 2026-11-10 — 진입 함수·중첩 Region 구조 점검

- **입력:** 현재 Source와 Shader, Project Docs의 기존 탐색 규칙.
- **소스:** Application·SceneLoader·Renderer·DebugUI의 초기화/프레임/뷰 선택 경로에서 세부 작업을 의미 단위 함수로 추출했다. Render/Debug/Geometry shader의 주요 `main()`은 짧은 호출 흐름으로 재배치하되 공유 메모리 동기화는 원위치를 유지했다.
- **서식:** Shader 전체의 중첩 Region 들여쓰기와 종료 마커를 검사·수정하고, 작업 성격이 달라지는 곳에 빈 줄과 목적 주석을 배치했다.
- **문서:** Code Reading Guide 및 Source별 탐색 경로를 실제 함수명에 맞춰 수정했다. Project Brief와 Architecture 정본에 작업 차수/버전 명칭을 추가하지 않았다.
- **검사:** ZIP 경로 보존, Region 짝과 중첩 깊이·들여쓰기, 괄호, GLSL 전처리 조건, 문서 링크를 정적 점검했다. 외부 의존성과 완전한 빌드 파일이 없어 컴파일·shader SPIR-V·실행 동등성 검사는 수행하지 못했다.

## 2026-11-24 — 기존 에디터에서 Surface Lab 실행

- **읽은 문서**는 Agent Vault 진입 규칙, C++·Python 작성과 포맷 규칙, 상태·표시 계약, Scene과 Renderer 기록 흐름이다.
- **구현**은 기본 실행의 Kiln spillway, 기존 UI 프레임 안의 1–4뷰, 씬 브라우저 분류, 상태·형상·외관 설정, 에디터의 검증 그림 표시와 PNG 캡처다. 기존 씬은 같은 창에서 전환할 수 있고, 기본 편집 화면의 구름·빗줄기는 선택한 경우에 표시한다.
- **문서와 검증**은 Main Flow, Surface Lab 계약과 코드 맵, Obsidian 렌더링/Pipeline Rationale, [에디터 실행 검증](../05_Experiments/04_Surface-Simulation-Validation.md)에 기록했다. Debug 빌드와 실행 스크립트로 기존 에디터를 열어 Cavity, 4뷰, 실험, PNG와 기존 씬 전환을 확인했다. CTest는 9/10이며 기존 SurfaceMapping 이음매 실패가 남았다.

## 2026-11-24 — 현재 씬 라이브러리와 프로파일 편집

- **화면**에 `File / Window / Capture`, 분류와 검색을 갖춘 `Scene Library`, `Scene Objects`, `Details`, 하단 `Timeline / Experiments / Profiler`를 배치했다. 기본 실행은 Kiln Spillway다. Cavity Fill & Spill 판은 `Cavity Plate`로 표시한다. 뷰 모드는 `Appearance / Geometry / Data / Diagnostics`로 묶고 Heatmap 소스와 팔레트는 뷰별로 지정한다.
- **프로파일**은 `SurfaceLabScene`의 검증된 연결에서 읽는다. `Details → Profile`에서 벽돌의 수송 물성과 천의 용융·흡수·건조 물리를 조절한다. 적용은 현재 실행을 재시작하고 복구는 디스크의 자산 값을 로드한다.
- **배포 에셋**은 `BrickRunoff.Scene`, `SnowOnCanvas.Scene`과 해당 프로파일을 사용한다. 구형 데모 씬, 전용 클립, 프로파일, 데모 생성 스크립트와 기본 성능 행렬을 제거했다. 논문 검증에 사용했던 저작 입력은 연구 픽스처에 이관했다.
- **검증**은 `run.ps1` 기본 장면, 명시한 `SnowOnCanvas.Scene`의 12프레임 실행, 빠진 참조 점검, 눈 용융 계수 0과 점도 변경의 CPU/GPU 경로, 4뷰 표시다. CTest 10개 중 9개가 통과했다. `MDSS_SurfaceMapping`의 기존 이음매 경고 단언이 실패한다.

## 2026-11-24 — 데모 씬 파일과 공통 렌더 기본값

- **읽은 문서**는 C++·주석 규칙, Surface Lab 계약과 코드 맵이다. 카탈로그의 일곱 데모를 `SurfaceLabScene` v1 에셋으로 이관했다. 라이브러리와 CLI가 같은 파일을 읽고 적용 시 로드된 설정으로 실행한다. 절차적 형상과 장면별 국소 규칙은 기존 구현을 사용한다.
- **렌더 기본값**은 `DefaultSurfaceLabView`에서 `PBR + refraction`으로 정한다. 새 세션의 첫 뷰에 적용하며, 에디터에서 선택한 모드는 장면 전환 시 이어 쓴다.
- **문서**는 Surface Lab 계약·코드 맵과 Obsidian의 에셋 배치를 갱신했다.
- **검증**은 `run.ps1`로 `GlacierCut.Scene`을 열어 12프레임을 실행하고, 이관 전후 CPU 기준 결과 JSON 17개가 바이트 단위로 일치하는지 확인했다. GPU 비교가 모두 통과했고 벽돌의 렌더 모드 생략 캡처와 PBR 명시 캡처도 바이트 단위로 일치했다.

## 2026-11-24 — 상태별 범례와 표면 시뮬레이션 에디터 이관

- **구현**은 상태 파일의 `displayName`과 `displayMax`, 씬 또는 프로파일의 선언 순서를 사용한다. 개별 히트맵과 그룹별 막대가 같은 GPU 상태 슬롯을 읽고, 자홍색부터 순서대로 만든 색을 범례와 공유한다. `Compare → Layer & Substrate`는 두 상태 그룹을 각각 한 뷰에 그린다.
- **이관**은 세션과 검증 코드를 `Source/Application/Editor/SurfaceSimulation`, 표시 코드를 `Source/Rendering/Surface/Simulation`과 대응하는 셰이더 경로로 옮겼다. 자동화 도구는 `--capture-surface`, `--reference-surface`, `--check-surface`로 실행한다.
- **문서**는 에셋 계약, 렌더링 소스 맵, 검증 링크와 Obsidian 에셋 배치를 갱신했다. 기존 캡처는 `Captures/Surface`에 이관했다.
- **검증**은 `run.ps1`에서 Snow on Canvas의 두 비교 뷰와 범례를 직접 확인했다. [에디터 캡처](../../../Build/SurfaceStateViewVerification/editor-layer-substrate.jpg), [GPU 비교 로그](../../../Build/SurfaceStateViewVerification/gpu.log), [두 뷰의 자동 캡처](../../../Build/SurfaceStateViewVerification/textile-layer-substrate.png)를 남겼다. GPU 검증과 텍스처 캡처 회귀 검사를 통과했다.

## 2026-11-24 — 데모의 기울기와 경계 확인

- 자동 기울기 최대각을 Brick Runoff 25°, Terraced Catchment 22°, Kiln Spillway 18°로 설정했다. 3초부터 8초까지 Y축을 따라 기울며, 벽돌의 기본 카메라 거리는 모서리가 화면 안에 들어오도록 기존의 1.2배로 조정했다.
- 세 장면의 CPU·GPU 시나리오 및 질량 수지 검사를 통과했다. [수평과 최대 기울기 비교](../05_Experiments/Artifacts/SurfaceSimulation/DemoTiltComparison.png)를 고정 카메라에서 캡처했다. 사용 중인 MDSS.exe가 잠겨 있어 카메라 수정은 같은 소스의 별도 이름 Debug 실행 파일에서 검증했다.
- [Surface Simulation 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md)에 열린 출구의 방향, 턱 높이, 유출 질량의 처리 방식을 기록했다.

## 2026-11-24 — 외곽 경계 기본값과 출구 표시

- 코드 스타일, C++ 주석 규칙, 문서 작업 절차와 Surface Simulation 계약을 읽었다.
- 씬의 `openOutlet`을 생략하면 `Closed`로 불러온다. Solver 설정에 `Open +X edge`, `Active boundary`, `Show outlet edge`를 추가했다. 계산 형상의 실제 출구에 대응하는 렌더 메시 테두리를 표시 스냅샷의 자세와 각 뷰의 카메라로 투영한다.
- [ADR-0001](../03_Decisions/00_Records/ADR-0001-Surface-Boundaries.md)에 선택 근거와 단면 그림을 넣고 [구현 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md#표면-물의-외곽-경계)을 갱신했다.
- C++ 포맷을 적용한 뒤 별도 이름으로 Debug를 빌드했다. 전체 Surface Simulation CPU·GPU 검사에서 시나리오 보고서 16개가 통과했다. `openOutlet`을 생략한 임시 씬으로 에디터를 열었다. 실제 벽돌 뷰에서 기울어진 +X 테두리와 라벨을 확인했고, `Closed`를 선택한 뒤 적용할 때 마커가 사라지는 것을 확인했다.

## 2026-11-24 — Snow on Canvas의 융해수와 천 외관

- 코드 스타일, C++·GLSL 규칙과 Surface Simulation 상태·그래픽 계약을 읽었다.
- 공유 표면 프로파일의 수면 합성 기준과 PBR 보조 조명을 렌더 필드에 연결했다. `SnowyCoat`에서 색과 반사 가시성을 조정하고, 직조·체크 무늬와 거칠기를 보존한 적갈색 텍스처를 두 천 씬에 사용했다.
- [표면 렌더 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md)과 [융해수 캡처·검증 기록](../05_Experiments/04_Surface-Simulation-Validation.md#눈에-가려진-얇은-융해수-표시)을 갱신했다.
- C++ 포맷, GLSL Region 검사, 별도 이름의 Debug 빌드, SPIR-V 검증 및 Surface Simulation CPU·GPU 시나리오 검사 16개를 통과했다. 천의 CPU·GPU 보고서와 수심 세 프레임은 전후 일치했다.

## 2026-11-24 — 데모의 자동 급수와 물 출처 표시

- Agent-Vault 코드 스타일, C++ 및 주석 기준, 문서 작업 절차와 Surface Simulation 계약을 확인했다.
- 에디터 도구 모음과 프레젠테이션에 물 출처를 표시한다. 자동 급수는 솔버가 물을 받는 저장 영역의 경계와 실제 제출 누계·목표량을 보여 준다. 초기 액체와 눈·얼음 용융 씬에는 해당 출처를 표시한다.
- [Surface Simulation 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md#자동-급수와-물-출처-표시)에 누계 기준, 표시 범위와 토글 경로를 기록했다.
- C++ 포맷, 별도 이름 Debug 빌드와 Surface Simulation CPU·GPU 비교를 통과했다. 벽돌을 재시작해 0.72초의 0.43 / 1.2 g과 밝은 입구, 완료 후 1.2 / 1.2 g과 옅은 입구를 확인했다. 프레젠테이션에 완료 문구와 입구가 표시되는 것도 확인했다.

## 2026-11-25 — 데모 외관과 변형 재생 정리

- 코드 스타일, C++·GLSL 규칙과 Surface Simulation 계약을 읽었다. 두 천 씬의 알베도를 아이보리·머스터드 체크로 교체하고, Kiln Spillway의 새 이름을 Heated Water Channel로 정했다. 금속은 은회색 브러시드 스틸로 표시하고, 연료 패드 영역에 탄화와 불꽃을 한정했다.
- 배포 씬 9개 계열에서 카메라·기울기·급수·가열 스케줄을 맞췄다. 젖은 나무 변형은 급수 애니메이션을 띄우기 전에 급수 면적에 2.4 g을 초기 배분한다. 이전 시간 입력은 [검증용 씬](../../../Scripts/Research/Fixtures/SceneTiming/README.md)에 보관했다.
- 액체 외관을 추가한 물성 구조체와 계산 버퍼의 충돌을 해결했다. 계산용 GPU 물성은 고정된 16바이트 구조로 명시하고 그래픽 속성은 렌더 필드에서 읽는다. [상태·형상·GPU 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md)과 [검증](../05_Experiments/04_Surface-Simulation-Validation.md#체크무늬-천-가열-수로와-계열별-재생)을 갱신했다.
- 공유 빌드의 MSVC PDB 경합으로 독립 Debug 빌드에서 검증했다. SPIR-V 검사, RGB·텍스처 캡처 검사와 PBR 캡처를 확인했다.
- 독립 빌드의 전체 Surface Simulation CPU·GPU 비교 25건이 통과했다. 캡처의 색 변경 후에도 새 수로의 흡수수와 연료 패드 밖 탄화율은 0이었다.

## 2026-11-25 — 공용 PBR 스트립과 재질 거칠기

- Agent-Vault 코드 스타일, C++·GLSL 및 포맷 절차와 Surface Simulation 상태 계약을 검토했다.
- 공용 환경 반사에 월드 방향으로 고정된 스트립을 추가했다. 물·얼음의 거칠기와 굴절률, 바닥의 마른·젖은 거칠기와 굴절률을 `.PhysicalMaterial`에서 읽는다. 에디터와 자동 캡처에서 스트립을 끌 수 있다. 반복적인 수면 반사 줄무늬를 줄이려고 PBR 잔물결 근사를 완만하게 조정했다.
- [표시 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md), [실험과 캡처](../05_Experiments/04_Surface-Simulation-Validation.md#공용-스트립-조명-비교)를 갱신했다.
- Debug 빌드, GLSL 빌드·검증, CPU·GPU 비교, 텍스처 캡처 회귀를 통과했다. 고정 프레임의 조명 ON/OFF와 재질 거칠기 변경에서 두께 뷰 RGB가 모두 일치했다. 실행 중인 기본 에디터의 파일 잠금을 피하려고 별도 이름의 Debug 에디터에서 토글을 직접 확인했다.

## 2026-11-25 — 바닥 재질 노멀과 광택

- 코드 스타일, C++·GLSL 및 포맷 절차와 Surface Simulation 계약을 검토했다.
- 바닥 재질의 선택적 노멀 맵을 선형 텍스처로 연결했다. 뷰별 토글, 캡처 옵션, 강도 편집을 추가했다. 바닥 PBR의 프레넬 반사와 공용 소프트박스가 캔버스와 블록 표면의 광택을 보여 주도록 조정했다.
- [렌더링 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md), [캡처 비교](../05_Experiments/04_Surface-Simulation-Validation.md#재질-노멀-맵과-바닥-광택-비교) 및 번들 노멀의 출처를 갱신했다.
- Debug 빌드, SPIR-V, 텍스처 캡처 회귀와 벽돌·천의 CPU/GPU 비교를 확인했다. 기본 두 씬에서 PBR 시각 차이와 물 두께 이미지의 픽셀 일치를 검증했다.

## 2026-11-25 — 재질의 정반사 배율

- Agent-Vault C++·GLSL, 주석, Region 포맷 규칙과 Surface Simulation 계약을 확인했다.
- 벽돌과 천의 물리 재질 외관에 specularScale을 추가했다. 에디터 Scene Setup의 Assets에 0~4 범위의 재질 입력을 두고, 바닥 굴절률에서 계산한 정면 반사율에 배율을 적용했다. 키를 생략하면 1.0으로 해석한다.
- [렌더링 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md)과 [고정 프레임 비교](../05_Experiments/04_Surface-Simulation-Validation.md#거칠기와-정반사-배율-비교)를 갱신했다.
- 별도 이름의 Debug 빌드, Surface Simulation SPIR-V 6개와 텍스처 캡처 회귀를 통과했다. 벽돌과 두 캔버스 씬에서 조건별 PBR 차이와 동일한 물 두께 픽셀을 확인했다. 이전 재질의 기본값과 상한 초과 입력도 캡처 실행에서 검증했다.

## 2026-11-25 — BrickCube 노멀 기반 용암 씬

- Agent-Vault C++·GLSL, 주석과 포맷 절차 및 Surface Simulation 계약을 확인했다.
- BrickCube 노멀 PNG를 높이로 복원해 용암의 저장·수송에 연결했다. 씬의 상면 알베도와 본체 표시 두께를 지정하고, 액체 재질·상태·표면 프로파일을 등록했다. 에디터의 자동 입구 및 수동 주입을 같은 액체 상태에 연결했다.
- 용암은 저장 질량에서 구한 수위를 기준으로 불투명하게 그린다. [씬과 셰이더 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md)과 [CPU·GPU 및 같은 프레임 비교](../05_Experiments/04_Surface-Simulation-Validation.md#brickcube-노멀로-복원한-상면의-용암-수송)를 갱신했다.
- 별도 이름의 Debug 빌드, SPIR-V, 기존 텍스처 캡처 회귀 및 정상·거의 평평한 바닥의 CPU/GPU 질량 비교를 확인했다. 캡처로 외관·두께·그리드를 확인했다.

## 2026-11-25 — 강철 접촉 냉각과 배포 씬 점검

- Agent-Vault C++·GLSL·평가 규칙과 Surface Simulation 계약을 검토했다.
- 가열 수로 두 씬의 눈 상태와 초기 얼음을 제거했다. 강철의 초기 온도, 열용량과 접촉 열전달을 씬 설정으로 연결했다. 물이 기화하며 빼앗은 열을 바닥 온도에 반영하고 이 온도를 PBR 및 상태 진단에서 읽는다.
- 배포 21개를 점검했다. 초원 두 씬에는 이미 계산 중 갱신되던 바닥 온도 상태를 추가했다. 빙하 두 씬의 4분할 카메라 구도를 조정했다. 활성 계산값과 씬의 상태 선언을 대조하는 검사도 추가했다.
- 별도 이름 Debug 빌드, 기존 캡처 회귀, 관련 SPIR-V와 CPU·GPU 계산 28개 보고서를 확인했다. 가열 수로의 무급수·상온 대조군 및 초원 온도 상태 누락 대조군을 실행했다. [계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md)과 [실험·캡처](../05_Experiments/04_Surface-Simulation-Validation.md#배포-씬-전수-점검)를 갱신했다.

## 2026-11-25 — Firebreak 불꽃·기화·탄화 외관

- Agent-Vault C++·GLSL과 주석·포맷 절차, 표면 상태 계약과 배출 효과 결정을 확인했다.
- 낮은 불꽃 분기를 제거하고 모든 Surface Simulation 불꽃에 Charring Wood의 체적 렌더링을 연결했다. 가열된 표면수·흡수수의 기화 질량을 초원의 물 장부와 공용 응결 렌더링에 전달했다. 탄화의 명암과 무광 대비를 초원에 적용했다.
- Debug 빌드, SPIR-V, 텍스처 캡처 회귀와 CPU/GPU 젖은·마른 비교를 확인했다. 나머지 계산 케이스 26개를 같은 시뮬레이션 구현에서 확인했다. [배출 효과 결정](../03_Decisions/06_Surface-Emissions.md)과 [검증·비교 캡처](../05_Experiments/04_Surface-Simulation-Validation.md#firebreak의-공통-불꽃기화탄화-외관)를 갱신했다.

## 2026-11-25 — 단일 GPU 솔버와 활성 영역 갱신

- Agent-Vault C++·GLSL 및 Surface Simulation 계약을 확인했다. 구형 Surface State GPU 솔버·셰이더, 에디터의 구형 씬·벤치마크 진입점과 구형 솔버에 의존하는 빌드 타깃을 제거했다. 공용 GPU 자원과 매핑 검증, 기존 성능·연구 자료는 유지했다.
- `LiquidRunoff`에 GPU 활성 작업 목록과 간접 디스패치를 적용했다. 마른 표면과 국소 주입에서 전체 갱신과 비교하고 건조 이웃으로의 전파를 확인했다. [수송 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md#01-수송과-연산-순서) · [성능 검토](../05_Experiments/02_Performance-Measurements.md#현재-surface-simulation-최적화-검토)
- 분리 MSVC Debug 빌드 및 SPIR-V 검증, RX 6700 XT에서 Surface Simulation GPU·캡처 회귀와 BrickRunoff 에디터 시작 검사를 통과했다. 공용 회귀는 SurfaceMapping의 기존 seam fixture 경고 검사에서 실패했다. 활성 갱신의 GPU 시간 개선율은 미측정이다.
