# C++ Engineering Profile

> 기본 선호 언어는 C++이다. 이 문서는 **범용 개발 원칙**과 **선택 가능한 네이밍 프로필**을 구분한다. 프로젝트의 표준·컴파일러·실제 관례를 우선하며, Unreal Engine의 규칙을 일반 C++ 프로젝트에 무조건 강제하지 않는다.

## 00. 프로젝트별 활성 설정

| 설정 | 프로젝트에서 확정할 값 |
|---|---|
| C++ 표준 | `{{CPP_STANDARD}}` (미정이면 초기화 시 결정; 예: C++98/17/20/23) |
| 컴파일러 / 플랫폼 | `{{CPP_COMPILER}}` / `{{TARGET_PLATFORM}}` |
| 빌드 시스템 / 테스트 | `{{BUILD_SYSTEM}}` / `{{CPP_TEST_COMMAND}}` |
| 포맷터 / 정적 검사 | `{{CPP_FORMATTER}}` / `{{CPP_LINTER}}` |
| 네이밍 프로필 | `{{CPP_NAMING_PROFILE}}` (`Unreal`, `Unreal-Inspired`, `Existing`) |
| 파일 및 헤더 정책 | `.h` / `.hpp`, `#pragma once` / include guards, 프로젝트 관례 |

값의 정본은 [`Project Brief`](../../01_Project-Context/00_Project-Brief.md)와 [`Constraints`](../../01_Project-Context/03_Constraints.md)이며, 이 문서는 각 선택지의 뜻을 설명한다. **프로필이 미정이라면 무단 대규모 이름 변경을 수행하지 않는다.**

## 01. 언어별 구현 원칙

- [`Code Style`](../00_Core/00_Code-Style.md)의 상위 흐름 우선·의도 드러내기·동작 보존 규칙을 따른다.
- 공개 인터페이스는 호출자가 알아야 할 계약을 보이고, 구현 내부의 세부 계산은 적절히 캡슐화한다.
- RAII를 우선하고 owning / non-owning 경계, 예외 안전, 리소스 수명, const-correctness를 명시한다.
- 표준에 따른 구현 가능성에 주의한다. C++98 프로젝트에 C++11 이후 기능(`auto`, move, smart pointer 표준 라이브러리 등)을 무단 도입하지 않는다.
- 헤더 의존성은 직접 필요한 항목으로 최소화한다. 올바른 선언·정의를 위해 필요한 include는 생략하지 않는다.
- 성능이 중요한 경로의 추가 할당·복사·가상 호출·동기화 등은 측정하거나 변경 근거를 검토한다.

## 02. 이름 규칙 프로필

### A. `Unreal` — Unreal Engine 프로젝트의 공식 규칙

Epic Games의 [공식 C++ 코딩 표준](https://dev.epicgames.com/documentation/en-us/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine)을 따르며, 최신 프로젝트/엔진 버전의 실제 가이드를 우선 확인한다.

| 접두어 | 의미 | 예시 |
|---|---|---|
| `F` | 일반 클래스·구조체 등의 타입 | `FVector`, `FTransform` |
| `T` | 템플릿 클래스 | `TArray`, `TMap` |
| `U` | `UObject` 상속 클래스 | `UActorComponent` |
| `A` | `AActor` 상속 클래스 | `ACharacter` |
| `S` | `SWidget` 상속 Slate 위젯 | `SCompoundWidget` |
| `I` | 추상 인터페이스 클래스 | `IAnalyticsProvider` |
| `E` | 열거형 타입 | `EColorBits` |
| `b` | Boolean 변수 | `bIsVisible` |
| `C` | UE의 콘셉트 유사 타입 | `CStaticClassProvider` |

- 접두어는 **타입의 실제 성격 또는 상속 구조**를 표현한다. `U`, `A`, `S`를 임의의 모듈 카테고리 표시로 쓰지 않는다.
- 일반적으로 PascalCase를 사용하며, 함수명은 의미가 드러나는 동사·질문으로 구성한다.
- Unreal Header Tool(UHT)이 관여하는 타입은 엔진의 UHT·리플렉션 규칙을 반드시 따른다.
- 언리얼 공식 관례를 전부 임의의 standalone C++ 프로젝트에 적용하지 않는다.

### B. `Unreal-Inspired` — 일반 C++ 프로젝트 권장 후보

엔진에 종속되지 않는 프로젝트에서 **참고할 수 있는** 타입 표기 방식이며, 아직 프로젝트에서 확정하지 않았다면 강제하지 않는다.

| 대상 | 예시 | 원칙 |
|---|---|---|
| 일반 클래스·구조체 | `FFrameCoordinator`, `FFrameState` | `F` 접두어 |
| 템플릿 클래스 | `THandlePool<T>`, `TArrayView<T>` | `T` 접두어는 템플릿에 사용 |
| 인터페이스 | `IInputSource` | 추상 계약을 나타낼 때만 `I` |
| enum | `EFramePhase` | `E` 접두어 |
| Boolean | `bInitialized`, `bIsReady` | `b` 접두어 |
| 함수·메서드 | `UpdateFrame()`, `CanAdvance()` | PascalCase; 의도를 드러내는 이름 |
| 변수·매개변수 | `FrameIndex`, `DeltaTime` | PascalCase; 기존 코드 관례 우선 |

`U`, `A`, `S`, `C`는 Unreal 엔진의 특정 의미를 가진 접두어이므로 standalone 코드에서 별도 의미로 **재사용하지 않는 것**을 기본으로 한다. 필요하면 ADR과 네이밍 프로필에서 명시적으로 정의한다.

### C. `Existing` — 기존 저장소 관례

기존 코드 스타일, 포맷터, 외부 프레임워크 제약을 우선한다. 단순 취향 때문에 공개 타입명을 일괄 변경하지 않는다.

## 03. 파일·헤더·섹션

- 파일명은 주요 역할을 설명하며, 타입의 `F/T/U/A/...` 접두어를 파일명에 자동 복사하지 않는다.
- 헤더 가드는 프로젝트 관례와 도구 호환성을 따른다. Unreal에서는 `#pragma once` 관례를 따른다.
- C++ 파일의 섹션은 상위 흐름 → 조율 → 하위 구현 순으로 배치하되 불필요한 파일/섹션 분할은 피한다.
- 섹션 구분은 [`Local Numbering`](../00_Core/01_Local-Numbering-and-Navigation.md)의 **세 줄 주석 + `#pragma region 00_Main_Process`** 형식을 따른다. `#pragma region`은 비표준 확장이므로 IDE·컴파일러 호환성이 확인된 경우에만 사용하고, 그렇지 않으면 섹션 주석만 사용한다.
- 패키지·빌드·소스 경로에 영향을 미치면 파일명 숫자 접두어를 강요하지 않는다.

## 04. 주석·API 문서·검증

- 인라인 주석은 [`Source Comments`](../02_Documentation/01_Source-Comments.md)를, 공개 API 명세는 [`Doxygen`](../02_Documentation/00_Doxygen.md)을 따른다. C++ API의 Doxygen 설명 문장은 한국어를 사용한다.
- 의미 없는 설명 주석을 만들지 않고, 계약·단위·오류·소유권·스레드 경계·비자명한 이유를 문서화한다.
- **프로젝트가 직접 관리하는 C++ 소스·헤더(`.cpp`, `.hpp`, `.h` 등)의 맨 앞에 한국어 `@file` / `@brief` 주석을 필수로 작성한다.** 개별 함수의 Doxygen은 비자명한 계약이 있는 경우에만 작성한다.
- 코드 및 API 변경 후 관련 Contract와 주석을 동기화한다. 실행하지 않은 빌드·테스트를 성공했다고 기록하지 않는다.

## 05. 큰 클래스/상태 구조화

큰 클래스는 [공통 2단계 리팩터링 지침](../00_Core/03_Section-and-Responsibility-Refactoring.md)을 따른다. **1단계**에서는 Public API → 기능별 내부 메서드 → 관련 상태의 섹션을 두어 탐색을 개선한다. **2단계**에서는 `TProfilingUIState`처럼 응집된 상태를 구조체·클래스로 추출하거나 독립 컴포넌트로 분리하는 방안을 검토한다. `TDebugUI` 예시는 해당 지침에 있다.

C++ 멤버의 **선언 순서와 접근 지정자**를 섹션 정리 과정에서 무심코 바꾸지 않는다. 선언 순서는 초기화 순서에 영향을 주며, 필드 배치·공개 API·레이아웃·ABI·수명 등과 연관될 수 있다. 책임 추출은 실제 참조·리셋·검증을 동반하는 별도 변경이다.
