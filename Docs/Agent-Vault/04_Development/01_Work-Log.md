# Work Log — 코드 변경 및 문서 동기화

## 2026-11-26 — Glacier Cut 얼음과 금속 받침

- Glacier Cut의 두 조건에 표층 얼음 배율 1.75, 접촉 열 감쇠 4.4초와 브러시드 스틸 받침을 적용했다. 얼음 아래의 직접 표시와 굴절은 공용 강철 음영을 사용한다.
- 상태·표시 계약과 소스 맵을 동기화했다. MSVC Debug, SPIR-V 두 개, 두 조건의 CPU·Vulkan 대조 및 PBR 캡처를 확인했다. [수치 비교](../05_Experiments/04_Surface-Simulation-Validation.md#얼음-보충과-금속-받침)

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

## 2026-11-26 — 씬과 에디터의 셀당 형상 표본

- Agent-Vault C++·주석·문서 규칙과 Surface Simulation 계약을 확인했다. 단일 씬과 비교 에디터의 해상도 선택을 셀당 1×1, 2×2, 4×4, 8×8 및 Custom으로 바꾸고 전체 형상 격자를 숫자로 표시했다.
- 씬에서 `geometrySamplesPerCell`은 셀당 비율을, `geometrySamples`는 직접 지정한 전체 격자를 설정한다. 기존 크기에 정확히 맞는 23개 씬을 비율로 전환했다. 나머지 21개에는 기존 전체 격자 설정을 유지했다. [씬·에디터 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md#00-경계와-단위)과 개인 볼트의 에셋 형식 문서를 갱신했다.
- MSVC Debug 빌드, 셀당 비율 유지·상한·직접 입력 검사, 프리셋·Custom 씬의 에디터 초기 프레임을 확인했다. 두 씬 형식으로 실행한 Terraced Catchment의 CPU 결과가 일치했고 새 형식의 CPU·GPU 비교와 배포 씬 전체 CPU 기준 회귀를 통과했다. 중복 키, 미지원 비율, 비정수 비율 및 형상 범위를 위반한 입력은 로더에서 거부했다.

## 2026-11-26 — ThinFilm 프로파일의 수송 바인딩 간소화

- 프로파일 로더와 에셋 12개에서 중복 모델 지정을 제거했다. 대표 액체의 상태 ID는 transport.state에서 읽는다. Agent-Vault 계약과 개인 볼트의 현재 에셋 예시를 갱신했다.
- clang-format·Region 처리와 MSVC Debug 빌드를 통과했다. Snow on Fabric, Ice Gate, Brick Runoff의 CPU·Vulkan 검사를 실행했다. [검증 로그](../../../Build/Transport-State-Only)

## 2026-11-26 — ThinFilm 실행 통합과 씬 검색 제거

- Code Style, C++·GLSL, 주석·Region과 Surface Simulation 계약을 확인했다. 12개 수송 프로파일을 공통 ThinFilm 스키마로 묶었다. CPU·GPU는 각 층의 경계 유량과 같은 셀의 영역·바닥 규칙을 한 흐름에서 실행한다. 외투의 셀 단위 눈·흡수수와 고정 스텝당 한 번의 건조를 보존했다.
- Scenes 패널의 텍스트 검색 위젯, 검색 상태와 문자열 필터를 제거했다. 그룹과 비교 조건 선택은 유지했다. Agent-Vault 계약·코드 맵·에디터 문서 및 개인 볼트의 모델·GPU 패스와 분류 그림을 맞췄다.
- clang-format·Region 처리, MSVC Debug, Vulkan 1.2 SPIR-V, 15개 씬 CPU/Vulkan 검사, Python 6건, 비교 씬 실행과 외투 캡처를 확인했다. [측정 결과](../05_Experiments/04_Surface-Simulation-Validation.md#thinfilm-실행-경로-통합)

## 2026-11-26 — 층별 수송 물성과 유효 바닥

- Code Style, C++·GLSL, 주석·Region 규칙과 Surface Simulation의 수송·상태 계약을 확인했다. 단일층도 공유 재질의 안식각과 항복응력을 읽는다. CPU는 같은 면 유량 함수, GPU는 같은 이동성 함수와 면 거리 계산을 두 경로에서 사용한다. 다층의 유효 바닥은 각 수송 서브스텝의 시작 질량과 층 순서로 계산한다.
- clang-format·Region 처리 후 MSVC Debug 빌드와 Vulkan 1.2 SPIR-V 검사를 통과했다. 단일·다층의 뉴턴 극한, 마찰·항복 조건, 정지한 아래층의 턱과 월류를 검증했다. 외투, 얼음 마개, 흙이 덮인 돌길과 계단형 수조의 CPU/Vulkan 검사 및 Python 기준 검사 6건을 통과했다. 수송 계약·소스 맵·개인 볼트의 Transport 및 결합 문서를 갱신했다. [검증 결과](../05_Experiments/04_Surface-Simulation-Validation.md#층별-이동성과-정지한-아래층의-턱)

## 투명 단면 하부의 외관

- GLSL과 Surface Simulation 표시 계약을 확인했다. 공용 눈금창의 불투명 바닥 단면을 반투명한 유리색으로 바꿨다. 바닥 높이는 얇은 선으로 표시한다. 표시 계약을 갱신했다.
- Region 서식과 셰이더 빌드를 적용하고 두 씬의 초기·완료 PBR을 캡처했다. 단면창을 쓰지 않는 Heated Water Channel의 PBR은 변경 전과 픽셀이 일치한다.

## 2026-11-26 — 공유 열발광과 왁스 경사로 표기

- Code Style·C++·주석 규칙과 Surface Simulation 계약을 확인했다. 발광 활성 조건을 강철 재질과 온도 상태 연결로 옮겼다. 건식 강판과 물 접촉 냉각이 같은 PBR 발광 곡선을 사용한다. 왁스의 가열·냉각부는 기존 chart albedo 슬롯에서 읽는 표식이다.
- 포맷·Region 처리와 별도 Debug 빌드 뒤, 강판 두 조건·가열 수로·왁스·흡수 시편의 CPU·Vulkan 대조를 통과했다. 가열 수로의 31프레임 PBR은 변경 전후 픽셀 단위로 같고, 왁스 상별 질량 시계열도 일치했다. 공용 렌더 계약, 텍스처 목록과 검증 기록을 갱신했다. [비교 기록](../05_Experiments/05_Surface-Physics-Comparison.md#가열냉각의-시각적-단서)

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

## 2026-11-25 — Fabric 젖음의 색과 반사

기본·주름 Fabric의 젖은 부분에서 바탕색과 거칠기를 더 낮추고, PBR 반사에 쓸 직조 법선의 영향도 젖음에 따라 줄였다. 표면 물의 표시용 광학 경로 배율을 6으로 낮춰 물막 아래의 섬유색이 드러나게 했다. 셰이더를 빌드하고 두 씬의 첫 프레임이 전후 같은 픽셀인지 확인했다. 360번째 프레임에서 젖은 영역의 어두운 색과 각도에 따른 밝은 반사를 비교했다. Textile CPU·GPU 검증의 질량 장부, 시간 계열, 최종 상태값은 전후 일치했다. 캡처의 Vulkan present 레이아웃 경고는 변경 전후 모두 기록됐다. [동일 프레임 PBR 비교](../05_Experiments/Artifacts/SurfaceSimulation/Fabric080_WetnessComparison.png)

## 2026-11-25 — ambientCG 재질과 HDR 환경 반사

- C++ 주석·포맷 규칙, 표면 표시 계약과 씬 코드 맵을 확인했다. 씬에서 반복 알베도·거칠기와 노멀을 지정하고 HDR 파노라마의 반사, 노출, 수평 회전을 설정할 수 있게 했다. 바닥 재질 세 종과 HDRI 세 종의 배포 경로 및 CC0 출처를 동기화했다. [표시 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md#03-재질-표시와-캡처) · [에셋 출처](../../../Assets/Textures/SurfaceSimulation/README.md)
- MSVC Debug, 표시 셰이더 SPIR-V 여섯 개, 실제 Vulkan 캡처, Brick Runoff CPU/GPU 검사와 텍스처 캡처 회귀를 통과했다. 두 쌍의 두께 뷰에서 픽셀 차이가 0이었다. [검증](../05_Experiments/04_Surface-Simulation-Validation.md#ambientcg-데모-재질과-hdr-환경-반사)

## 2026-11-25 — 물가 스무딩과 표시 수심 분리

- GLSL 작성·접기 규칙과 표면 표시 계약을 확인했다. 저장 수위로 수면 정점, 가시 경계, 두께 뷰를 계산하고, 주변 수심 보간은 바닥 젖음에만 적용한다. [표시 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md)과 [코드 맵](../02_Architecture/02_Source/03_Surface-Simulation.md)을 갱신했다.
- Debug 셰이더 빌드와 SPIR-V 검증을 통과했다. 다섯 Vulkan 스냅샷에서 스무딩 ON/OFF 수심과 젖은 영역을 비교했고, 세 씬의 PBR·두께 뷰를 캡처했다. 기본 CTest 실행 파일은 바닥 온도 상태를 읽는 구형 로더에서 실패했다. 현재 씬 형식을 지원하는 실행 파일로 전체 CPU 기준, Brick Runoff·Twin Pockets GPU 검사와 텍스처 캡처 회귀를 통과했다. [측정 결과](../05_Experiments/04_Surface-Simulation-Validation.md#물가-스무딩과-표시-수심-분리)

## 2026-11-25 — 공통 바닥 온도 상태

- 코드 작성·주석·포맷 규칙을 확인했다. 씬 로더가 `SubstrateTemperature`를 공통 등록하고, CPU·GPU가 물 반응 전에 셀의 바닥 온도를 갱신한다. Snow on Canvas의 용융 규칙을 이 상태에 연결했다. [상태 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md)과 [코드 맵](../02_Architecture/02_Source/03_Surface-Simulation.md)을 갱신했다.
- 표준 실행 파일을 사용 중인 에디터와 분리해 MSVC Debug와 계산 셰이더를 빌드했다. 배포 씬과 변형의 `--reference-surface` 및 `--check-surface`를 통과했다. CPU·GPU에서 물 반응 없이 선택한 셀의 온도 입력과 입력 종료를 확인했다. 최종 실행 파일의 Snow on Canvas 검사와 벽돌 바닥 상태 뷰 캡처를 확인했다. 로그와 캡처는 `Build/SubstrateTemperature`에 보관했다.

## 2026-11-25 — 마른 턱과 물가의 표시 조건 분리

- GLSL 작성 규칙과 표시 계약을 확인했다. 고인 수면과 물가의 연결 조건을 분리하고, 보간한 높이차의 0점에서 물가를 표시했다. [표시 계약](../02_Architecture/01_Contracts/08_Surface-Simulation.md)
- Debug 셰이더와 SPIR-V 네 개, 텍스처 캡처 회귀를 확인했다. Cavity와 두 포켓 초기·후반, 벽돌 용암의 PBR·두께를 고정 카메라에서 비교했다. [수로 윤곽과 마른 턱 검사](../05_Experiments/04_Surface-Simulation-Validation.md#마른-턱과-수로-물가의-표시-분리)

## 2026-11-24 — 벽돌 외곽의 물 줄무늬

- Surface Simulation 상태·렌더 계약과 GLSL, C++ 및 빌드 서식 규칙을 확인했다.
- 벽돌의 급수 사각형을 5 mm 테두리에서 1 mm 홈 입구로 옮겼다. 벽돌 검증에 양의 수심을 가진 테두리 표본 수를 추가했고, 최대 급수 수심의 기준 높이는 씬의 급수 위치에서 읽는다.
- Debug 빌드와 벽돌 CPU·GPU 검사, RGB 캡처 CTest가 통과했다. Smooth를 끈 6초 캡처를 대조하고 8초 수심에서 젖은 테두리 표본 108 → 0개를 확인했다. [검증 기록](../05_Experiments/04_Surface-Simulation-Validation.md#벽돌-테두리의-줄무늬)

## 2026-11-24 — 공통 계산 격자의 분리된 액체 홈과 형상 세트

- C++·GLSL·Python, Code Style, Surface Lab 상태·GPU 계약, 검증과 성능 기록 절차를 확인했다.
- 형상 전처리에서 계산 셀 내부의 홈과 접촉을 찾고, CPU 및 Vulkan 수송이 각 홈의 질량과 수위를 계산한다. 표시 경로는 형상 표본에 속한 홈의 수위를 읽는다. 부모 셀 단위 반응을 유지하고, 벽돌 전용 격자 절단을 제거했다.
- `Assets/Geometry` 바로 아래에 원본 OBJ, 노멀, 높이, 변위 맵을 세 형상별로 배치했다. Scene 탭과 CLI에서 교체한다. 빌드를 막던 장면 v2 버전 전달, JSON 타입 비교, 캐시 해시 문자열 인자를 바로잡았다.
- [상태 계약](../02_Architecture/01_Contracts/08_Surface-Lab.md), [코드 맵](../02_Architecture/02_Source/03_Surface-Lab.md), [검증과 캡처](../05_Experiments/04_Surface-Simulation-Validation.md), Obsidian의 Implementation Rationale과 실험 기록을 갱신했다.
- MSVC Debug 빌드와 SurfaceLabReference·SurfaceLabGpu·SurfaceLabCapture 세 테스트, SPIR-V 6개, 형상 3세트 × 입력 4종을 검증했다. 3 mm 고개와 6 mm 턱의 수평·기울임 전달을 확인하고, RX 6700 XT에서 두 형상의 GPU 타임스탬프를 수집했다.

## 2026-11-24 — Surface Lab 불꽃 표현

- **읽은 문서:** Agent Vault 진입 규칙, Document Map, C++/GLSL·포맷 규칙, Surface Lab 계약과 효과 결정.
- **구현:** 불꽃 카드를 제거했다. 표시 스냅샷에서 활성도를 보간해 난류가 상승하는 빛을 적분한다. 주변 바닥에는 깜박이는 불빛과 보간한 그을음을 표현한다.
- **문서·검증:** 상태 계약, 효과 Rationale, Agent Vault와 Obsidian의 검증을 갱신했다. Debug, SPIR-V 5개, 기준·수송 CTest 3개, 복합 수로 CPU/GPU, 최대 4뷰와 RGB 캡처를 확인했다.

> **모든 코드 변경 작업에서 작업 단위 최소 1건 기록 필수.** 상세 설계는 Architecture/ADR, 실제 테스트 결과는 Experiments에 기록한다.

| 날짜 | 작업 / 코드 범위 | 읽은 주요 문서 | 갱신 문서 또는 영향 없음 근거 | 검증 상태 |
|---|---|---|---|---|
| `YYYY-MM-DD` | `{{TASK_AND_CODE_PATHS}}` | `{{READ_DOCS}}` | `{{UPDATED_DOCS_OR_NO_IMPACT_REASON}}` | `{{COMMAND_AND_RESULT_OR_NOT_RUN}}` |

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
