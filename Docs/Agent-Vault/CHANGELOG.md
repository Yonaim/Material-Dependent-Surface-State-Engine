# Universal Agent Vault — Template Changelog

## v5.8

- VS Code Custom Folding의 GLSL·Shell Region 정규식을 **LF·CRLF 양쪽 줄바꿈**에 대응하도록 변경 (`foldStartRegex` / `foldEndRegex` / `defaultFoldStartRegex`).
- Custom Folding 설정 객체 안에서 기본 Region의 파일 열기 자동 접기를 활성화. 사용 중인 확장 환경에서 실제 동작 확인 필요.
- README와 섹션·Region 규칙에 **프로젝트 루트/하위 볼트/사용자 설정 적용 범위** 및 LF·CRLF 회귀 확인 절차를 추가.
- 문서·Obsidian·기존 코드 스타일 규칙은 유지. VS Code 확장의 실제 실행 동작은 템플릿 생성 환경에서 검증하지 않음.

## v5.7

- 모든 언어에 **의도적 공백 줄과 의미 단위별 그룹화** 원칙 추가. 길고 밀집된 검증·계산·상태 갱신은 읽기 쉬운 단위로 구분한다.
- 큰 함수·섹션에만 **선택적인 하위 Region** 적용, 모든 블록이나 조건에 Region을 추가하지 않도록 명확화.
- GLSL의 장문 조건·조기 반환·처리 단계에 대한 읽기 쉬운 서식 예시 및 동기화/계약 보존 주의사항 추가.
- 코드 리뷰·Document Map·AGENTS에 읽기 쉬운 공백·Region 판단 기준 연결.
- VS Code Custom Folding의 GLSL 접기 실패 원인은 사용자 환경에서 미확인 상태이므로 확장 동작을 수정했다고 주장하지 않음. v5.6 설정은 보존.

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
