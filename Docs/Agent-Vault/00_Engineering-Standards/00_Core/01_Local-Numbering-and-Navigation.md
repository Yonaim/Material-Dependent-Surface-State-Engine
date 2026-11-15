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

### 새 디렉터리 이름 — 대문자 시작

- 프로젝트에서 직접 만드는 디렉터리는 기본적으로 **대문자로 시작**한다. 예: `Tools/`, `Source/`, `Shaders/`, `Docs/`, `Tools/Formatting/`.
- 번호가 있는 폴더는 번호 뒤의 의미 단어를 대문자로 시작한다. 예: `00_Core/`, `10_Contracts/`.
- 플랫폼·프레임워크·빌드 도구가 고정한 이름(`.git`, `.vscode`, `.obsidian`, 규정된 `src`/`include` 등), 외부 라이브러리 경로 및 기존 호환성이 필요한 경로는 예외다. **이미 존재하는 경로를 일괄 변경하지 않는다.**
- 새 도구·스크립트는 원칙적으로 별도 `Scripts/`를 만들지 않고 `Tools/` 내부에서 책임별로 구성한다.

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

## 03. 읽기 순서와 파일 정렬은 별개

번호는 사람이 편리하게 찾기 위한 힌트다. 항목 순서는 `00`, `01`, `02`처럼 1씩 증가시킨다. 중간 항목 삽입 시 필요한 범위에서 재번호화하고 참조 링크를 확인한다. 작업에 따른 **실제 읽기 순서**는 [`01_Document-Map.md`](../../01_Document-Map.md)에 기술하며, 파일 시스템 구조와 반드시 같을 필요는 없다.
