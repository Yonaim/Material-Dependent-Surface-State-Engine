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

### 중첩 Region: 큰 섹션 내부의 하위 구획

**중첩은 허용하지만, 최상위 구획의 중요도를 유지한다.** 하나의 섹션에 서로 구분되는 하위 책임이 여러 개 있고, 접었을 때 탐색량이 실제로 줄어드는 경우에만 사용한다.

- **최상위:** 기존 **3줄 섹션 주석 + Region**을 유지한다.
- **하위:** **들여쓰기된 한 줄 섹션 주석 + Region**을 사용한다. 중첩 단계마다 3줄 구분선을 반복하지 않는다.
- **번호:** 하위 구획은 해당 상위 섹션의 번호에 지역 번호 `00, 01, 02...`를 이어 `00.00`, `00.01`로 표시한다. Region의 식별자는 `00_00_Camera_Controls`처럼 점을 밑줄로 바꾼다.
- **깊이:** 일반적으로 **최상위 1단계 + 하위 1단계(총 2단계)**까지만 사용한다. 세 번째 단계가 필요하면 함수·타입·파일 분리를 먼저 검토한다.
- **들여쓰기:** 하위 주석과 마커는 둘러싼 코드의 논리적 들여쓰기를 따른다. 전처리기를 지원하지 않는 환경에서는 동일한 구조의 일반 주석만 유지한다.

```cpp
// ============================================================
// 00. Scene & Viewport
// ============================================================
#pragma region 00_Scene_And_Viewport

    // 00.00. Camera Controls
    #pragma region 00_00_Camera_Controls

    void ProcessCameraInput(TScene& Scene);
    void DrawCameraWindow(TScene& Scene);
    void ApplyCameraViewPreset(TScene& Scene);

    #pragma endregion

    // 00.01. Gizmo Controls
    #pragma region 00_01_Gizmo_Controls

    void ProcessSelectionAndGizmo(TScene& Scene);
    void DrawSelectedObjectWindow(TScene& Scene);

    #pragma endregion

#pragma endregion
```

위 예시는 **클래스 내부 선언 그룹의 일부를 보여주는 개념 예시**다. 실제 C++ 클래스에서는 `public:` / `private:` 접근 범위와 선언 순서를 유지해야 한다.

**함수 내부에서도 문법상 중첩할 수 있지만, 긴 함수를 Region으로만 숨기지 않는다.** 먼저 의미 있는 하위 함수 추출이 가능한지 확인하고, 공유 지역 변수의 생명주기·제어 흐름·성능 제약 등으로 추출하는 편이 오히려 불리할 때만 제한적으로 사용한다. `if`/`switch`/루프를 통과하는 블록을 임의로 나누거나 제어 흐름을 가리는 구획은 만들지 않는다.

다른 언어에서도 동일한 원칙을 적용하되, `#pragma region` 대신 각각의 **언어별 `// #region` 또는 `#region`**을 사용한다. 중첩 접기 지원은 IDE·언어 확장에 따라 다를 수 있으므로 실제로 확인한다.

**중첩 적용 판단:** (1) 상위 구획 안에 적어도 두 개의 의미 있는 하위 책임이 있고, (2) 각각 독립적으로 접고 펼칠 가치가 있으며, (3) 함수·타입 분리보다 탐색 보조가 적절할 때만 사용한다. 파일 길이 또는 줄 수만으로 자동 생성하지 않는다.


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

### VS Code Custom Folding — LF / CRLF 호환

GLSL(`glsl`)·Shell(`shellscript`)의 `#region` 주석을 인식하는 정규식은 **LF와 CRLF 모두를 처리**해야 한다. Custom Folding이 줄바꿈을 `\n` 단위로 나눠 검사하는 환경에서는 CRLF 파일의 각 줄 끝에 `\r`이 남을 수 있기 때문이다.

- 정규식에서 들여쓰기는 `[ \t]*`, 줄 끝은 `\r?$`로 표현한다. `[^\r\n]*`는 Region 이름에 개행 문자를 포함하지 않게 한다.
- `foldStartRegex`, `foldEndRegex`뿐 아니라 `defaultFoldStartRegex`에도 동일한 개행 대응을 적용한다. `collapseDefaultRegionsOnOpen`은 `maptz.regionfolder` **객체 안에** 설정한다. `maptz.regionfolder.collapseDefaultRegionsOnOpen`이라는 독립된 설정 키를 만들지 않는다.
- 실제 사용 값은 [`.vscode/settings.json`](../../.vscode/settings.json)을 단일 출처로 삼는다. 이 문서에 정규식을 복제하지 않는다.
- **볼트가 하위 디렉터리라면:** VS Code가 프로젝트 루트를 열었을 때 하위 볼트의 `.vscode/settings.json`이 자동으로 적용되지 않는다. 프로젝트 루트 설정에 **기존 설정을 보존하면서 병합**하거나 사용자 설정을 사용한다.
- 변경 후 GLSL 파일을 닫았다 다시 열거나 창을 다시 로드하여 수동 접기(`Region folding: Collapse all #regions`)를 먼저 확인한다. 이후 자동 접기를 확인한다. 설치된 확장 버전·언어 모드·설정 충돌 때문에 실제 동작이 달라질 수 있다.
- 회귀 검증은 동일한 마커를 가진 **LF 파일과 CRLF 파일** 모두에서 시작/종료 마커 인식 및 중첩 구간 접기를 확인한다. 설정 검사만으로 에디터에서 실제 접힘을 보장한다고 기록하지 않는다.

## 04. 읽기 순서와 파일 정렬은 별개

번호는 사람이 편리하게 찾기 위한 힌트다. 항목 순서는 `00`, `01`, `02`처럼 1씩 증가시킨다. 중간 항목 삽입 시 필요한 범위에서 재번호화하고 참조 링크를 확인한다. 작업에 따른 **실제 읽기 순서**는 [`01_Document-Map.md`](../../01_Document-Map.md)에 기술하며, 파일 시스템 구조와 반드시 같을 필요는 없다.

## 05. 큰 클래스/파일에서의 적용 범위

섹션은 **위치를 찾고 접는 도구**다. 큰 클래스를 다룰 때는 먼저 공개 API → 기능별 메서드 → 상태를 탐색 가능하게 정리한다. 그다음 응집된 상태·동작을 추출할 필요가 있는지 별도 검토한다. 자세한 2단계 절차와 C++ `TDebugUI` 예시는 [`Section & Responsibility Refactoring`](03_Section-and-Responsibility-Refactoring.md)을 따른다.

특히 C++ 멤버 필드의 재배치는 초기화 순서·레이아웃에 영향을 줄 수 있으므로 **1단계가 곧 안전한 선언 재배치를 뜻하지 않는다.**

## MDSS 프로젝트의 MSVC Region 호환성

2026-11-10 실제 재빌드에서 숫자로 시작하는 `08_...`·`09_...` C++ Region 라벨이 C2041로 거부됐다. 해당 파일과 새 하위 구획의 라벨은 `#pragma region "09_Profile_Editing_State"`처럼 따옴표로 감싼다. 번호/책임과 코드의 선언 순서는 유지한다. GLSL 주석 마커에는 이 C++ 예외를 적용하지 않는다. 상세 범위는 [v5.8 이관 기록](../../04_Development/13_Vault-5.8-Readability-Coverage.md)을 따른다.
