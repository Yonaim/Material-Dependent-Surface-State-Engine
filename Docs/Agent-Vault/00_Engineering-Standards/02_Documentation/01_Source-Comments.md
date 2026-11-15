# Source Comment Guidelines — C / C++ / Python / Shell

> **좋은 이름과 함수 구조가 먼저, 주석은 그 구조만으로 전달되지 않는 정보를 보충한다.** 코드가 이미 설명하는 What을 반복하지 말고 비자명한 Why, 중요한 제약, 호출 계약과 실패·상태 경계를 남긴다.

## 00. 작성할 가치가 있는 주석

- **설계 이유(Why)**: 다른 구현보다 이 방식을 선택한 근거, 중요한 트레이드오프.
- **불변식 / 전제조건**: 호출 순서, 상태 제약, 범위, 단위, 메모리 정렬.
- **부작용 및 위험**: 소유권 이전, 다른 스레드와의 동기화, 데이터 변경, 되돌릴 수 없는 작업.
- **비자명한 알고리즘**: 수식의 출처, 경계조건의 이유, 성능에 관한 검증 근거.
- **호환성 제약**: 특정 컴파일러·운영체제·프로토콜과의 상호 운용 이유.

## 01. 피할 주석

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

## 02. 언어별 표현

| 언어 | 일반 주석 | 주의사항 |
|---|---|---|
| C / C++ | `//`, `/* ... */` | API 문서화는 Doxygen과 역할 분리, 매크로/전처리 경계는 명확히 |
| Python | `#`, 필요 시 docstring | docstring은 런타임 공개 인터페이스 설명에 사용, 단순 인라인 주석과 구분 |
| Shell | `#` | shebang(`#!/usr/bin/env bash` 등)은 실행 선언, `# shellcheck disable=...`에는 이유 병기 |

## 03. 섹션 주석과 Region — 선택적 탐색 보조

큰 C/C++ 파일에서 여러 의미 있는 책임을 구분할 필요가 있으면 **섹션 주석과 `#pragma region`을 함께** 사용한다.

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
```

- 구분선은 `// ============================================================` 형식을 사용한다. 제목은 `00. Main Process`, Region 이름은 `00_Main_Process`처럼 번호와 의미를 일치시킨다.
- 섹션 번호는 파일별로 `00, 01, 02...`에서 시작한다. 함수마다 무조건 Region을 만들지 않는다.
- `#pragma region`/`#pragma endregion`은 **IDE와 컴파일러가 지원하는 프로젝트에서만** 사용한다. 미지원 환경에서는 섹션 주석만 남겨도 가독성을 유지할 수 있어야 한다.
- 적용 여부, 호환성 주의사항, 추가 예시는 [`Local Numbering & Code Navigation`](../00_Core/01_Local-Numbering-and-Navigation.md)을 정본으로 한다.

## 04. TODO / FIXME / NOTE

- TODO는 **남은 일과 완료 조건**이 구체적일 때만 남긴다. 가능하면 관련 작업 ID나 이슈 링크를 참조한다.
- FIXME는 재현 가능한 결함·위험을 가리키도록 작성하고, 단순한 기술 부채는 작업 기록으로 관리한다.
- `NOTE`는 현재 유지해야 할 비자명한 제약이 있을 때만 쓴다.
- 댓글 형태의 TODO를 이미 완료했다고 기록하지 않는다.

```c
// TODO(PROJ-42): Reject truncated headers before parsing payload bytes.
```

## 05. Doxygen과 문서 동기화

- 공개 API의 인자·반환·오류·단위·스레드·수명 계약은 [`Doxygen`](00_Doxygen.md)에서 더 자세히 다룬다. C/C++ Doxygen 서술은 한국어로 작성한다.
- 알고리즘의 큰 흐름은 관련 Architecture에, 결정의 근거는 ADR에 남기고 필요하면 링크한다.
- **코드를 수정할 때 관련 주석의 유효성도 반드시 확인한다.** 변경된 계약과 모순되는 주석을 남기지 않는다.
- 주석만 고쳤더라도 아직 검증되지 않은 동작을 사실로 단정하지 않는다.
