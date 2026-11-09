# Local Numbering & Code Navigation

## 00. 지역 번호 방식

- **각 디렉터리에서 번호를 독립적으로 `00, 01, 02, ...`으로 시작한다.** 부모 숫자를 자식에 이어 붙이지 않는다.
- 같은 디렉터리 안에서도 **파일 그룹과 폴더 그룹의 번호는 각각 독립적**이다. VS Code `filesFirst` 정렬을 전제로 한다.
- 번호 = 해당 그룹 내의 **권장 탐색 순서**; 파일/폴더 이름 = **책임**; 폴더 계층 = **포함 관계**.
- 번호는 런타임 실행 순서·함수 호출 깊이·절대적인 추상화 레벨을 나타내지 않는다.
- 새 항목이 중간에 들어오면 해당 디렉터리의 해당 그룹에서 필요한 범위만 재번호화하고, 내부 링크와 외부 참조 경로를 갱신한다.
- 읽기 순서가 의미 없는 독립 모듈은 숫자 없는 이름을 사용할 수 있다.
- 언어·빌드·패키지 명명 제한과 충돌하면 번호를 적용하지 않는다. `#include`, Python import, Bash 소스 호출의 실제 경로를 깨뜨려서는 안 된다.

```text
repo/
├── 00_Main.md
├── 01_Map.md
├── 00_Flow/
│   ├── 00_Entry.md
│   ├── 01_Pipeline.md
│   └── 00_Details/
│       ├── 00_Stage-A.md
│       └── 01_Stage-B.md
└── 01_Contracts/
    └── 00_Interfaces.md
```

위 트리는 **같은 위치에서 파일 `00`과 폴더 `00`이 함께 존재할 수 있음**을 보여준다.

## 01. VS Code 정렬

볼트 내부의 [`.vscode/settings.json`](../../.vscode/settings.json):

```json
{ "explorer.sortOrder": "filesFirst" }
```

이 설정은 VS Code가 **볼트를 작업 영역 루트로 열었을 때** 적용된다. 저장소 루트로 연다면 저장소의 `.vscode/settings.json`에 같은 값을 병합한다.

## 02. 파일 내부 섹션과 접기 (C / C++)

책임이 다른 구역을 한 파일 안에서 탐색할 필요가 있을 때는 **섹션 주석 + `#pragma region` / `#pragma endregion`**을 함께 사용할 수 있다. 다음 형식을 표준 예시로 사용한다.

```cpp
// ============================================================
// 00. Main Process
// ============================================================
#pragma region 00_Main_Process

void UpdateFrame() {
    ProcessInput();
    UpdateSimulation();
    RenderFrame();
}

#pragma endregion


// ============================================================
// 01. Simulation Coordination
// ============================================================
#pragma region 01_Simulation_Coordination

void UpdateSimulation() {
    CollectActiveRegions();
    DispatchSimulation();
}

#pragma endregion
```

- 섹션 번호는 **각 파일에서** `00, 01, 02, ...`으로 시작한다. 주석의 제목과 region 이름은 동일한 의미를 표현한다.
- 주석은 편집기의 접기 기능이 없어도 시각적인 구분을 제공하고, `#pragma region`은 **지원하는 IDE**에서 접기 기능을 추가한다.
- **`#pragma region`은 ISO C/C++ 표준 지시문이 아니다.** 사용 전 프로젝트의 컴파일러/IDE 지원 여부를 확인한다. 미지원 컴파일러에서는 경고가 발생하거나, 경고를 오류로 처리하면 빌드가 실패할 수 있다.
- **지원하지 않는 빌드 환경**에서는 `#pragma region`/`#pragma endregion`을 생략하고 동일한 섹션 주석만 사용한다.
- 섹션은 책임 단위로만 만든다. 함수마다 무조건 생성하거나 과도하게 중첩하지 않는다.
- 섹션은 함수 분리·응집도·의존성 설계를 대신하지 않는다. 먼저 코드를 명확히 추상화한다.
- 구체적인 코드 주석 규칙은 [`Source Comments`](../02_Documentation/01_Source-Comments.md)를 따른다.

## 03. 언어 공통 Region 표기법 — `#region` / `#endregion`

여러 언어에서 섹션 제목의 가독성과 IDE의 코드 접기 기능을 함께 유지한다. **마커 이름은 `#region` / `#endregion`으로 통일**하되, 언어마다 유효한 주석 또는 전처리 문법으로 감싼다. C/C++에서만 `#pragma region`을 사용한다.

| 언어 | 시작 마커 | 끝 마커 | 접기 지원 기준 |
|---|---|---|---|
| C/C++ | `#pragma region 00_Main_Process` | `#pragma endregion` | VS Code 기본 지원. 비표준 pragma이므로 컴파일러 호환성 확인 |
| GLSL (`.vert`, `.frag`, `.comp`, `.glsl`) | `// #region 00_Main_Process` | `// #endregion` | 일반 GLSL 주석. VS Code에 Custom Folding 등 접기 공급자 설정 필요할 수 있음 |
| JavaScript / TypeScript | `// #region 00_Main_Process` | `// #endregion` | VS Code 기본 마커 지원 |
| Python | `#region 00_Main_Process` | `#endregion` | VS Code 기본 마커 지원. Python에서는 주석 |
| Bash / POSIX Shell | `#region 00_Main_Process` | `#endregion` | Shell에서는 주석. Custom Folding 등의 지원을 확인 |

### GLSL / JavaScript / TypeScript

```glsl
#version 450

// ============================================================
// 00. Main Process
// ============================================================
// #region 00_Main_Process

void main() {
    // ...
}

// #endregion
```

JavaScript/TypeScript에서는 `#version 450` 없이 동일한 `//` 형식의 주석을 사용한다.

### Python / Shell

```python
# ============================================================
# 00. Main Process
# ============================================================
#region 00_Main_Process

def main():
    load_config()
    execute_task()

#endregion
```

Shell 스크립트에서는 **shebang이 필요한 경우 첫 줄**에 `#!/usr/bin/env bash` 또는 `#!/bin/sh`를 두고, 이후에 동일한 `#region` 형식을 사용한다.

- 섹션 번호는 파일마다 `00, 01, 02, ...`으로 다시 시작하고 제목과 Region 이름을 일치시킨다.
- **세 줄 섹션 주석은 Region 접기를 지원하지 않는 에디터에서도 가독성을 유지**하기 위한 것이다. Region만으로 구조를 숨기지 않는다.
- 작은 파일에는 강제하지 않고, 책임별로 큰 단위를 접을 필요가 있을 때 사용한다. 꼭 필요한 컴파일러 지시문, `#version`·`#extension`·shebang 등은 언어 규칙에 따른다.
- GLSL에는 C/C++용 `#pragma region`을 넣지 않는다. `// #region`은 컴파일러에 영향을 주지 않는 일반 주석이다.
- 개별 편집기의 접기 동작은 확장과 언어 모드에 따라 다를 수 있다. [VS Code 설정](../../.vscode/settings.json)과 [README](../../README.md)를 참고한다.

## 04. 읽기 순서와 파일 정렬은 별개

번호는 사람이 편리하게 찾기 위한 힌트다. 항목 순서는 `00`, `01`, `02`처럼 1씩 증가시킨다. 중간 항목 삽입 시 필요한 범위에서 재번호화하고 참조 링크를 확인한다. 작업에 따른 **실제 읽기 순서**는 [`01_Document-Map.md`](../../01_Document-Map.md)에 기술하며, 파일 시스템 구조와 반드시 같을 필요는 없다.
