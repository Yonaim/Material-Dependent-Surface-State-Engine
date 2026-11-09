# Universal Agent Vault v5.8 — 가독성 이관 결과

## 적용 기준과 범위

- 기준 원본: 저장소 루트의 독립 체크아웃 `Universal-Agent-Vault/`의 **v5.8 작업 트리**. 해당 체크아웃의 HEAD는 `93dbbed`(v5.5)이므로 v5.8을 커밋 버전으로 표현하지 않는다.
- 프로젝트 이전 기준: v5.5 공통 규칙과 기존 2단계 구조화 결과. v5.7의 의미 단위 공백·조건 서식·선택적 하위 Region, v5.8의 LF/CRLF 접기 설정을 반영했다.
- 추적 중인 구현 323개: Source 187, Shaders 69, Scripts 56, Tests 9, 루트 실행 도구 2. 외부 라이브러리·자산·생성물·독립 upstream 체크아웃과 사용자 미추적 `Shaders/Debug/test.glsl`은 제외했다.
- 공통 Standards 변경 문서 6개와 AGENTS·Document Map을 동기화했다. MDSS의 Unreal-Inspired 명명, 모든 shader에 최상위 Region을 두는 정책, Context·Contracts·Flows·과거 검증 기록은 보존했다.
- 최신 검증 문서의 중첩 Region 설명이 Markdown 표 중간을 끊던 배치는 표 뒤로 옮겼다. 요구사항 내용은 유지했다.

## 소스 변경

의미가 바뀌는 제어 흐름 경계와 결과 반환에 빈 줄을 두고, 길게 압축된 조건과 GLSL의 단일 문장 본문을 펼쳤다. C++ 식별자·멤버/접근/초기화 순서와 include 순서, GLSL 식·선언·바인딩·매크로 분기·barrier 순서는 유지했다. 기존 2단계 추출을 다시 수행하거나 작은 함수마다 Region을 만들지 않았다.

| 선택적 하위 Region 위치 | 구획 책임 |
|---|---|
| `Source/Rendering/Core/Renderer.h` | benchmark capture / overlay display / runtime performance controls |
| `Source/DebugUI/Panels/DebugUIViewSettings.cpp` | popup 공통 context·controls / mode별 switch 전체 |
| `Shaders/Rendering/Surface/Lighting/SurfaceLit.glsl` | normal 복원 / State·재질 효과 / 조명·발광 합성 |
| `Shaders/Rendering/Surface/Lighting/Lighting.glsl` | specular / film environment / 최종 조명 |
| `Shaders/Simulation/Solver/SurfaceSolverPass2.comp` | State 갱신 / 이웃 수송 gather |
| `Shaders/Simulation/Solver/SurfaceReaction.comp` | texel publish / rule 적분 / term 평가 |

최상위 3줄 주석은 유지하고 하위 구획만 한 줄 제목·연결 번호를 사용한다. 최대 깊이는 2단계다. C++ 하위 주석과 pragma의 논리적 들여쓰기는 의도적으로 보존했다. clang-format 22가 이를 열 0으로 옮기는 두 파일은 마커 삽입 후 일괄 재포맷하지 않았다.

현재 MSVC의 재빌드에서 기존 `#pragma region 08_...`, `09_...`가 C2041(잘못된 8진수)로 거부됐다. 원본에도 있던 라벨임을 snapshot으로 확인하고 해당 파일 및 새 하위 Region을 추가한 C++ 파일의 이름을 문자열로 감쌌다. 번호와 구획의 의미는 같다. 실행 코드 변경과 구별해 검증했다.

## 서식 도구와 설정

- C++: 기존 `.clang-format` / clang-format 22.1.6. 이번 이관에서는 `SortIncludes: Never`, `IncludeBlocks: Preserve`, `ReflowComments: false`, `SkipMacroDefinitionBody: true` 오버라이드로 include·계약 주석·매크로 본문을 보존했다.
- GLSL: `Shaders/.clang-format`에서 상위 Allman 규칙을 상속하고 include 정렬·주석 재서술을 끄며 `InsertBraces: true`를 사용한다. 기존 GLSL 이름은 변경하지 않는다.
- Python: `uvx --from black==26.10.1 black -S --target-version py312 --line-length 100 Scripts`. 문자열 표기는 유지하고 AST를 대조했다. 실행 환경 요구 버전이나 의존성을 바꾼 것은 아니다.
- Shell·PowerShell·루트 실행 도구는 기존 구획/흐름을 유지했다. 이번 변경으로 명령 의미나 실행 경로를 바꾸지 않았다.
- root/vault Custom Folding 설정을 v5.8 정규식으로 병합했다. 루트 `.vscode`는 기존 ignore 정책에 따라 로컬 설정이며 공유 정본은 볼트 `.vscode`다. 사용자 전역 설정은 이번 작업에서 수정하지 않았다.

## 검증과 한계

- 구현 323개 비교: C++ 201개는 주석·Region pragma를 제외한 토큰 동일, Python 44개 AST 동일. GLSL 69개는 기존 토큰에 제어문 중괄호만 추가됐는지 검사했다. 자료형·문자열·식·함수/API의 의도하지 않은 변경은 발견하지 못했다.
- Debug 전체 CMake build 성공. 첫 시도의 C2041 오류는 위의 기존 라벨을 보완한 뒤 해소됐다. 기존 padding/형변환 등 경고를 모두 해결한 작업은 아니다.
- Debug/Release shader entry·variant 150개 각각 glslc와 spirv-val 통과. 변경 전후 SPIR-V 150개 **바이트 동일**.
- 기존 바이너리의 변경 전 CTest와 최종 재빌드 후 CTest는 모두 **7 통과 / 7 실패**다. 실패한 테스트와 원인 범주가 같다. 기존 fixture/기대값을 변경하지 않았다.
- 설치된 Maptz 1.0.22의 실제 RegionProvider로 GLSL 69개·Shell 5개를 LF/CRLF 각각 검사: 148회 통과, GLSL 133개·Shell 2개 Region 인식 및 default 지정. 시작·종료·중첩도 정적으로 점검했다. 실제 편집기 수동/자동 접기 UI는 이번 작업에서 검증하지 않았다.
- 별도 성능 측정·전체 시나리오 렌더 캡처·장시간·다른 GPU/운영체제 검증은 수행하지 않았다. C++ 토큰/AST·SPIR-V 비교의 범위를 넘어선 동등성을 주장하지 않는다.
- 원자료: [요약](../../../Build/Validation/20261110-vault58/summary.md), `before/`, `coverage.json`, `before-shaders.json`, `after-shaders.json`, `provider-results.json`, build/CTest 로그.

## 파일별 적용 결과

대상 323개 중 242개 변경, 81개 유지. 미변경 파일에는 이번 규칙 차이만을 이유로 새로운 타입·함수·Region을 강제하지 않았다.

| 파일 | 적용 결과 | 최대 Region 깊이 |
|---|---|---:|
| `Scripts/Performance/analyze_history.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Performance/analyze_optimization_round.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Performance/analyze_suite.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Performance/analyze_transport_validation.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Performance/audit_optimization_round.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Performance/audit_study.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Performance/build_study_report.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Performance/optimization_round.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Performance/render_quality_review.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Performance/run_matrix.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Performance/run_suite.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Performance/summarize.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Performance/update_optimization_report.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Research/AuthoringProbe.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Scripts/Research/CouplingStudy.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Scripts/Research/MesoStudy.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Scripts/Research/ModularBaseline.h` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Scripts/Research/ResearchVulkanDevice.h` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Scripts/Research/analyze_authoring_pilot.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Research/analyze_authoring_retry.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Research/analyze_coupling_study.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Research/analyze_layout_lifetime_fix.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Research/analyze_meso_study.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Research/analyze_modular_baseline.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Research/archive_coupling_inputs.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Research/audit_coupling_study.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Research/audit_modular_baseline.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Research/authoring_pilot.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Research/authoring_pilot_figures.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Research/authoring_render_validation.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Research/authoring_study.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Research/finalize_authoring_readiness.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Research/finalize_authoring_retry.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Research/finalize_coupling_scope.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Research/finalize_layout_lifetime_fix.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Research/finalize_meso_study.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Research/finalize_modular_baseline.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Research/layout_lifetime_validation.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Research/meso_study.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Research/plot_modular_baseline.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Research/prepare_authoring_goal.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Research/replay_missing_authoring_captures.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Research/retry_authoring_validation.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Research/run_coupling_study.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Research/run_modular_baseline.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/Validation/verify_reactions.ps1` | 기존 구획·서식 유지 | 0 |
| `Scripts/build.ps1` | 기존 구획·서식 유지 | 0 |
| `Scripts/build.sh` | 기존 구획·서식 유지 | 1 |
| `Scripts/build_midterm_demos.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/build_reaction_transition_demo.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/build_showcase_stonework.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/format.ps1` | 기존 구획·서식 유지 | 0 |
| `Scripts/format.sh` | 기존 구획·서식 유지 | 0 |
| `Scripts/import_showcase_assets.py` | 공백·줄바꿈 정리; AST 동일 | 0 |
| `Scripts/run.sh` | 기존 구획·서식 유지 | 0 |
| `Scripts/run_performance_benchmarks.sh` | 기존 구획·서식 유지 | 0 |
| `Shaders/Debug/SurfaceDebug.frag` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Debug/SurfaceDebug.vert` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Debug/SurfaceDebugData.glsl` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Debug/TexelInspector.comp` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Debug/Views/SurfaceDebugColors.glsl` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Overlay/Coverage/OverlayCoverageSmoothing.comp` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Overlay/Coverage/OverlayOccupancyScan.comp` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Overlay/Coverage/OverlaySideCoverage.comp` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Overlay/Coverage/OverlayStateSampling.glsl` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Overlay/Geometry/OverlayDrawReset.comp` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Overlay/Geometry/OverlaySide.vert` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Overlay/Geometry/OverlaySideBoundary.comp` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Overlay/Geometry/OverlayTop.vert` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Overlay/Geometry/OverlayTopCommand.comp` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Overlay/Geometry/TexelGeometry.comp` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Overlay/Geometry/TexelGeometry.vert` | 기존 구획·서식 유지 | 1 |
| `Shaders/Rendering/Overlay/Geometry/TexelGeometryBaseline.comp` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Overlay/Geometry/TexelGeometryData.glsl` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Overlay/Geometry/TexelGeometryHeight.comp` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Overlay/Geometry/TexelMeshVertex.glsl` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Overlay/Shading/OverlayAppearance.glsl` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Overlay/Shading/OverlayCombined.frag` | 기존 구획·서식 유지 | 1 |
| `Shaders/Rendering/Overlay/Shading/OverlayLava.frag` | 기존 구획·서식 유지 | 1 |
| `Shaders/Rendering/Overlay/Shading/OverlayMud.frag` | 기존 구획·서식 유지 | 1 |
| `Shaders/Rendering/Overlay/Shading/OverlaySurface.glsl` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Scene/Gizmo.frag` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Scene/Gizmo.vert` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Scene/StaticMesh.frag` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Scene/StaticMesh.vert` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Scene/WorldReference.frag` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Scene/WorldReference.vert` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Surface/Lighting/BaseSurfaceLit.frag` | 기존 구획·서식 유지 | 1 |
| `Shaders/Rendering/Surface/Lighting/Effects/AbsorbedWetness.glsl` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Surface/Lighting/Effects/BurnScorched.glsl` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Surface/Lighting/Effects/Heat.glsl` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Surface/Lighting/Effects/Lava.glsl` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Surface/Lighting/Effects/Mud.glsl` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Surface/Lighting/Effects/WaterFilm.glsl` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Surface/Lighting/Lighting.glsl` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 2 |
| `Shaders/Rendering/Surface/Lighting/MaterialParameters.glsl` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Surface/Lighting/SurfaceLit.frag` | 기존 구획·서식 유지 | 1 |
| `Shaders/Rendering/Surface/Lighting/SurfaceLit.glsl` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 2 |
| `Shaders/Rendering/Surface/Lighting/SurfaceLit.vert` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Surface/Lighting/TexelSurfaceLit.frag` | 기존 구획·서식 유지 | 1 |
| `Shaders/Rendering/Surface/Lighting/TexelSurfaceLit.vert` | 기존 구획·서식 유지 | 1 |
| `Shaders/Rendering/Surface/Smoothing/HeightFieldSmoothing.comp` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Surface/StateTexture/RenderStateSampling.glsl` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Surface/StateTexture/RenderStateTexture.comp` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Surface/StateTexture/RenderStateTextureSmoothing.comp` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Rendering/Surface/StateTexture/StateSampling.glsl` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Shared/SurfaceState/SurfaceStateData.glsl` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Shared/SurfaceState/SurfaceTypes.glsl` | 기존 구획·서식 유지 | 1 |
| `Shaders/Simulation/Geometry/SurfaceAccumulationHeight.comp` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Simulation/Geometry/SurfaceDynamicGeometryUpdate.comp` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Simulation/Geometry/SurfaceDynamicWeightsUpdate.comp` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Simulation/Input/SurfaceInput.comp` | 기존 구획·서식 유지 | 1 |
| `Shaders/Simulation/Scheduling/SurfaceSparseScheduleReset.comp` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Simulation/Solver/FluxCache.glsl` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Simulation/Solver/GeometryDrive.glsl` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Simulation/Solver/ReactionBindings.glsl` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Simulation/Solver/SolverBindings.glsl` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Simulation/Solver/SparseSchedule.glsl` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Simulation/Solver/StateFinalize.glsl` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Simulation/Solver/StateMath.glsl` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Simulation/Solver/SurfaceReaction.comp` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 2 |
| `Shaders/Simulation/Solver/SurfaceSolverCommon.glsl` | 기존 구획·서식 유지 | 1 |
| `Shaders/Simulation/Solver/SurfaceSolverPass1.comp` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Shaders/Simulation/Solver/SurfaceSolverPass2.comp` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 2 |
| `Shaders/Simulation/Solver/Transport.glsl` | 공백·조건 블록·구획; 기존 토큰 + 중괄호 | 1 |
| `Source/Application/Application.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/Application/Application.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/Application/BenchmarkOptions.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/Application/BenchmarkOptions.h` | 기존 구획·서식 유지 | 0 |
| `Source/Application/EngineConfig.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/Application/EngineConfig.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/Application/FrameCoordinator.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/Application/FrameCoordinator.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/Application/PerformanceSettings.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/Application/SceneFileDialog.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/Application/SceneFileDialog.h` | 기존 구획·서식 유지 | 0 |
| `Source/Application/Window.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/Application/Window.h` | 기존 구획·서식 유지 | 0 |
| `Source/AssetManager/Assets/MaterialAsset.cpp` | 기존 구획·서식 유지 | 1 |
| `Source/AssetManager/Assets/MaterialAsset.h` | 기존 구획·서식 유지 | 0 |
| `Source/AssetManager/Assets/MeshAsset.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/AssetManager/Assets/MeshAsset.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/AssetManager/Assets/MeshSourceData.h` | 기존 구획·서식 유지 | 0 |
| `Source/AssetManager/Assets/SRProfileAsset.cpp` | 기존 구획·서식 유지 | 1 |
| `Source/AssetManager/Assets/SRProfileAsset.h` | 기존 구획·서식 유지 | 0 |
| `Source/AssetManager/Assets/TextureAsset.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/AssetManager/Assets/TextureAsset.h` | 기존 구획·서식 유지 | 0 |
| `Source/AssetManager/Core/Asset.cpp` | 기존 구획·서식 유지 | 1 |
| `Source/AssetManager/Core/Asset.h` | 기존 구획·서식 유지 | 0 |
| `Source/AssetManager/Core/AssetManager.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/AssetManager/Core/AssetManager.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/AssetManager/Loaders/MTLLoader.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/AssetManager/Loaders/MTLLoader.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/AssetManager/Loaders/OBJLoader.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/AssetManager/Loaders/OBJLoader.h` | 기존 구획·서식 유지 | 0 |
| `Source/AssetManager/Loaders/SRProfileLoader.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/AssetManager/Loaders/SRProfileLoader.h` | 기존 구획·서식 유지 | 0 |
| `Source/AssetManager/Loaders/SurfaceProfileDistributionLoader.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/AssetManager/Loaders/SurfaceProfileDistributionLoader.h` | 기존 구획·서식 유지 | 0 |
| `Source/AssetManager/Loaders/TextureLoader.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/AssetManager/Loaders/TextureLoader.h` | 기존 구획·서식 유지 | 0 |
| `Source/DebugUI/Core/DebugUI.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/DebugUI/Core/DebugUI.h` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/DebugUI/Core/DebugUIShared.h` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/DebugUI/Editor/DebugUIEditor.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/DebugUI/Panels/DebugUIContactProfile.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/DebugUI/Panels/DebugUILog.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/DebugUI/Panels/DebugUIRender.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/DebugUI/Panels/DebugUISimulation.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/DebugUI/Panels/DebugUIViewSettings.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 2 |
| `Source/DebugUI/Panels/DebugUIViewport.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/DebugUI/Panels/RenderViewDescriptions.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/DebugUI/Profiling/DebugUIPerformanceTab.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/DebugUI/Profiling/DebugUIProfiling.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/DebugUI/Profiling/ProfilingUIState.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/DebugUI/Profiling/ProfilingUIState.h` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/GPU/Pass/ComputePass.cpp` | 기존 구획·서식 유지 | 0 |
| `Source/GPU/Pass/ComputePass.h` | 기존 구획·서식 유지 | 0 |
| `Source/GPU/Pass/GraphicsPass.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/GPU/Pass/GraphicsPass.h` | 기존 구획·서식 유지 | 0 |
| `Source/GPU/Vulkan/Command/VulkanCommand.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/GPU/Vulkan/Command/VulkanCommand.h` | 기존 구획·서식 유지 | 0 |
| `Source/GPU/Vulkan/Pipeline/GraphicsPipeline.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/GPU/Vulkan/Pipeline/GraphicsPipeline.h` | 기존 구획·서식 유지 | 0 |
| `Source/GPU/Vulkan/Render/Framebuffer.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/GPU/Vulkan/Render/Framebuffer.h` | 기존 구획·서식 유지 | 0 |
| `Source/GPU/Vulkan/Render/RenderPass.cpp` | 기존 구획·서식 유지 | 1 |
| `Source/GPU/Vulkan/Render/RenderPass.h` | 기존 구획·서식 유지 | 0 |
| `Source/GPU/Vulkan/Resource/GPUBuffer.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/GPU/Vulkan/Resource/GPUBuffer.h` | 기존 구획·서식 유지 | 0 |
| `Source/GPU/Vulkan/Resource/GPUImage.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/GPU/Vulkan/Resource/GPUImage.h` | 기존 구획·서식 유지 | 0 |
| `Source/GPU/Vulkan/Resource/GPUImageView.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/GPU/Vulkan/Resource/GPUImageView.h` | 기존 구획·서식 유지 | 0 |
| `Source/GPU/Vulkan/Resource/GPUResourceMemoryStats.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/GPU/Vulkan/Resource/GPUSampler.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/GPU/Vulkan/Resource/GPUSampler.h` | 기존 구획·서식 유지 | 0 |
| `Source/GPU/Vulkan/Swapchain/Swapchain.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/GPU/Vulkan/Swapchain/Swapchain.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/GPU/Vulkan/VulkanContext.cpp` | 기존 구획·서식 유지 | 1 |
| `Source/GPU/Vulkan/VulkanContext.h` | 기존 구획·서식 유지 | 0 |
| `Source/GPU/Vulkan/VulkanDevice.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/GPU/Vulkan/VulkanDevice.h` | 기존 구획·서식 유지 | 0 |
| `Source/GPU/Vulkan/VulkanInstance.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/GPU/Vulkan/VulkanInstance.h` | 기존 구획·서식 유지 | 0 |
| `Source/GPU/Vulkan/VulkanQueue.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/GPU/Vulkan/VulkanQueue.h` | 기존 구획·서식 유지 | 0 |
| `Source/InputSystem/InputSystem.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/InputSystem/InputSystem.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/InputSystem/Raycaster.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/InputSystem/Raycaster.h` | 기존 구획·서식 유지 | 0 |
| `Source/Logger/Logger.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/Logger/Logger.h` | 기존 구획·서식 유지 | 0 |
| `Source/Rendering/Core/RenderContext.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/Rendering/Core/RenderContext.h` | 기존 구획·서식 유지 | 0 |
| `Source/Rendering/Core/RenderSettings.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/Rendering/Core/Renderer.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/Rendering/Core/Renderer.h` | 공백·서식·구획; Region 외 토큰 동일 | 2 |
| `Source/Rendering/Core/RendererCommands.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/Rendering/Core/RendererSettings.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/Rendering/Core/RendererShared.h` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/Rendering/Overlay/AccumulationOverlaySides.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/Rendering/Overlay/AccumulationOverlaySides.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/Rendering/Overlay/Geometry/TexelGeometryPreview.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/Rendering/Overlay/Geometry/TexelGeometryPreview.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/Rendering/Profiling/RendererBenchmark.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/Rendering/Profiling/RendererTiming.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/Rendering/Scene/FrustumCulling.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/Rendering/Scene/RendererMaterials.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/Rendering/Scene/WorldReferenceGeometry.h` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/Rendering/Surface/DemoSurfaceEffects.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/Rendering/Surface/HeightFieldSmoothing.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/Rendering/Surface/HeightFieldSmoothing.h` | 기존 구획·서식 유지 | 0 |
| `Source/Rendering/Surface/RenderStateTexture.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/Rendering/Surface/RenderStateTexture.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/Scene/Camera.cpp` | 기존 구획·서식 유지 | 1 |
| `Source/Scene/Camera.h` | 기존 구획·서식 유지 | 0 |
| `Source/Scene/DemoAnimation.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/Scene/DemoAnimation.h` | 기존 구획·서식 유지 | 0 |
| `Source/Scene/Scene.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/Scene/Scene.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/Scene/SceneLoader.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/Scene/SceneLoader.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/Scene/StaticMeshInstance.cpp` | 기존 구획·서식 유지 | 1 |
| `Source/Scene/StaticMeshInstance.h` | 기존 구획·서식 유지 | 0 |
| `Source/Scene/Transform.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/SurfaceState/Cache/SurfaceCache.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/SurfaceState/Cache/SurfaceCache.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/SurfaceState/Contact/SurfaceContactApplication.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/SurfaceState/Debug/DebugData.cpp` | 기존 구획·서식 유지 | 0 |
| `Source/SurfaceState/Debug/DebugData.h` | 기존 구획·서식 유지 | 0 |
| `Source/SurfaceState/Debug/TexelInspector.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/SurfaceState/Debug/TexelInspector.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/SurfaceState/GPU/Abi/SurfaceGPUAbi.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/SurfaceState/GPU/Abi/SurfaceGPUDescriptorBinding.h` | 기존 구획·서식 유지 | 0 |
| `Source/SurfaceState/GPU/Packing/ProfilePacking.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/SurfaceState/GPU/Packing/SharedGeometryPacking.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/SurfaceState/GPU/Packing/SparseMetadataLayout.cpp` | 기존 구획·서식 유지 | 0 |
| `Source/SurfaceState/GPU/Packing/SurfaceGPUResourceLayout.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/SurfaceState/GPU/Packing/TransferWeightPacking.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/SurfaceState/GPU/Resources/DescriptorResources.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/SurfaceState/GPU/Resources/DescriptorResources.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/SurfaceState/GPU/Resources/InstanceResources.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/SurfaceState/GPU/Resources/InstanceResources.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/SurfaceState/GPU/Resources/ProfileResources.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/SurfaceState/GPU/Resources/ProfileResources.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/SurfaceState/GPU/Resources/ResourceBufferHelpers.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/SurfaceState/GPU/Resources/SharedGeometryResources.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/SurfaceState/GPU/Resources/SharedGeometryResources.h` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/SurfaceState/GPU/Resources/SurfaceGPUBufferPool.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/SurfaceState/GPU/Resources/SurfaceGPUResourceManager.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/SurfaceState/GPU/Resources/SurfaceGPUResourceManager.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/SurfaceState/GPU/Resources/SurfaceGPUResources.h` | 기존 구획·서식 유지 | 0 |
| `Source/SurfaceState/Geometry/MesoGeometryBuilder.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/SurfaceState/Geometry/MesoGeometryBuilder.h` | 기존 구획·서식 유지 | 0 |
| `Source/SurfaceState/Geometry/SharedSurfaceGeometryData.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/SurfaceState/Geometry/SharedSurfaceGeometryData.h` | 기존 구획·서식 유지 | 0 |
| `Source/SurfaceState/Geometry/SurfaceGeometryBuilder.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/SurfaceState/Geometry/SurfaceGeometryBuilder.h` | 기존 구획·서식 유지 | 0 |
| `Source/SurfaceState/Geometry/SurfaceGeometryUpdate.cpp` | 기존 구획·서식 유지 | 0 |
| `Source/SurfaceState/Geometry/SurfaceGeometryUpdate.h` | 기존 구획·서식 유지 | 0 |
| `Source/SurfaceState/Geometry/SurfaceTexelMeshBuilder.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/SurfaceState/Geometry/SurfaceTexelMeshBuilder.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/SurfaceState/Mapping/NormalMapTransferNormalBuilder.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/SurfaceState/Mapping/NormalMapTransferNormalBuilder.h` | 기존 구획·서식 유지 | 0 |
| `Source/SurfaceState/Mapping/SurfaceMappingBuilder.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/SurfaceState/Mapping/SurfaceMappingBuilder.h` | 기존 구획·서식 유지 | 0 |
| `Source/SurfaceState/Mapping/SurfaceMappingData.h` | 기존 구획·서식 유지 | 0 |
| `Source/SurfaceState/Mapping/SurfaceMappingValidation.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/SurfaceState/Mapping/SurfaceMappingValidation.h` | 기존 구획·서식 유지 | 0 |
| `Source/SurfaceState/Runtime/SurfaceDataManager.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/SurfaceState/Runtime/SurfaceDataManager.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/SurfaceState/Runtime/SurfaceRuntimeData.cpp` | 기존 구획·서식 유지 | 1 |
| `Source/SurfaceState/Runtime/SurfaceRuntimeData.h` | 기존 구획·서식 유지 | 0 |
| `Source/SurfaceState/Solver/SimulationClock.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/SurfaceState/Solver/SurfaceStateSolver.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/SurfaceState/Solver/SurfaceStateSolver.h` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/SurfaceState/Solver/SurfaceStepRecording.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/SurfaceState/Solver/SurfaceTimestep.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/SurfaceState/State/SurfaceInput.h` | 기존 구획·서식 유지 | 0 |
| `Source/SurfaceState/State/SurfaceInstanceStateData.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/SurfaceState/State/SurfaceInstanceStateData.h` | 기존 구획·서식 유지 | 0 |
| `Source/SurfaceState/SurfaceStateSystem.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/SurfaceState/SurfaceStateSystem.h` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/SurfaceState/Types/SurfaceMappingTypes.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/SurfaceState/Types/SurfaceReactions.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/SurfaceState/Types/SurfaceSolverRates.h` | 기존 구획·서식 유지 | 0 |
| `Source/SurfaceState/Types/SurfaceStateRegistry.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/SurfaceState/Types/SurfaceStateRegistry.h` | 기존 구획·서식 유지 | 0 |
| `Source/SurfaceState/Types/SurfaceStateTypes.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Source/SurfaceState/Types/SurfaceStateTypes.h` | 공백·서식·구획; Region 외 토큰 동일 | 0 |
| `Source/main.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Tests/NormalMapTransferTests.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Tests/SceneResourceTests.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Tests/SharedSurfaceGeometryTests.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Tests/SimulationTransportTests.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Tests/SurfaceCacheTests.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Tests/SurfaceDebugRenderingTests.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Tests/SurfaceGPUResourceTests.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Tests/SurfaceMappingTests.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `Tests/SurfaceStateContractTests.cpp` | 공백·서식·구획; Region 외 토큰 동일 | 1 |
| `run.ps1` | 기존 구획·서식 유지 | 0 |
| `run.sh` | 기존 구획·서식 유지 | 0 |

## 후속: Shader 중첩 Region 전수 검토

이 문서의 Region 개수·깊이와 선택적 적용 위치는 최초 v5.8 이관 시점의 기록이다. 당시 shader 중첩 적용은 4개 파일에 한정되어 있었다. 2026-11-10 후속 지시로 shader 69개를 다시 검토해 35개 파일에 115개 하위 구획을 추가했다. 최신 파일별 생성/유지 판단과 Region 개수는 [전수 검토표](14_Shader-Nested-Region-Coverage.md)를 따른다.
