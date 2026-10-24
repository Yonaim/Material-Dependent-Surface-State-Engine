# 코드 및 셰이더 구조

## C++ 소유 경계

- `Source/Rendering/Core`: 프레임, 스왑체인, 명령 기록을 조정한다. 장면 파이프라인과 머티리얼은 `Scene`, 상태 표시와 측면 생성은 `Surface`와 `Overlay`, 측정 출력은 `Profiling`에 둔다.
- `Source/DebugUI/Core`: ImGui 수명주기와 UI 상태를 소유한다. 입력·장면 편집은 `Editor`, 렌더·시뮬레이션·로그 화면은 `Panels`, 계측 화면은 `Profiling`에 둔다. `DebugUIShared.h`는 구현 파일에서만 사용하는 보조 코드다.
- `Source/SurfaceState/GPU/Abi`: CPU와 셰이더가 공유하는 레코드 크기·정렬 계약이다. `Packing`은 CPU 데이터를 업로드 형태로 변환하고, `Resources`는 Vulkan 버퍼·descriptor 수명주기를 소유한다.
- `Source/SurfaceState/Contact`: 접촉 입력 적용. `Solver`: 시뮬레이션 단계 기록과 시간 간격 판단. `Runtime`: 장면에 연결된 Surface 데이터. `Cache`: 디스크 캐시.

## 셰이더 추가·이동

`CMake/ShaderManifest.cmake`에 실행 가능한 `.vert`, `.frag`, `.comp` 진입점과 컴파일 variant를 등록한다. `.glsl`은 include 전용이다. 등록되지 않은 진입점은 CMake 구성 오류가 난다. 미구현 진입점은 `MDSS_SHADER_PLACEHOLDERS`에 둔다.

CMake가 manifest에서 `Build/generated/ShaderCatalog.h`를 생성한다. C++에서는 `MDSS::Shaders::EShader`와 `MDSS::Shaders::Path()`를 사용한다. 셰이더 파일을 이동할 때 manifest와 셰이더의 `#include`를 갱신하면 생성된 C++ 경로도 함께 바뀐다. 생성 헤더와 SPIR-V는 저장소에 넣지 않는다.

`Shaders/Shared/SurfaceState/SurfaceTypes.glsl`은 GPU 레코드 형식을 공유한다. CPU 형식은 `Source/SurfaceState/GPU/Abi/SurfaceGPUAbi.h`에 있다. 레코드 필드·binding·정렬을 바꿀 때 두 계약과 descriptor 생성 코드, 관련 GPU 테스트를 함께 확인한다.

## 검증

구조 변경 후 CMake 구성, Debug/Release의 모든 셰이더 variant, 앱과 테스트 대상 빌드, CTest를 확인한다. Vulkan 장면 테스트는 `MDSS_BUILD_VULKAN_SCENE_TESTS`로 활성화한다. 현재 구조 정리는 계산식과 성능 설정을 변경하지 않는다. 성능 비교가 필요한 변경에서는 `0003_Solver-Performance.md`의 측정 규칙과 실험 기록 절차를 따른다.
