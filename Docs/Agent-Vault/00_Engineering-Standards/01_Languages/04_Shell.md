# Shell Script Engineering Profile

## 00. 실행 환경

- 대상: `{{SHELL_DIALECT}}` (Bash / POSIX sh 중 **명시적으로 선택**)
- 실행 OS/환경: `{{SHELL_RUNTIME}}`; 검증 명령: `{{SHELL_TEST_COMMANDS}}`
- Bash 전용 확장을 `#!/bin/sh`에서 사용하지 않는다.

## 01. 읽기 쉬운 실행 흐름

- 스크립트 맨 위에서 목적·필요 도구·입력·출력·실행 위치·파괴적 효과를 확인할 수 있게 한다.
- 상위 함수나 `main`에서 `check_prerequisites → prepare → run → verify`처럼 작업 순서를 보여준다.
- 경로·인자 처리 등 세부 구현을 명확히 명명한 함수에 둔다. 단순 한 줄 명령을 무의미하게 감싸지 않는다.

## 02. 안전 규칙

- 문자열·경로·인자 확장을 적절히 quote한다 (`"$var"`, `"$@"`); 공백·와일드카드·개행에 주의한다.
- Bash 배열과 POSIX sh의 인자 모델을 혼동하지 않는다.
- `set -e`, `set -u`, `pipefail`의 지원 여부·예외 동작을 이해하고 적용한다. `set -e`가 모든 실패를 자동 처리한다고 가정하지 않는다.
- 임시 파일은 `mktemp` 등으로 안전하게 만들고 필요한 경우 `trap`으로 정리한다.
- 삭제·덮어쓰기·권한 변경·네트워크 설치는 입력 검증, 명확한 범위, 실패 복구를 고려한다.
- 가능하면 반복 실행 시 멱등성을 확보하고, 비밀정보·토큰을 로그에 노출하지 않는다.

## 03. 검증 및 문서

- Bash 사용 시 `bash -n`, `shellcheck`, `shfmt` 등의 적용을 고려한다. POSIX sh는 해당 셸의 문법 검사를 확인한다.
- 실행 전제조건·환경 변수·실패 코드는 관련 문서나 스크립트 주석에 기록한다.
- 자동화 절차를 변경하면 실행 Flow, Source Map, 검증 기록을 동기화한다.

## 주석 규칙

주석과 설명은 [`Source Comments`](../02_Documentation/01_Source-Comments.md)를 따른다. 코드와 주석을 함께 수정하며, 이미 이름으로 드러나는 동작을 반복하지 않는다.

## 섹션 Region 마커

큰 스크립트에서는 [공통 표기법](../00_Core/01_Local-Numbering-and-Navigation.md)의 `#region` / `#endregion`을 사용할 수 있다. Shell에서는 **실행에 영향을 주지 않는 주석**이고, VS Code에서는 Custom Folding 등 접기 지원을 확인해야 한다. **shebang을 파일 첫 줄**에 유지한다.

```bash
#!/usr/bin/env bash

# ============================================================
# 00. Main Process
# ============================================================
#region 00_Main_Process

main() {
    check_prerequisites
    run_pipeline
}

main "$@"

#endregion
```

프로젝트가 POSIX sh를 사용하는 경우 Bash 전용 기능을 넣지 않는다.
