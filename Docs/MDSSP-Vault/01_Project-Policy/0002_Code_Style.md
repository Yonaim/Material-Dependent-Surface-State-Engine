# MDSS Engine Code Style

이 문서는 MDSS Engine C++ 코드의 명명·구성 관례를 정하고, 형식 규칙은 저장소 루트의 `.clang-format`을 기준으로 삼는다. `.clang-format`은 자동으로 적용할 수 있는 형식을 제어하지만, 명명 규칙과 소유권 의도까지 검사하지는 않는다.

## 적용 기준

| 항목 | 기준 |
|---|---|
| 언어 표준 | C++20. CMake에서 표준을 필수로 지정하고 compiler extensions를 끈다. |
| 자동 형식 | `.clang-format`을 기준으로 `clang-format`을 적용한다. |
| 프로젝트 관례 | 이 문서의 명명·파일·헤더 규칙을 따른다. |
| 우선순위 | 자동 형식은 `.clang-format`, 언어 표준은 `CMakeLists.txt`, 프로젝트 명명은 이 문서가 기준이다. |

## 명명 규칙

| 대상 | 규칙 | 예 |
|---|---|---|
| 프로젝트 namespace | `MDSS` 사용 | `namespace MDSS` |
| class, struct, enum type | PascalCase | `SurfaceStateSystem`, `Transform`, `SurfaceStateType` |
| enum value | PascalCase | `Wetness`, `Heat`, `Burn`, `Mud` |
| 함수·메서드 | PascalCase | `RenderFrame()`, `LoadTexture()` |
| 변수·멤버·매개변수 | PascalCase | `FrameRenderer`, `WindowHandle`, `DeltaTime` |
| bool 변수 | `b` + PascalCase | `bInitialized`, `bSRGB` |
| 상수·`constexpr` | PascalCase | `InvalidAssetHandle`, `MoveSpeed` |
| 약어 | 프로젝트에서 정한 약어는 대문자 유지 | `MDSS`, `GPU`, `UV`, `SR`, `OBJ`, `MTL` |

`enum class`를 사용해 열거자 이름을 해당 enum 범위에 둔다. 상태 채널 등의 고정 순서는 관련 데이터 계약에서 정의하고 코드와 Shader 사이에 일치시킨다.

## 파일과 헤더 구성

| 항목 | 규칙 |
|---|---|
| C++ 파일명 | 주된 class 또는 type 이름을 사용한다. 확장자는 `.h`, `.cpp`다. |
| Shader 파일명 | Shader stage를 확장자로 표시한다. 예: `.vert`, `.frag`, `.comp`. |
| Header guard | `#pragma once`를 사용한다. |
| Include 최소화 | 완전한 type 정의가 필요하지 않으면 forward declaration을 고려한다. |
| 대응 헤더 | `.cpp`에서는 해당 구현의 대응 헤더를 첫 include로 둔다. |

## 선언과 API

| 항목 | 규칙 |
|---|---|
| 상수성 | 읽기만 하는 매개변수와 메서드는 `const`로 의도를 표시한다. 큰 객체는 필요에 따라 `const T&`로 전달하고, 소유권 이전은 값 전달과 `std::move`로 명시한다. |
| 반환값 | 호출자가 결과를 무시하면 오류가 될 수 있는 조회·계산 함수에는 `nodiscard` attribute를 고려한다. 실제 코드에서도 결과가 중요한 조회 함수에 사용한다. |
| 예외 보장 | 예외를 던지지 않는 단순 조회 함수에만 `noexcept`를 붙인다. 할당·초기화 등 실패할 수 있는 작업에 무조건 지정하지 않는다. |
| 기본값 | 기본값이 유효한 데이터 멤버는 선언 위치에서 초기화한다. 초기화 목록은 멤버 선언 순서와 일치시킨다. |
| 접근 지정자 | 필요한 최소 범위만 공개하고 내부 구현은 `private`에 둔다. 선언 순서는 `.clang-format` 설정에 따라 정렬한다. |
| 전방 선언 | 헤더 의존성은 필요한 만큼만 포함한다. 멤버 선언에 완전한 정의가 필요하지 않으면 전방 선언을 사용한다. |
| 복사·이동 | 자원 소유 type은 복사·이동의 의미를 명확히 정한다. 복사가 안전하지 않으면 복사 생성자와 복사 대입을 `= delete` 한다. |

`auto`는 우변만으로 type이 명확하거나 iterator·템플릿 type을 길게 반복하는 것을 피할 때 사용한다. 일반적인 수치·상태 변수는 코드만 읽어도 의미 있는 type을 드러낸다.

## 오류와 로그

- 초기화나 필수 자원 생성에 실패하면 오류를 조용히 삼키지 않는다. 현재 엔진 초기화 코드는 실패 원인을 예외로 전달하고, 최상위 진입점에서 기록한 뒤 종료한다.
- 진단 로그는 `Logger`를 통해 기록하고, 메시지에 `Vulkan`, `AssetManager`처럼 모듈 맥락을 제공한다. 매 frame의 고빈도 경로에 반복 로그를 추가하지 않는다.
- 입력 데이터 검증은 가능한 한 오류가 발생한 Asset·Surface·Triangle 식별 정보를 함께 보고한다.

## Include 순서

Include는 한 줄씩 정렬하고 그룹 사이를 빈 줄로 분리한다. `.clang-format`의 `SortIncludes: true`, `IncludeBlocks: Regroup` 및 `IncludeCategories`가 자동 정렬 기준이다.

| 순서 | 종류 | 예 |
|---:|---|---|
| 1 | 구현 파일의 대응 헤더 | `#include "Scene/Camera.h"` |
| 2 | 따옴표를 사용하는 프로젝트 헤더 | `#include "Renderer/Renderer.h"` |
| 3 | 설정에서 별도 분류한 Vulkan / GLFW 헤더 | `#include <vulkan/vulkan.h>`, `#include <GLFW/glfw3.h>` |
| 4 | 그 밖의 꺾쇠괄호 헤더 | GLM, ImGui, C++ 표준 라이브러리 등 |

현재 설정에서는 따옴표 헤더를 하나의 include category로 취급한다. GLM·ImGui와 표준 라이브러리는 모두 마지막 angle-bracket category에 속하므로, 각각 별도 category처럼 수동으로 순서를 가정하지 않는다.

## 형식 규칙

아래 수치는 `.clang-format`의 현재 설정을 요약한 것이다. 자동 formatter 결과를 임의의 수동 정렬로 덮어쓰지 않는다.

| 항목 | 설정 |
|---|---|
| 기본 스타일 | LLVM 기반, C++ |
| 들여쓰기 | 공백 4칸, Tab 사용 금지 |
| 중괄호 | Allman style. 함수·class·namespace·제어문 본문을 다음 줄에서 연다. |
| 한 줄 본문 | 짧은 block, 함수, `if`, loop, case도 한 줄로 축약하지 않는다. |
| Namespace | 중첩 namespace마다 들여쓴다. |
| Pointer / reference | 기호를 type에 붙인다. 예: `GPUBuffer* Buffer`, `const Scene& SceneRef` |
| 줄 길이 | 120 columns를 기준으로 줄바꿈한다. |
| 함수 인자 | 여러 인자·매개변수를 한 줄에 무리하게 묶지 않는다. 줄바꿈 시 여는 괄호 기준으로 정렬한다. |
| 연산자 정렬 | 여러 줄 expression의 연산자를 설정에 따라 정렬한다. |
| 선언 정렬 | 연속된 선언은 정렬할 수 있지만 대입 연산자는 열 맞춤하지 않는다. |
| 괄호 공백 | 제어문 괄호 외 불필요한 괄호 안쪽 공백을 두지 않는다. |

예:

```cpp
namespace MDSS
{
    class SurfaceStateSystem
    {
    public:
        void UpdateState(float DeltaTime);

    private:
        bool bInitialized = false;
    };
}
```

## Type, 값과 소유권

- `std::uint32_t`, `std::int32_t`처럼 크기가 명확한 정수 type을 사용한다.
- 의미 있는 기본값이 있는 멤버는 선언 위치에서 초기화한다.
- `class`는 캡슐화·불변조건·소유권이 있는 추상화에, `struct`는 주로 간단한 데이터 묶음에 사용한다. 이는 코드 의도에 관한 관례이며 formatter가 검사하지 않는다.
- C++ 객체의 소유권은 명확히 한다. 소유하는 자원은 RAII와 smart pointer 사용을 우선하고, raw pointer/reference는 비소유 접근인지 드러나게 한다. Vulkan handle의 생성·파괴 책임도 owner가 명확해야 한다.
- 자원 wrapper의 복사·이동 가능 여부는 실제 소유권에 맞게 선언한다. 복사할 수 없는 type은 copy operation을 명시적으로 제한한다.

## Placeholder 파일

미래 구현을 위해 남겨둔 placeholder는 다음 주석으로 표시한다.

```cpp
// dummy
```

placeholder에는 구현된 것처럼 보이는 임시 동작을 넣지 않는다. 실제 구현이 시작되면 placeholder 주석과 불필요한 선언을 제거한다.

## 적용과 확인

```bash
# 파일 형식 적용
clang-format -i Source/Renderer/Renderer.cpp Source/Renderer/Renderer.h

# 형식 확인만 수행
clang-format --dry-run --Werror Source/Renderer/Renderer.cpp Source/Renderer/Renderer.h
```

변경한 C++ 파일에 formatter를 적용한 뒤 프로젝트 빌드와 관련 테스트도 실행한다. `clang-format`은 naming, API 의도, Vulkan lifetime, 알고리즘 정확성을 검증하는 도구가 아니다.
