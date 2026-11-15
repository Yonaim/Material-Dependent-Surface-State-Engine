## v6.1

- 새 프로젝트와 기존 프로젝트를 구분하는 Setup 절차, **조건부 질문 목록**, 빌드 시스템 선택 기준 추가.
- `Assets/Build`에 소규모 C용 Make, 일반 C++용 CMake + 선택적 Presets, UnrealBuildTool 타깃 연동 원본 추가.
- Make/CMake/Unreal별 실제 컴파일 이전 `clang-format → indent_regions.py` 연결과 IDE/CLI/CI 검증·예외 사항 명시.
- 프로젝트 기존 빌드 파일·엔진 Target.cs 무단 교체 금지, `Project Brief`에 Setup 이력 미기재 원칙 재확인.
- 기존 포맷 도구 및 Obsidian/VS Code 설정은 변경하지 않음.

## v6.0

- Setup 에이전트가 `Assets/Formatting/`의 원본 3개를 프로젝트 루트 `.clang-format` 및 `Tools/`로 배치하는 절차 추가.
- 모든 빌드 전에 C/C++ `clang-format → indent_regions.py`를 실행하는 빌드 시스템 연결 및 실패 차단 규칙 추가.
- `format_before_build.py`와 빌드 훅 예시, 별도 검증 체크리스트 제공. VS Code Run On Save는 보조 기능으로 구분.
- 신규 프로젝트 디렉터리명은 대문자로 시작하도록 규정하고 스크립트는 `Tools/`에 통합.
- 기존 압축 파일에 있던 명시적 빈 디렉터리 12개를 제외. 기존 문서·설정·Git 메타데이터 등 비어 있지 않은 내용은 보존.

## v5.5

- 언어 공통 **큰 클래스·파일의 2단계 리팩터링** 규칙 추가: (1) 섹션 기반 구조화, (2) 상태·책임 기반 타입/모듈 추출.
- `TDebugUI` 클래스의 역할별 선언과 `TProfilingUIState` 추출 예시를 **설명용 코드**로 정리.
- C++ 멤버 선언 순서에 따른 초기화·레이아웃 위험과 단계별 검증 기준을 명시.
- C/C++·GLSL·Python·Shell·JS/TS 언어 규칙, 라우팅 맵, 에이전트 워크플로, 검증 체크리스트, 구현 템플릿을 연결.
- README의 **VS Code 섹션 접기** 설명을 간결한 합의 문구로 교체하고 과거 버전 번호를 제목에서 제거.
- 이전 Blue Topaz/Obsidian 및 VS Code 설정 유지.

## v5.4

- 공통 섹션 표기법 `#region`/`#endregion` 통일 (C/C++은 `#pragma region` 유지).
- 세 줄 번호 섹션 주석을 C/C++, GLSL, Python, Shell, JS/TS 예시에 적용.
- GLSL은 `// #region`, Python·Shell은 `#region`, JS/TS는 `// #region` 사용.
- 선택형 JavaScript/TypeScript 프로필 문서 추가.
- `.vscode/settings.json`에 GLSL·Shell Custom Folding 설정 추가, `.vscode/extensions.json`에 추천 확장 등록.
- Fold on Open은 기존 환경과의 연동 확인이 필요하므로 자동 접기를 기본값으로 강제하지 않음.
- Blue Topaz, Obsidian 자동 설치, filesFirst, 문서 동기화·ADR 규칙은 v5.3에서 유지.

## v5.3

- Blue Topaz 상세 설정 및 Obsidian 최소 플러그인 자동 설치 구성.
