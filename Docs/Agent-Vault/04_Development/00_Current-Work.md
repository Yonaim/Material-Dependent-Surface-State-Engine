# Current Work

현재 진행 중이거나 검토할 과제만 기록한다. 완료 작업의 변경 근거는 [[04_Development/01_Work-Log|Work Log]], 검증 결과는 [[05_Experiments/01_Validation-and-Tests|Validation]]에서 확인한다.

## Compile-Time GPU A/B Shader Variants

상태: **Decision 0029 구현 완료 · A/B 성능·화질 검증 대기**

Decision 0029에 따라 build configuration당 43개의 유효 SPIR-V variant를 생성하고 A/B pipeline으로 선택한다. Solver의 cache/layout/precision, rendering의 fragment sampling/precompute 및 세 shared halo를 별도 variant로 연결했다. 측정 결과를 기록한 뒤 채택할 fast path와 제거할 비교 경로를 후속 결정한다.

Debug/Release 각각 대상 43개 SPIR-V, Debug 전체 MDSS 빌드, BrickCube.Scene 기본값 2 frame과 세 shared halo ON 5 frame 기동을 확인했다. 남은 검증은 같은 Scene, 초기 State, 입력, GPU와 측정 구간으로 variant별 GPU timestamps·전체 frame time을 비교하는 것이다. Smoothing의 chart/Profile 경계, tile halo 및 출력 동등성도 검사한다. Debug/Release shader 검증 define은 각자 별도 variant set을 만든다.

위 실행 결과는 당시 검증 기록의 결과이며 **2026-11-10 이후의 소스 변경까지 포함한 최신 빌드 결과가 아니다.** 이후 entry-flow/Region 정리 작업은 정적 점검으로 기록되어 있다. 최신 코드의 빌드·테스트 상태는 [Work Log](01_Work-Log.md) 및 [Validation](../05_Experiments/01_Validation-and-Tests.md)을 확인한다.

## 빌드 전 자동 포맷 연결

상태: **미완료 — 최신 Universal-Agent-Vault 기준과 현재 빌드 경로 사이의 차이**

`Scripts/build.ps1`와 `Scripts/build.sh`는 CMake configure/build를 호출하고, 현재 `CMakeLists.txt`에는 포맷 선행 타깃/의존성이 없다. 수동 포맷 스크립트는 `Scripts/format.ps1`·`Scripts/format.sh`다. Universal v6.0 이후 기준은 실제 빌드 타깃마다 컴파일 전에 `clang-format → indent_regions.py`를 실행하는 것이므로, 별도 작업으로 도구 실행 가능성·대상 범위·반복 실행 결과를 확인하고 CMake 및 스크립트 진입점에 연결해야 한다. 이 문서 갱신에서는 빌드 동작을 바꾸지 않았다.

## 연구·아이디어와 실제 구현 지시의 구분

- 2026-11-18 대화의 최신 연구 판정과 미실행 검증 순서는 개인 볼트의 [Reaction·Geometry 기여 검토](../../../../MDSSP-Personal-Vault/07_Learning-Notes/0002_Reaction-Geometry-Contribution-Review.md)에 정리했다. 문헌·현재 계약 대조가 우선이며 새 Solver·Modifier 구현 지시가 아니다. 로컬 앱·GPU·빌드·벤치마크·학습은 명시적 재개 요청 전 중지한다.
- `Surface Water Wetting`, `Angle-Sampled Gravity Cache`는 이 자료의 수집 시점에 **검토안/미채택**이었다. 이번 Agent Vault의 현재 구현 작업에는 포함하지 않았다.
- 해당 아이디어의 검토 맥락은 개인 `MDSS Engine` 볼트의 `05_Design/Ideas/`에서 관리한다. 실행을 위한 명시적 승인 전에는 새로운 기능 요구사항으로 취급하지 않는다.
- 코드 상태 및 최근 Work Log는 저장소와 대조하여 갱신한다. 이 볼트의 스냅샷을 현재 소스 검증 결과라고 주장하지 않는다.
