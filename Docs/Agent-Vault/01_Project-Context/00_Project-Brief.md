# Project Brief — MDSS Engine

## 프로젝트 목적

**MDSS Engine (Material-Dependent Surface State Propagation Engine)**은 표면의 재질·형상·상태 특성에 따라 물질 또는 효과가 어떻게 유입되고, 이동하고, 축적·감쇠·반응하는지를 시뮬레이션하고 시각화하는 엔진이다. 상태 변화가 형상과 외관에 반영되며, 변화된 형상은 이후 상태 전파에도 영향을 줄 수 있다.

## 해결하려는 문제

표면 위 현상을 단순한 색상 변화로만 표현하지 않고, **표면 위치별 상태량과 재질별 규칙을 바탕으로 일관되게 갱신**하고자 한다. 하나의 Scene에서도 Surface마다 서로 다른 Profile과 Geometry를 사용하며, 사용자 입력이나 Scene 이벤트가 GPU 시뮬레이션과 화면 출력까지 연결되어야 한다.

## 주요 기능과 범위

| 영역 | 책임 |
|---|---|
| Scene·Asset | Mesh, Surface Profile, Instance 및 Scene 이벤트를 로드하고 관리 |
| Surface Runtime | Simulation UV를 Texel로 매핑하고 공유 정적 Geometry 및 Instance별 상태를 준비 |
| Contact·Input | 사용자 조작·Scene 이벤트를 텍셀별 `InputDelta`로 변환 |
| GPU Simulation | Surface State의 Flux, Gather, Decay, Reaction 및 다음 상태 계산 |
| Geometry Feedback | 상태 축적에 따른 동적 높이·법선 등 전달 관련 형상 정보 갱신 |
| Rendering | 현재 상태를 Render State Texture·Overlay·Surface Shading으로 시각화 |
| Debug UI·Diagnostics | 실행 옵션, 상태 관찰, 입력 조작 및 프로파일링 제공 |

## 시스템의 기본 원칙

- **Surface별 재질 규칙:** `.SRProfile` 및 State Registry가 상태 채널과 물리·표현 파라미터를 정의한다.
- **공유 정적 형상과 개별 동적 상태 분리:** 동일 Geometry를 여러 Instance가 재사용할 수 있지만, 시간에 따른 State는 Instance별로 관리한다.
- **명시적인 GPU 실행 계약:** CPU와 GLSL은 Descriptor, Buffer Layout, Push Constant, Pass 순서 및 Synchronization 규칙을 공유한다.
- **Simulation과 표시용 데이터 분리:** Render State Texture나 Overlay의 표시용 가공이 Solver의 State를 임의로 바꾸지 않는다.

## 기술 구성 및 코드 위치

- **주요 언어:** C++20, GLSL (`.comp`, `.vert`, `.frag`, `.glsl`)
- **GPU 인터페이스:** Vulkan 및 SPIR-V Shader
- **빌드 체계:** CMake 기반 프로젝트; 실제 빌드 조건과 검증 결과는 관련 설정 및 테스트 문서를 따른다.
- **핵심 소스:** 프로젝트 루트의 `Source/`와 `Shaders/`
- **문서 위치:** `Docs/Agent-Vault/`. 문서에서 프로젝트 소스까지의 상대 경로는 `../../Source/`, `../../Shaders/`이다.

## 처음 읽을 문서

프로젝트 구성과 실행 경로는 [[02_Architecture/03_System-Map|System Map]], 상위 함수의 탐색 순서는 [[02_Architecture/04_Code-Reading-Guide|Code Reading Guide]]에서 시작한다. 구체적인 용어와 요구사항·제약은 [[01_Project-Context/01_Glossary|Glossary]], [[01_Project-Context/02_Requirements|Requirements]], [[01_Project-Context/03_Constraints|Constraints]]를 참조한다.
