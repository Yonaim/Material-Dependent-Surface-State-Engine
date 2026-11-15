# GLSL Engineering Profile — Graphics / Compute Shaders

> GLSL은 C/C++과 문법이 비슷하지만 **GPU 단계, 호스트–셰이더 인터페이스, 메모리 가시성·동기화**라는 고유한 제약을 가진다. `.comp`는 별도 언어가 아니라 GLSL 컴퓨트 셰이더의 대표적인 확장자다. 이 문서는 [Code Style](../00_Core/00_Code-Style.md)의 **상위 프로세스 우선** 원칙을 GLSL에 적용한다.

## 00. 프로젝트별 활성 설정

| 항목 | 프로젝트에서 확정할 값 |
|---|---|
| 셰이더 언어·버전 | `{{GLSL_VERSION}}` (사용할 셰이더별로 확인) |
| API / 실행 플랫폼 | `{{GRAPHICS_API}}` (예: Vulkan / OpenGL) |
| 컴파일·검증 도구 | `{{SHADER_COMPILER}}` / `{{SHADER_VALIDATION_COMMAND}}` |
| 사용 단계와 확장자 | `{{SHADER_STAGES}}` (`.vert`, `.frag`, `.comp`, `.glsl` 등) |
| 명명·서식 | `{{GLSL_NAMING}}` / `{{GLSL_FORMATTER}}` |
| CPU 측 관련 언어 | `{{HOST_LANGUAGE}}` 및 대응하는 [CPU–GPU 계약](../../02_Architecture/01_Contracts/00_Contract-Register.md) |

- 버전, 확장, 지원 기능 및 API별 `layout` 규칙은 프로젝트의 [Constraints](../../01_Project-Context/03_Constraints.md)를 우선한다. Vulkan 전용 문법을 OpenGL 셰이더에 무조건 적용하지 않는다.
- 기존 셰이더 스타일이 있다면 그 관례를 우선한다. 범용 GLSL의 유일한 공식 네이밍 방식이 있는 것처럼 주장하지 않는다.

## 01. 기본 서식과 선언 순서

- C/C++처럼 명확한 들여쓰기, 일관된 중괄호·공백, 응집된 헬퍼 함수를 사용한다. 기본 예시는 **4칸 들여쓰기**, 함수·변수 `snake_case`, 타입 `PascalCase`, 상수 `UPPER_SNAKE_CASE`이며 기존 프로젝트 관례가 우선한다.
- 버전 및 확장 지시문을 파일 앞쪽에 유지한다. **`#version`은 GLSL의 전처리 규칙에 따라 다른 전처리 명령·토큰보다 먼저** 둔다. 섹션 주석은 `#version` 다음에 놓는 것을 기본으로 한다.
- `layout(location=...)`, `layout(set=..., binding=...)`, `layout(local_size_*=...)`, `layout(std430)` 등은 해당 리소스 또는 실행 단계의 선언 가까이에 둔다.
- 권장 논리적 순서: **버전/확장 → 인터페이스·리소스 선언 → 헬퍼 선언(prototype) → `main()` → 세부 구현**. GLSL의 선언 순서 제약으로 컴파일이 안 된다면 프로토타입 추가 또는 헬퍼 정의 위치 조정을 사용한다.
- `main()`만 읽어도 주요 실행 단계와 유효 범위 처리가 보이게 하되, 짧은 계산까지 무조건 함수로 추출하지 않는다.

## 02. 섹션 주석 (Region 지시문을 강제하지 않음)

GLSL에서는 C/C++용 `#pragma region`이 표준화된 접기 지시문이 아니므로 **섹션 주석을 기본**으로 사용한다. 편집기에서 접기 마커를 지원한다면 실제 컴파일러 호환성을 확인한 경우에만 선택적으로 사용한다.

```glsl
#version 450

// ============================================================
// 00. Configuration & Resources
// ============================================================

// ...

// ============================================================
// 01. Main Process
// ============================================================

// ...

// ============================================================
// 02. Detailed Implementation
// ============================================================

// ...
```

- 섹션 번호는 파일마다 `00, 01, 02...`로 다시 시작하며, 책임 단위로만 나눈다.
- GPU 단계나 컴파일 조건으로 인해 섹션 배치에 제약이 있으면 **유효한 셰이더 코드**를 우선한다.

## 03. 컴퓨트 셰이더 예시 — Vulkan GLSL

아래 예시는 **구조를 보여 주는 최소 예제**다. 실제 Set/Binding 값, Workgroup Size, 버퍼 구조 및 Push Constant는 프로젝트에서 검증한 CPU 측 계약과 일치시켜야 한다.

```glsl
#version 450

// ============================================================
// 00. Configuration & Resources
// ============================================================

layout(local_size_x = 16, local_size_y = 16) in;

// CPU 대응: VkDescriptorSetLayout의 set=0/binding=0.
// 읽기 전용 입력 버퍼. 실제 오프셋·정렬은 CPU 자료형과 대조한다.
layout(set = 0, binding = 0, std430) readonly buffer InputBuffer {
    float values[];
} input_data;

// CPU 대응: set=0/binding=1. 다음 상태를 이 패스가 기록한다.
layout(set = 0, binding = 1, std430) writeonly buffer OutputBuffer {
    float values[];
} output_data;

// CPU 대응: Push Constant Range의 크기·단계·멤버 오프셋과 일치해야 한다.
layout(push_constant) uniform Parameters {
    uint width;
    uint height;
} params;

void update_cell(uint index);

// ============================================================
// 01. Main Process
// ============================================================

void main() {
    uvec2 cell = gl_GlobalInvocationID.xy;
    if (cell.x >= params.width || cell.y >= params.height) {
        return;
    }

    uint index = cell.y * params.width + cell.x;
    update_cell(index);
}

// ============================================================
// 02. Detailed Implementation
// ============================================================

void update_cell(uint index) {
    float current_value = input_data.values[index];
    output_data.values[index] = max(current_value - 0.01, 0.0);
}
```

이 예시에는 Workgroup Barrier가 없으므로 범위를 벗어난 Invocation의 조기 반환이 가능하다. **모든 Invocation의 참여가 필요한 `barrier()`가 있다면 동일한 방식의 조기 반환을 적용해서는 안 된다.**

## 04. 셰이더 소스 자체에 남길 계약 — 필수

> **셰이더 파일만 읽어도 해당 단계의 리소스 인터페이스와 실행 제약을 파악할 수 있어야 한다.** Architecture 문서에만 적고 소스에서 생략하지 않는다.

1. `layout` 선언 자체에 **Location / Set / Binding / Buffer Layout / Push Constant / Workgroup Size**를 정확하게 명시한다(해당되는 경우).
2. `readonly` / `writeonly` 등의 접근 제한과 실제 사용이 일치하도록 작성한다.
3. 단순 선언만으로 드러나지 않는 **CPU 측 대응 위치**, 멤버 단위·좌표계, 인덱싱 방식, 범위·수명, 입력과 출력의 의미를 필요한 위치에 짧게 주석으로 적는다.
4. Host와 다른 타입 표현·정렬(예: `bool`, `vec3`, 패딩, `std140`/`std430`)을 가정하지 않는다. **오프셋·정렬·크기**를 실제 규칙과 Host 데이터로 확인한다.
5. 리소스 바인딩이나 구조가 변경되면 CPU의 Descriptor Set Layout, Descriptor Write, Pipeline Layout, Push Constant Range와 함께 수정·검증한다.
6. 소스코드에는 해당 셰이더의 **구체적 선언과 로컬 제약**, Architecture에는 **시스템 전체 연결 관계**를 기록한다. 문서에 동일한 선언 블록 전체를 복사하지 않는다.

실제 CPU–GPU 연결 및 검증 문서화의 시작점은 [Contract Register](../../02_Architecture/01_Contracts/00_Contract-Register.md)다. 필요한 프로젝트에서만 [CPU–GPU Contract 템플릿](../04_Docs_Templates/06_Docs_Template-CPU-GPU-Contract.md)을 복사해 상세 문서를 만든다.

## 05. Graphics Shader별 확인 사항

- **Vertex:** 입력 Attribute의 `location`·형식과 CPU 측 Vertex Layout의 일치, 좌표계·행렬 곱셈 순서·단위 확인.
- **Fragment:** 입력 Interpolation, Render Target 출력 Location/형식, 색 공간(sRGB/Linear), Depth 및 Blend 규칙 확인.
- **Compute:** Workgroup Size·Dispatch 크기·Global/Local Invocation ID, 경계 검사, 메모리 접근 경합 및 동기화 확인.
- 여러 단계 사이에서 공유하는 인터페이스(`location`, 타입, 배열 크기 등)는 파이프라인 링크/검증으로 호환성을 확인한다.

## 06. 동기화와 성능 제약

- **`barrier()`는 Workgroup 내 실행 장벽**이며, 참가하는 Invocation들이 요구된 균일 제어 흐름에서 도달하도록 한다. Barrier 앞에서 일부만 반환하는 패턴을 경계한다.
- `memoryBarrier*()`는 메모리 가시성·순서 보장의 범위가 다르다. **서로 다른 Dispatch/Draw/Pass 사이의 의존성은 Vulkan 등 호스트 API 측 적절한 동기화가 별도로 필요**하며 셰이더 내부 명령만으로 대체하지 않는다.
- 데이터 경쟁, 읽기/쓰기 충돌, 동기화 전 데이터 소비, 인덱스 범위 초과를 검토한다.
- Workgroup 크기, 접근 패턴, Divergence, Atomics, Shared Memory 사용은 목표 GPU와 실제 측정 결과에 따라 결정한다. 모든 장치에 대해 성능 향상을 단정하지 않는다.

## 07. 검증과 문서 동기화

- 프로젝트에서 채택한 GLSL 컴파일러로 Stage/Variant별 구문·SPIR-V 컴파일을 확인하고, 가능하면 `spirv-val`, Reflection 및 Pipeline 생성 검증을 수행한다.
- Host–GPU 계약 변경 시 **셰이더 선언/주석 + CPU 측 코드 + Architecture/Contracts + 관련 테스트/Work Log**를 함께 검토한다.
- 관련 규칙: [Source Comments](../02_Documentation/01_Source-Comments.md), [Document Workflow](../03_Workflow/00_Document-Workflow.md), [Verification and Review](../03_Workflow/01_Verification-and-Review.md).
- 실행하지 않은 컴파일·실행·성능 테스트의 성공을 기록하지 않는다.
