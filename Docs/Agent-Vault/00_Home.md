# MDSS Engine — Agent Vault Home

> **목적:** C++20/Vulkan/GLSL 기반 MDSS Engine을 안전하게 탐색·수정·검증하기 위한 작업 문맥. 개인 연구 아이디어와 Related Work는 이 볼트의 실행 지시가 아니다.
>
> **현재 체크아웃 대조:** 2026-11-16에 루트 빌드·소스 경로와 주요 프레임/Solver 진입 경로를 확인했다. 전체 빌드·테스트를 이 문서 갱신 과정에서 실행하지 않았으며, 검증 결과는 날짜가 붙은 Validation 기록을 기준으로 판단한다.

## 프로젝트 프로필

| 항목 | 값 |
|---|---|
| 프로젝트명 | Material-Dependent Surface State Propagation Engine (MDSS Engine / MDSSP) |
| 목적 | 표면별 재질·형상에 따라 입력, 수송, 감쇠, 반응 및 적층을 GPU에서 처리하고 실시간 시각화 |
| 범위 | Static Mesh 기반 UV Texel Graph, 공통 Solver, Profile v5 Reaction, Geometry Feedback, Display Overlay |
| 언어 | C++20, GLSL; Python 및 PowerShell/Bash 보조 도구 |
| 그래픽스 / 빌드 | Vulkan, SPIR-V / 루트 CMake (`CMakeLists.txt` 최소 3.24; Visual Studio 프리셋은 CMake 4.2+) |
| 소스 위치 | 프로젝트 루트 `Source/`, `Shaders/`; 빌드·성능·연구 도구는 `Scripts/`, `CMake/`, `Tools/` |
| 문서 배치 기준 | `Docs/Agent-Vault/` (`01_Project-Context`에 기록된 경로) |
| 주요 빌드 진입점 | `Scripts/build.ps1`, `Scripts/build.sh`; CMake configure/build preset은 `CMakePresets.json` 참조 |
| 현재 검증 기준 | 최근 소스 변경의 실행 검증은 Work Log와 Validation 기록을 확인한다. 이번 대조에서 빌드·테스트는 실행하지 않음 |

## 연구 참고

- [[01_Related-Work|Related Work / 선행 연구]] — 문헌 검토와 열람 수준을 기록한 참고 문서. 현재 구현이나 신규성 주장을 대신하지 않는다.

## 시작 순서

1. [[AGENTS|Agent Entry]] → [[01_Document-Map|Document Routing Map]]; **전체 볼트를 한꺼번에 읽지 않는다.**
2. [[01_Project-Context/00_Project-Brief|Project Brief]]와 해당 [[01_Project-Context/02_Requirements|Requirements]], [[01_Project-Context/03_Constraints|Constraints]] 확인.
3. [[02_Architecture/04_System-Map|System Map]] → [[02_Architecture/00_Flows/00_Main-Flow|Main Flow]] → [[02_Architecture/02_Source/00_Source-Map|Source Map]].
4. 변경 전에는 관련 [[02_Architecture/01_Contracts/00_Contract-Register|Contract]] 및 [[03_Decisions/00_ADR-Index|Decisions]] 확인.
5. [[04_Development/00_Current-Work|현재 승인된 작업]], [[05_Experiments/01_Validation-and-Tests|검증]]과 실제 소스·테스트를 대조한다.

## 폴더 책임

- `00_Engineering-Standards/`: 범용 개발 규칙·Setup·Assets. **범용 템플릿의 구조를 유지.**
- `01_Project-Context/`: 제공된 프로젝트 Brief/Glossary/Requirements/Constraints.
- `02_Architecture/`: Flows, Contracts, Source, System Map, Code Reading Guide.
- `03_Decisions/`: 현재 결정 요약과 정식 ADR 색인·기록.
- `04_Development/`: 승인된 작업과 Work Log; 미채택 연구 제안은 개인 볼트에서 관리.
- `05_Experiments/`: 테스트·성능 인덱스 및 필요 시 참조할 과거 상세 기록.

## 근거 충돌 시

코드의 관찰 사실과 명시적인 요구사항·계약을 구별하고, 확인 안 된 사항은 TODO로 남긴다. 보관된 연구 성과나 이전 실험 수치를 현재 코드의 검증 결과로 확대하지 않는다.
