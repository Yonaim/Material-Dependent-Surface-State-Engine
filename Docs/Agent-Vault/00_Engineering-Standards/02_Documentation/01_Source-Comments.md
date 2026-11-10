# Source Comment Guidelines — C / C++ / GLSL / Python / Shell / JS / TS

> **좋은 이름과 함수 구조가 먼저, 주석은 그 구조만으로 전달되지 않는 정보를 보충한다.** 코드가 이미 설명하는 What을 반복하지 말고 비자명한 Why, 중요한 제약, 호출 계약과 실패·상태 경계를 남긴다.

## 00. 파일 최상단 설명 — 필수

**프로젝트가 직접 작성·관리하는 모든 소스코드 및 헤더 파일에는 파일의 역할을 설명하는 최상단 주석을 작성한다.** 파일명만 보고 역할이 자명하더라도 해당 파일이 책임지는 범위 또는 진입점을 **한국어로 간결하게** 설명한다. 이 규칙은 C/C++, GLSL, Python, Shell, JavaScript/TypeScript에 공통 적용한다.

- **C/C++:** 첫 부분에 Doxygen `@file` + `@brief`를 사용한다. `.c`, `.h`, `.cpp`, `.hpp` 등 프로젝트 소유 소스와 헤더에 적용한다.
- **GLSL:** 파일 시작에 일반 `//` 주석으로 파일명/주요 역할을 기록한다. `#version`은 여전히 **첫 전처리 지시문**이어야 한다. 파일 설명 주석을 `#version` 앞에 둘 수 있으나, 다른 토큰·지시문을 앞에 두지 않는다.
- **Python:** 파일 시작의 모듈 docstring(`"""..."""`)을 기본으로 하며, shebang·인코딩 선언이 있다면 그 뒤에 둔다. 모듈 docstring은 주석과 달리 Python이 인식하는 문서 문자열이다.
- **Shell:** shebang(`#!/usr/bin/env bash` 등)을 **반드시 첫 줄**에 둔 뒤 `#` 주석으로 파일 역할과 실행 전제조건을 적는다.
- **JS/TS:** 파일 시작에 `//` 또는 JSDoc 블록으로 모듈의 목적과 공개 범위를 설명한다. 자동 생성된 `use strict`/특수 지시문 등의 구문 조건은 보존한다.
- **예외:** 자동 생성 파일, 외부에서 가져온 제3자 코드, 의도적으로 해시/서명 등 원본 바이트를 보존해야 하는 파일, 문법상 맨 앞의 특수 표기가 필수인 파일에는 그대로 강제하지 않는다. 예외 이유를 프로젝트 규칙에 기록한다.

**예시 — C++**

```cpp
/**
 * @file DebugUI.h
 * @brief 디버그 UI의 생명주기, 화면 입력, 진단 기능의 공개 인터페이스를 정의한다.
 */

#pragma once
```

**예시 — GLSL**

```glsl
// SurfaceUpdate.comp — 표면 상태의 입력을 검사하고 다음 상태를 계산하는 Compute Shader.
#version 450
```

**예시 — Python / Shell**

```python
"""프로젝트 리소스의 유효성을 검사하고 빌드 작업을 조율한다."""

from pathlib import Path
```

```bash
#!/usr/bin/env bash
# 프로젝트 의존성을 검사하고 빌드 및 테스트 명령을 순서대로 실행한다.
```

파일 헤더에는 구현 세부사항·변경 이력·본문의 함수 목록을 장황하게 복제하지 않는다. 책임이 바뀌면 관련 코드 변경과 함께 헤더 설명을 갱신한다. API 개별 함수의 Doxygen 주석은 계약이 필요한 경우에만 추가한다.

## 01. 작성할 가치가 있는 주석

- **설계 이유(Why)**: 다른 구현보다 이 방식을 선택한 근거, 중요한 트레이드오프.
- **불변식 / 전제조건**: 호출 순서, 상태 제약, 범위, 단위, 메모리 정렬.
- **부작용 및 위험**: 소유권 이전, 다른 스레드와의 동기화, 데이터 변경, 되돌릴 수 없는 작업.
- **비자명한 알고리즘**: 수식의 출처, 경계조건의 이유, 성능에 관한 검증 근거.
- **호환성 제약**: 특정 컴파일러·운영체제·프로토콜과의 상호 운용 이유.

## 02. 피할 주석

```cpp
// Increment the index.
++Index;

// Check if ready.
if (bReady) Start();
```

코드와 동일한 설명이 반복되는 경우 우선 이름·함수 구조를 개선한다. 다음 예시처럼 **원인이나 계약**을 설명하면 유지 가치가 있다.

```cpp
// The device can still read the previous buffer until the fence signals.
WaitForFence(FrameFence);
ReleaseOldBuffer();
```

- 구현과 불일치하거나 이미 폐기된 주석은 수정·제거한다.
- 과거의 구현 과정을 나열하는 주석은 대신 Work Log 또는 ADR에 남긴다.
- 디버그용 주석 처리된 코드 블록은 장기 보관하지 않는다. Git 이력을 활용한다.

## 03. 언어별 표현

| 언어 | 일반 주석 | 주의사항 |
|---|---|---|
| C / C++ | `//`, `/* ... */` | API 문서화는 Doxygen과 역할 분리, 매크로/전처리 경계는 명확히 |
| GLSL | `//`, `/* ... */` | 바인딩·레이아웃 선언과 Host 대응 관계를 소스에 남김. Region은 `// #region`, C/C++용 `#pragma region`은 사용하지 않음 |
| Python | `#`, 필요 시 docstring | docstring은 공개 API 설명, `#region`은 일반 주석 |
| Shell | `#` | shebang은 최상단, `# shellcheck disable=...`에는 이유 병기, Region은 `#region` |
| JavaScript / TypeScript | `//`, `/* ... */`, JSDoc | 동작 계약과 타입 정보가 충돌하지 않게 유지, Region은 `// #region` |

## 04. 섹션 주석과 Region — 선택적 탐색 보조

긴 코드 파일에 여러 의미 있는 책임이 있으면 **3줄 섹션 주석과 `#region` 계열 접기 마커를 함께** 사용할 수 있다. C/C++에서는 `#pragma region`을 사용한다.

```cpp
// ============================================================
// 00. Main Process
// ============================================================
#pragma region 00_Main_Process
// 입력 처리부터 시뮬레이션과 렌더링까지 한 프레임의 실행 순서를 조율한다.

void UpdateFrame() {
    ProcessInput();
    UpdateSimulation();
    RenderFrame();
}

#pragma endregion
```

- 구분선은 `// ============================================================` 형식을 사용한다. 제목은 `00. Main Process`, Region 이름은 `00_Main_Process`처럼 번호와 의미를 일치시킨다.
- **Region 설명은 시작 마커 바로 다음 줄에 1줄 주석으로 둔다.** 해당 구획이 수행하는 책임·입출력·중요한 부작용 중 제목만으로 알기 어려운 사실을 적는다. Region을 접으면 설명도 함께 숨겨지는 방식이다.
- 최상위와 중첩 Region에 동일한 설명 배치 원칙을 적용한다. 제목을 그대로 한국어로 옮기는 공허한 설명은 억지로 쓰지 않으며, 1줄에 담기 어려울 정도로 서로 다른 책임이 있다면 분리를 검토한다.
- 섹션 번호는 파일별로 `00, 01, 02...`에서 시작한다. 함수마다 무조건 Region을 만들지 않는다.
- **중첩 섹션:** 3줄 구분 주석은 최상위에서만 사용한다. 내부 하위 구획은 `// 00.01. Input Handling`처럼 들여쓰기된 **한 줄 주석 + 중첩 Region**으로 표현하며, 기본적으로 2단계까지만 허용한다. 상세 판단 기준은 [섹션·Region 표준](../00_Core/01_Local-Numbering-and-Navigation.md)에 따른다.
- `#pragma region`/`#pragma endregion`은 **IDE와 컴파일러가 지원하는 프로젝트에서만** 사용한다. 미지원 환경에서는 섹션 주석만 남겨도 가독성을 유지할 수 있어야 한다.
- 적용 여부, 호환성 주의사항, 추가 예시는 [`Local Numbering & Code Navigation`](../00_Core/01_Local-Numbering-and-Navigation.md)을 정본으로 한다.

### 언어별 마커

- **GLSL / JavaScript / TypeScript:** `// #region 00_Main_Process` … `// #endregion`
- **Python / Shell:** `#region 00_Main_Process` … `#endregion`
- **C/C++:** `#pragma region 00_Main_Process` … `#pragma endregion` (비표준 pragma 호환성 확인)

자세한 정규 형식과 VS Code 지원 방식은 [`Local Numbering & Code Navigation`](../00_Core/01_Local-Numbering-and-Navigation.md)을 정본으로 한다. GLSL은 [`GLSL Engineering Profile`](../01_Languages/02_GLSL.md)을 따라 `#version` 이후에 섹션을 두고, 셰이더의 실제 `layout` 선언 및 CPU 대응의 필수 주석은 소스 파일에 유지한다. Host–GPU 시스템 전체 관계는 Architecture/Contracts에 남긴다.

## 05. TODO / FIXME / NOTE

- TODO는 **남은 일과 완료 조건**이 구체적일 때만 남긴다. 가능하면 관련 작업 ID나 이슈 링크를 참조한다.
- FIXME는 재현 가능한 결함·위험을 가리키도록 작성하고, 단순한 기술 부채는 작업 기록으로 관리한다.
- `NOTE`는 현재 유지해야 할 비자명한 제약이 있을 때만 쓴다.
- 댓글 형태의 TODO를 이미 완료했다고 기록하지 않는다.

```c
// TODO(PROJ-42): Reject truncated headers before parsing payload bytes.
```

## 06. Doxygen과 문서 동기화

- 공개 API의 인자·반환·오류·단위·스레드·수명 계약은 [`Doxygen`](00_Doxygen.md)에서 더 자세히 다룬다. C/C++ Doxygen 서술은 한국어로 작성한다.
- 알고리즘의 큰 흐름은 관련 Architecture에, 결정의 근거는 ADR에 남기고 필요하면 링크한다.
- **코드를 수정할 때 관련 주석의 유효성도 반드시 확인한다.** 변경된 계약과 모순되는 주석을 남기지 않는다.
- 주석만 고쳤더라도 아직 검증되지 않은 동작을 사실로 단정하지 않는다.

## 07. 빈 줄·한 줄 주석·Region의 역할

- **빈 줄:** 논리적으로 다른 명령 그룹을 나눈다. 한 그룹 안에서는 관련 명령을 가까이 배치한다.
- **한 줄 주석:** 추상화된 하위 단계의 이름이나 비자명한 제약을 설명한다. 단순한 대입·증감·조건을 그대로 번역하지 않는다.
- **Region:** 여러 의미 있는 그룹으로 구성된 큰 파일·함수·섹션의 **선택적 접기 경계**다. 모든 빈 줄이나 분기마다 추가하지 않는다.
- **최상위 섹션:** 기존 3줄 구분선을 사용한다. **내부 섹션:** 들여쓰기된 한 줄 제목 주석과 언어별 Region을 사용한다. 두 경우 모두 필요한 **설명 한 줄은 Region 시작 마커의 바로 아래**에 둔다.
- 접기 기능이 없는 IDE에서도 빈 줄과 주석만으로 단계와 흐름을 파악할 수 있어야 한다.

코드 밀도와 판단 기준은 [Code Style](../00_Core/00_Code-Style.md)의 *코드 밀도와 시각적 그룹화*를 정본으로 한다.
