# Build-System Selection — Decision Rules

> **빌드 시스템은 프로젝트 특성에 맞춰 선택한다.** 기존 프로젝트에서는 유지가 기본값이며, 전환은 별도의 명시적 설계 결정으로 다룬다.

## 00. 기본 추천

| 상황 | 기본 선택 | 적용 조건 |
|---|---|---|
| 이미 `Makefile`, CMake, Unreal 등으로 빌드 중 | **Existing** | 기존 설정·CI·타깃 확인 후 확장만 수행 |
| 소규모 C 과제·유틸리티, 외부 라이브러리 적음 | **Make** | GNU Make·C 컴파일러가 있고 단순한 빌드 그래프 |
| 소규모 C++ | **Make 또는 CMake** | 외부 SDK, 테스트·다중 플랫폼·IDE 생성 필요 시 CMake 쪽 우선 |
| 다중 모듈 C++, 여러 SDK, Vulkan, 플랫폼 지원 | **CMake** | 실제 툴체인과 의존성 설치 방법을 함께 확정 |
| `.uproject` 기반 게임·플러그인 | **UnrealBuildTool** | 기존 `Target.cs`·`Build.cs`와 엔진 버전에 따름 |
| 특수 플랫폼·기존 타 도구 | **해당 환경의 기존 도구** | Make/CMake로 무리하게 바꾸지 않음 |

Make/CMake는 선호 기본값이지 보편적인 우열이나 강제 규칙이 아니다. CMake는 Ninja·Makefile·Visual Studio·Xcode 등의 생성기를 지원하며, 외부 의존성이 많아야만 사용할 수 있는 것은 아니다.

## 10. Setup 결정 과정

1. **기존 빌드 자산 확인** — `.uproject`/`*.Target.cs` → Unreal, `CMakeLists.txt` → CMake, `Makefile` → Make. 다수 존재하면 어떤 경로가 실제 정본인지 확인한다.
2. **빌드 목표 확인** — 실행 파일 / 라이브러리 / Unreal 모듈, 지원 플랫폼, 컴파일러, C/C++ 표준, 디버그·릴리스 구성.
3. **의존성·자동화 확인** — SDK, 패키지 관리, 테스트, 셰이더 컴파일, IDE, CI, 다중 구성 필요성.
4. **최소 변경 선택** — 신규만 [Build Assets](../../Assets/Build/README.md)의 해당 템플릿을 구체화한다. 기존 빌드 파일은 병합한다.
5. **포맷과 빌드 연동 확인** — [`Pre-Build Formatting`](03_Pre-Build-Formatting.md)의 공통 실행기를 실제 컴파일의 선행조건으로 연결한다.

## 20. 빌드 시스템별 후크

**Make**: [`Makefile.template`](../../Assets/Build/Make/Makefile.template)은 `make build`에서 포맷 후 재귀적 Make로 컴파일한다. 포맷이 소스 타임스탬프를 바꾸어도 다시 평가하도록 설계했다. `_compile`이나 오브젝트 파일을 직접 요청하는 비표준 경로는 포맷을 건너뛸 수 있으므로 **프로젝트 공개 빌드 경로를 `make`/`make build`로 정하고 CI·IDE도 같은 경로를 사용하도록** 구성한다. 다른 직접 타깃이 필요하면 별도로 훅을 추가한다.

**CMake**: [`CMakeLists.txt.template`](../../Assets/Build/CMake/CMakeLists.txt.template)의 `add_custom_target(FormatBeforeBuild)`는 매 호출 실행되며, **`add_dependencies(ActualTarget FormatBeforeBuild)`를 모든 프로젝트 소유 빌드 타깃에 지정**해야 컴파일보다 선행한다. `CMakePresets.json`은 선택사항이며 Ninja가 설치되어야 예제 그대로 사용 가능하다.

**Unreal**: [`Build-Integration.md`](../../Assets/Build/Unreal/Build-Integration.md)의 `TargetRules.PreBuildSteps`를 실제 사용 타깃에 추가한다. `.uproject`/`*.Build.cs` 등을 Make/CMake로 대체하지 않는다. Live Coding과 내부 Editor 컴파일 경로는 항상 같은 훅을 거친다고 가정하지 않으며 실기기에서 확인한다.

## 30. 선택 검증

- [ ] 도구·SDK의 실제 설치 여부와 버전 확인
- [ ] 각 타깃의 의존성·소스 디렉터리 및 외부 소스 제외 확인
- [ ] Debug/Release, CLI/IDE/CI에서 최소 1개 실제 빌드 성공 여부 확인
- [ ] 빌드 전 포맷 호출과 포맷 오류 중단 확인
- [ ] 신규 프로젝트 외의 빌드 파일을 통째로 교체하지 않음
