# 단계적 구조화 및 책임 분리 (Section & Responsibility Refactoring)

> 적용 대상: **C, C++, GLSL, Python, Shell, JavaScript, TypeScript**의 규모가 큰 파일·클래스·모듈. 두 단계는 별도로 수행하고 검증한다. 모든 코드에 섹션이나 추출을 강제하지 않는다.

## 00. 목적과 적용 판단

함수·변수·상태가 한 파일 또는 클래스에 많이 모이면 **상위 인터페이스만 보고 무엇을 하는지 파악하기 어렵고**, 관련 코드를 찾기 위한 탐색 비용이 커진다. 이때 아래 순서로 개선한다.

1. **1단계 — 섹션 기반 구조화:** 내용의 의미와 역할에 맞춰 시각적·탐색상 경계를 만든다. 가능한 한 로직과 선언 순서를 변경하지 않는다.
2. **2단계 — 책임 기반 분리:** 실제 응집된 상태·동작 단위를 구조체, 클래스, 모듈, 함수 등으로 추출한다. 이는 코드의 참조·의존성·수명에 영향을 줄 수 있는 **별도 리팩터링**이다.

적용 신호: 하나의 클래스를 읽을 때 필드 그룹을 반복해서 넘겨야 함, 서로 무관한 UI/로직/로그 등이 섞임, 특정 상태를 변경하는 함수가 명확하게 한데 모임, 상위 흐름을 찾기 어려움. 반대로 파일이 작거나 책임이 단일하고 이미 읽기 쉬우면 분리하지 않는다.

## 01. 1단계 — 섹션으로 읽기 경로 만들기

### 기본 구조

- 클래스·모듈의 **외부에 노출되는 진입점/API → 내부 처리 흐름 → 기능별 세부 작업 → 데이터·상태** 순으로 읽히게 한다. 언어 특성과 의존 관계가 먼저다.
- 하나의 섹션은 **서로 관련된 책임**을 묶는다. 공개 API, 프레임 조율, Viewport, Simulation, Profiling 등 의미 있는 이름을 사용한다.
- [섹션·Region 표준](01_Local-Numbering-and-Navigation.md)의 **3줄 섹션 주석 + 언어별 Region 마커**를 사용한다. 파일마다 지역 번호 `00, 01, 02...`로 시작한다.
- 섹션 수를 미리 고정하지 않는다. 작은 파일, 함수 하나, 단순 선언만 있는 파일에는 무리하게 추가하지 않는다.
- 이 단계의 주목적은 **탐색 개선**이지 책임 수 감소가 아니다. 변경 내용을 섹션·주석 중심으로 제한한다.

### 예시: 많은 역할을 가진 `TDebugUI` 헤더

다음은 큰 디버그 UI 클래스의 **관련 선언 일부만 추린 구조 예시**다. 완성된 컴파일 가능 코드가 아니며 멤버·함수를 생략했다. 원본 구현을 옮기는 패치가 아니다.

```cpp
class TDebugUI
{
public:
    // ============================================================
    // 00. Lifecycle & Public API
    // ============================================================
#pragma region 00_Lifecycle_And_Public_API

    void BeginFrame(TScene& SceneData);
    void Render(VkCommandBuffer CommandBuffer) const;
    void RestartScene(TScene& SceneData);
    [[nodiscard]] bool IsSimulationPaused() const noexcept;

#pragma endregion

private:
    // ============================================================
    // 01. Scene & Viewport Operations
    // ============================================================
#pragma region 01_Scene_And_Viewport_Operations

    void ProcessCameraInput(TScene& SceneData);
    void ProcessSelectionAndGizmo(TScene& SceneData);
    void DrawViewportPanels(TScene& SceneData);

#pragma endregion

    // ============================================================
    // 02. Simulation & Diagnostics UI
    // ============================================================
#pragma region 02_Simulation_And_Diagnostics_UI

    void DrawSimulationDebugWindow(TScene& SceneData);
    void DrawSolverTab();
    void DrawPerformanceTab(bool bRendering);
    void DrawLogWindow();

#pragma endregion

    // ============================================================
    // 03. Runtime & Profiling State
    // ============================================================
#pragma region 03_Runtime_And_Profiling_State

    bool bSimulationPaused = false;
    double ProfilingElapsedSeconds = 0.0;
    double ProfilingFpsSum = 0.0;
    std::array<double, 40> ProfilingMetricSums{};

#pragma endregion
};
```

**C++ 주의:** 섹션을 만든다는 이유만으로 멤버 **변수의 선언 순서를 무작정 이동하지 않는다.** 선언 순서는 생성자 초기화 순서에 영향을 주고, 표준 레이아웃·ABI·리플렉션·직렬화·`offsetof` 등을 사용한다면 배치 변경도 문제를 일으킬 수 있다. 기존 메서드 선언의 접근 지정자(`public`/`private`/`protected`)와 전처리 조건도 보존한다. 1단계에서 실제 재배치가 필요하면 영향 검토와 검증을 별도 수행한다.

## 02. 2단계 — 응집된 책임을 실제 타입·모듈로 추출

**1단계와 독립된 변경**으로 다룬다. 관련 상태·동작이 한 단위로 함께 변경되는지 확인하고, 아래와 같이 목적을 드러내는 구조로 추출한다.

```cpp
// 개념 예시 — 기존 멤버와 참조를 실제로 이동할 때는 별도의 검증이 필요하다.
struct TProfilingUIState
{
    double ElapsedSeconds = 0.0;
    double FpsSum = 0.0;
    std::array<double, 40> MetricSums{};
};

class TDebugUI
{
    // ... 공개 API와 기능별 처리 함수
private:
    // ============================================================
    // 00. Diagnostic State
    // ============================================================
#pragma region 00_Diagnostic_State

    TProfilingUIState Profiling;

#pragma endregion
};
```

예를 들어 `ProfilingElapsedSeconds`를 `Profiling.ElapsedSeconds`로 옮기면 **모든 사용처와 초기화·리셋·복사·테스트를 수정**해야 한다. 구조체 추출 자체가 유용한 경우에만 시행한다.

선택 기준:

1. 함께 읽고 갱신되는 필드인가? 관련 기능이 그 상태의 소유권을 명확히 설명하는가?
2. 해당 상태가 외부 소비자에게 노출되는 계약, 직렬화, 리플렉션, GPU 데이터 레이아웃을 갖는가?
3. 추출 후 상위 클래스가 더 짧은 **책임의 목차**로 읽히는가?
4. 새 계층·간접 참조·할당이 오히려 탐색·성능·수명 관리를 어렵게 만들지는 않는가?
5. 단순 상태 묶음은 작은 구조체로 충분한가? 동작과 독립 수명이 있다면 클래스로, 파일/모듈 경계가 명확하면 별도 모듈로 나누는가?

후보 예시: Viewport 배치·선택 상태, Gizmo 드래그 상태, 시뮬레이션 UI 설정, Profile 편집 상태, Profiling 측정 상태. 이들은 **검토 후보**이지 반드시 추출해야 하는 타입 목록은 아니다.

## 03. 언어별 적용

| 언어 | 1단계 섹션·접기 | 2단계 책임 분리에서 고려할 단위 / 주의사항 |
|---|---|---|
| C | 3줄 `//` + 지원 환경에서 `#pragma region` | `struct`와 함수·`.h/.c` 모듈, `static` 내부 함수; 포인터 소유권·ABI 유지 |
| C++ | 3줄 `//` + 지원 환경에서 `#pragma region` | `struct`/`class`, 별도 소스 파일; 멤버 초기화 순서·공개 API·ABI·객체 수명 확인 |
| GLSL | 3줄 `//` + `// #region` | 보조 함수, 공통 셰이더 코드, 패스 분리; `layout`·바인딩·실행 단계·동기화 계약 우선 |
| Python | 3줄 `#` + `#region` | 함수·클래스·`dataclass`·모듈; import 순환·가변 기본값·호출 호환성 확인 |
| JavaScript / TypeScript | 3줄 `//` + `// #region` | 함수·객체·클래스·타입·모듈; export/import·비동기 흐름·클로저 상태 확인 |
| Shell | 3줄 `#` + `#region` | 함수·스크립트·공통 라이브러리; 종료 코드·`trap`·스코프·실행 순서·셸 방언 확인 |

**핵심은 모든 언어에 똑같은 타입 계층을 만드는 것이 아니라, 작업을 적게 읽어도 의미가 보이도록 각 언어에 맞게 경계를 명확히 하는 것**이다.

## 04. 두 단계를 분리한 작업·검증 절차

### 1단계 체크리스트 — 섹션 기반 구조화

- [ ] 현재 공개 API, 상태 변경, 호출 관계, 접근 범위, 빌드 제약을 확인했다.
- [ ] 섹션은 의미 있는 책임으로 묶었고 번호는 파일 내부 `00`부터 시작한다.
- [ ] 주석·Region 외의 변경을 피했으며 불가피한 선언 재배치가 있다면 영향을 별도 확인했다.
- [ ] 해당 언어의 파서/컴파일러와 편집기에서 섹션 표기가 허용되는지 확인했다.
- [ ] 실제 수행한 구문 검사·빌드·테스트만 검증 결과로 기록했다.

### 2단계 체크리스트 — 책임 기반 분리

- [ ] 추출 대상의 데이터 소유권, 참조·호출 관계, 초기화·종료·리셋 경로를 찾았다.
- [ ] 공개 API, ABI, 직렬화, 데이터 레이아웃, 에러·예외, 스레드/GPU 경계 영향을 확인했다.
- [ ] 기존 실행 순서와 상태 변화, 단위·통합 테스트를 비교했다.
- [ ] 필요하면 변경 전후 성능을 같은 조건에서 비교했다.
- [ ] [`Source Map`](../../02_Architecture/02_Source/00_Source-Map.md), 관련 [`Contracts`](../../02_Architecture/01_Contracts/00_Contract-Register.md), [`Flows`](../../02_Architecture/00_Flows/00_Main-Flow.md), 테스트 문서의 영향을 검토했다.

두 단계는 **별도 작업/커밋으로 분리하는 것을 권장**한다. 각 코드 변경 작업마다 [`Work Log`](../../04_Development/01_Work-Log.md)를 기록한다. 기존 동작 보존 검증 없이 “동작 동일”이라고 단정하지 않는다.
