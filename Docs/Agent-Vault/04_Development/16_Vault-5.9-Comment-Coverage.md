# Universal v5.9 파일·Region 주석 전수 적용

상태: **적용·검증 완료 — 2026-11-10**. 로컬 `Universal-Agent-Vault/`의 v5.9 작업 트리를 기준으로 공통 규칙을 병합했다. 기존 명명 프로필·MSVC Region 라벨 인용·프로젝트 고유 라우팅·GPU 계약은 유지한다.

## 폐기된 볼트 정리

`Docs/MDSSP-Agent-Vault`는 현재 볼트로 이관된 폐기 경로다. 앞선 누락 파일 복원에서 이전 문서 20개와 보관 안내 1개까지 가져온 판단을 바로잡아 운영 문서 트리에서 제거했다. 재현 확인용 원본은 Git 제외된 `Build/Validation/20261110-vault59/RetiredVault/`에만 보관한다. 활성 볼트는 `Docs/Agent-Vault` 하나다.

연구·보고 스크립트 11개와 보관 안내 1개의 활성 경로를 현재 Performance/Validation/Authoring Protocol/GPU Layout/Current Work로 이관했다. 단순 폴더명 교체 대신 실제 재번호화된 문서에 연결했다. 과거 manifest·해시·판정 원문은 역사 기록으로 보존한다. 비교 데모 에셋 13개와 실험 자료 126개의 복원은 유지한다.

## 코드·문서 적용 범위

- 구현 323개: C++ 201, GLSL 69, Python 44, Bash 5, PowerShell 4. 그중 246개 파일 변경, 파일 머리말 171개 보완.
- CMake 구성 4개에도 별도 한국어 역할 머리말을 추가했다. 생성물·제3자 코드·Build 내부 도구·바이트 보존 대상 실험 원본은 제외한다.
- Region 656개: 시작 마커 바로 아래 설명 654개 추가, 기존 설명 1개 유지, 예약 구획 1개 생략. 마커·번호·범위·선언 순서 유지.
- 생략: `Shaders/Simulation/Input/SurfaceInput.comp`의 `00_Input_Placeholder`는 실행 구현이 없는 예약 구획이며 기존 reserved 안내·파일 역할로 범위를 알 수 있다. 새로운 실행 역할을 만들어 설명하지 않는다.
- C/C++ 한국어 `@file`·`@brief`, GLSL 일반 `//` 머리말, Python 모듈 docstring, Shell shebang/PowerShell param 조건을 확인했다. 분리된 C++ 파일에 남아 있던 잘못된 `@file` 5개도 바로잡았다.
- 공통 Standards 12개·AGENTS·Document Map·CHANGELOG·README를 v5.9와 동기화했다. 현재 프로젝트의 명명·MSVC 호환성 예외는 보존했다. Context·Source Map·Current Work·Validation·Work Log를 갱신했다.

## 검증과 한계

- 최종 CMake configure·전체 Debug build 성공. 복구된 연구 도구와 현재 WIP 기능 포함.
- C++ 201개·GLSL 69개의 비주석 토큰 동일. Python 44개의 AST는 모듈 docstring과 승인된 문서 경로 교체를 제외하면 동일하다. Python의 `__doc__`와 이를 쓰는 help 설명은 새 한국어 역할 주석을 반영한다.
- Bash 5개 구문·실행 토큰 동일, PowerShell 4개 parser 오류 0·비주석 토큰 동일. 활성 Python의 구 볼트 경로 잔존 0, 새 라우팅 대상 문서 존재 확인.
- shader entry·variant 150개 glslc/spirv-val 통과. v5.9 이전 WIP의 동일 구성 SPIR-V 150개와 바이트 동일.
- 실제 Maptz parser: shader 69개·Bash 5개 × LF/CRLF = 148회 통과. 각 EOL에서 252개 영역(shader 248 + Bash 4), 기존 default 접기 설정 유지. 실제 IDE 접힘 UI는 새로 검증하지 않았다.
- 원본 코드 EOL·Region 짝·최대 2단계·설명 인접성/들여쓰기·CRLF 대응 diff check 확인.
- 주석·문서 라우팅 변경이므로 전체 CTest·GPU 렌더·성능·전체 연구 파이프라인은 반복하지 않았다. 직전 기능 이식의 전체 CTest 9 통과/6 기존 실패 및 Standards 후속 관련 3개 통과와 구분한다.
- [원자료](../../../Build/Validation/20261110-vault59/source-validation.json), 파일별/영역별 전체 기록은 같은 디렉터리의 `coverage.json`에 보존한다. 커밋·push 없음.

## 파일별 적용표

머리말 열의 변경은 이번 작업 이전 snapshot과 비교한 결과다. Region 설명 추가 수는 기존 경계를 유지한 수치이며 새 Region 생성 수가 아니다.

| 파일 | 머리말 보완 | 설명 추가 | 기존 유지 | 생략 |
|---|---:|---:|---:|---:|
| [Scripts/Performance/analyze_history.py](../../../Scripts/Performance/analyze_history.py) | 예 | 0 | 0 | 0 |
| [Scripts/Performance/analyze_optimization_round.py](../../../Scripts/Performance/analyze_optimization_round.py) | 예 | 0 | 0 | 0 |
| [Scripts/Performance/analyze_suite.py](../../../Scripts/Performance/analyze_suite.py) | 예 | 4 | 0 | 0 |
| [Scripts/Performance/analyze_transport_validation.py](../../../Scripts/Performance/analyze_transport_validation.py) | 예 | 0 | 0 | 0 |
| [Scripts/Performance/audit_optimization_round.py](../../../Scripts/Performance/audit_optimization_round.py) | 예 | 0 | 0 | 0 |
| [Scripts/Performance/audit_study.py](../../../Scripts/Performance/audit_study.py) | 예 | 4 | 0 | 0 |
| [Scripts/Performance/build_study_report.py](../../../Scripts/Performance/build_study_report.py) | 예 | 4 | 0 | 0 |
| [Scripts/Performance/optimization_round.py](../../../Scripts/Performance/optimization_round.py) | 예 | 0 | 0 | 0 |
| [Scripts/Performance/render_quality_review.py](../../../Scripts/Performance/render_quality_review.py) | 예 | 0 | 0 | 0 |
| [Scripts/Performance/run_matrix.py](../../../Scripts/Performance/run_matrix.py) | 예 | 4 | 0 | 0 |
| [Scripts/Performance/run_suite.py](../../../Scripts/Performance/run_suite.py) | 예 | 4 | 0 | 0 |
| [Scripts/Performance/summarize.py](../../../Scripts/Performance/summarize.py) | 예 | 4 | 0 | 0 |
| [Scripts/Performance/update_optimization_report.py](../../../Scripts/Performance/update_optimization_report.py) | 예 | 0 | 0 | 0 |
| [Scripts/Research/AuthoringProbe.cpp](../../../Scripts/Research/AuthoringProbe.cpp) | 예 | 0 | 0 | 0 |
| [Scripts/Research/CouplingStudy.cpp](../../../Scripts/Research/CouplingStudy.cpp) | 예 | 3 | 0 | 0 |
| [Scripts/Research/MesoStudy.h](../../../Scripts/Research/MesoStudy.h) | 예 | 0 | 0 | 0 |
| [Scripts/Research/ModularBaseline.h](../../../Scripts/Research/ModularBaseline.h) | 예 | 3 | 0 | 0 |
| [Scripts/Research/ResearchVulkanDevice.h](../../../Scripts/Research/ResearchVulkanDevice.h) | 예 | 1 | 0 | 0 |
| [Scripts/Research/analyze_authoring_pilot.py](../../../Scripts/Research/analyze_authoring_pilot.py) | 예 | 3 | 0 | 0 |
| [Scripts/Research/analyze_authoring_retry.py](../../../Scripts/Research/analyze_authoring_retry.py) | 예 | 3 | 0 | 0 |
| [Scripts/Research/analyze_coupling_study.py](../../../Scripts/Research/analyze_coupling_study.py) | 예 | 3 | 0 | 0 |
| [Scripts/Research/analyze_layout_lifetime_fix.py](../../../Scripts/Research/analyze_layout_lifetime_fix.py) | 예 | 0 | 0 | 0 |
| [Scripts/Research/analyze_meso_study.py](../../../Scripts/Research/analyze_meso_study.py) | 예 | 3 | 0 | 0 |
| [Scripts/Research/analyze_modular_baseline.py](../../../Scripts/Research/analyze_modular_baseline.py) | 예 | 3 | 0 | 0 |
| [Scripts/Research/archive_coupling_inputs.py](../../../Scripts/Research/archive_coupling_inputs.py) | 예 | 0 | 0 | 0 |
| [Scripts/Research/audit_coupling_study.py](../../../Scripts/Research/audit_coupling_study.py) | 예 | 0 | 0 | 0 |
| [Scripts/Research/audit_modular_baseline.py](../../../Scripts/Research/audit_modular_baseline.py) | 예 | 0 | 0 | 0 |
| [Scripts/Research/authoring_pilot.py](../../../Scripts/Research/authoring_pilot.py) | 예 | 3 | 0 | 0 |
| [Scripts/Research/authoring_pilot_figures.py](../../../Scripts/Research/authoring_pilot_figures.py) | 예 | 0 | 0 | 0 |
| [Scripts/Research/authoring_render_validation.py](../../../Scripts/Research/authoring_render_validation.py) | 예 | 3 | 0 | 0 |
| [Scripts/Research/authoring_study.py](../../../Scripts/Research/authoring_study.py) | 예 | 3 | 0 | 0 |
| [Scripts/Research/finalize_authoring_readiness.py](../../../Scripts/Research/finalize_authoring_readiness.py) | 예 | 1 | 0 | 0 |
| [Scripts/Research/finalize_authoring_retry.py](../../../Scripts/Research/finalize_authoring_retry.py) | 예 | 0 | 0 | 0 |
| [Scripts/Research/finalize_coupling_scope.py](../../../Scripts/Research/finalize_coupling_scope.py) | 예 | 0 | 0 | 0 |
| [Scripts/Research/finalize_layout_lifetime_fix.py](../../../Scripts/Research/finalize_layout_lifetime_fix.py) | 예 | 0 | 0 | 0 |
| [Scripts/Research/finalize_meso_study.py](../../../Scripts/Research/finalize_meso_study.py) | 예 | 0 | 0 | 0 |
| [Scripts/Research/finalize_modular_baseline.py](../../../Scripts/Research/finalize_modular_baseline.py) | 예 | 0 | 0 | 0 |
| [Scripts/Research/layout_lifetime_validation.py](../../../Scripts/Research/layout_lifetime_validation.py) | 예 | 0 | 0 | 0 |
| [Scripts/Research/meso_study.py](../../../Scripts/Research/meso_study.py) | 예 | 3 | 0 | 0 |
| [Scripts/Research/plot_modular_baseline.py](../../../Scripts/Research/plot_modular_baseline.py) | 예 | 0 | 0 | 0 |
| [Scripts/Research/prepare_authoring_goal.py](../../../Scripts/Research/prepare_authoring_goal.py) | 예 | 3 | 0 | 0 |
| [Scripts/Research/replay_missing_authoring_captures.py](../../../Scripts/Research/replay_missing_authoring_captures.py) | 예 | 0 | 0 | 0 |
| [Scripts/Research/retry_authoring_validation.py](../../../Scripts/Research/retry_authoring_validation.py) | 예 | 0 | 0 | 0 |
| [Scripts/Research/run_coupling_study.py](../../../Scripts/Research/run_coupling_study.py) | 예 | 3 | 0 | 0 |
| [Scripts/Research/run_modular_baseline.py](../../../Scripts/Research/run_modular_baseline.py) | 예 | 3 | 0 | 0 |
| [Scripts/Validation/verify_reactions.ps1](../../../Scripts/Validation/verify_reactions.ps1) | 예 | 0 | 0 | 0 |
| [Scripts/build.ps1](../../../Scripts/build.ps1) | 예 | 3 | 0 | 0 |
| [Scripts/build.sh](../../../Scripts/build.sh) | 예 | 4 | 0 | 0 |
| [Scripts/build_midterm_demos.py](../../../Scripts/build_midterm_demos.py) | 예 | 3 | 0 | 0 |
| [Scripts/build_reaction_transition_demo.py](../../../Scripts/build_reaction_transition_demo.py) | 예 | 3 | 0 | 0 |
| [Scripts/build_showcase_stonework.py](../../../Scripts/build_showcase_stonework.py) | 예 | 3 | 0 | 0 |
| [Scripts/format.ps1](../../../Scripts/format.ps1) | 예 | 0 | 0 | 0 |
| [Scripts/format.sh](../../../Scripts/format.sh) | 예 | 0 | 0 | 0 |
| [Scripts/import_showcase_assets.py](../../../Scripts/import_showcase_assets.py) | 예 | 4 | 0 | 0 |
| [Scripts/run.sh](../../../Scripts/run.sh) | 예 | 0 | 0 | 0 |
| [Scripts/run_performance_benchmarks.sh](../../../Scripts/run_performance_benchmarks.sh) | 예 | 0 | 0 | 0 |
| [Shaders/Debug/SurfaceDebug.frag](../../../Shaders/Debug/SurfaceDebug.frag) | 예 | 13 | 0 | 0 |
| [Shaders/Debug/SurfaceDebug.vert](../../../Shaders/Debug/SurfaceDebug.vert) | 예 | 5 | 0 | 0 |
| [Shaders/Debug/SurfaceDebugData.glsl](../../../Shaders/Debug/SurfaceDebugData.glsl) | 예 | 4 | 0 | 0 |
| [Shaders/Debug/TexelInspector.comp](../../../Shaders/Debug/TexelInspector.comp) | 예 | 4 | 0 | 0 |
| [Shaders/Debug/Views/SurfaceDebugColors.glsl](../../../Shaders/Debug/Views/SurfaceDebugColors.glsl) | 예 | 3 | 0 | 0 |
| [Shaders/Rendering/Overlay/Coverage/OverlayCoverageSmoothing.comp](../../../Shaders/Rendering/Overlay/Coverage/OverlayCoverageSmoothing.comp) | 예 | 5 | 0 | 0 |
| [Shaders/Rendering/Overlay/Coverage/OverlayOccupancyScan.comp](../../../Shaders/Rendering/Overlay/Coverage/OverlayOccupancyScan.comp) | 예 | 9 | 0 | 0 |
| [Shaders/Rendering/Overlay/Coverage/OverlaySideCoverage.comp](../../../Shaders/Rendering/Overlay/Coverage/OverlaySideCoverage.comp) | 예 | 6 | 0 | 0 |
| [Shaders/Rendering/Overlay/Coverage/OverlayStateSampling.glsl](../../../Shaders/Rendering/Overlay/Coverage/OverlayStateSampling.glsl) | 예 | 3 | 0 | 0 |
| [Shaders/Rendering/Overlay/Geometry/OverlayDrawReset.comp](../../../Shaders/Rendering/Overlay/Geometry/OverlayDrawReset.comp) | 예 | 2 | 0 | 0 |
| [Shaders/Rendering/Overlay/Geometry/OverlaySide.vert](../../../Shaders/Rendering/Overlay/Geometry/OverlaySide.vert) | 예 | 4 | 0 | 0 |
| [Shaders/Rendering/Overlay/Geometry/OverlaySideBoundary.comp](../../../Shaders/Rendering/Overlay/Geometry/OverlaySideBoundary.comp) | 예 | 7 | 0 | 0 |
| [Shaders/Rendering/Overlay/Geometry/OverlayTop.vert](../../../Shaders/Rendering/Overlay/Geometry/OverlayTop.vert) | 예 | 4 | 0 | 0 |
| [Shaders/Rendering/Overlay/Geometry/OverlayTopCommand.comp](../../../Shaders/Rendering/Overlay/Geometry/OverlayTopCommand.comp) | 예 | 2 | 0 | 0 |
| [Shaders/Rendering/Overlay/Geometry/TexelGeometry.comp](../../../Shaders/Rendering/Overlay/Geometry/TexelGeometry.comp) | 예 | 8 | 0 | 0 |
| [Shaders/Rendering/Overlay/Geometry/TexelGeometry.vert](../../../Shaders/Rendering/Overlay/Geometry/TexelGeometry.vert) | 예 | 1 | 0 | 0 |
| [Shaders/Rendering/Overlay/Geometry/TexelGeometryBaseline.comp](../../../Shaders/Rendering/Overlay/Geometry/TexelGeometryBaseline.comp) | 예 | 5 | 0 | 0 |
| [Shaders/Rendering/Overlay/Geometry/TexelGeometryData.glsl](../../../Shaders/Rendering/Overlay/Geometry/TexelGeometryData.glsl) | 예 | 1 | 0 | 0 |
| [Shaders/Rendering/Overlay/Geometry/TexelGeometryHeight.comp](../../../Shaders/Rendering/Overlay/Geometry/TexelGeometryHeight.comp) | 예 | 5 | 0 | 0 |
| [Shaders/Rendering/Overlay/Geometry/TexelMeshVertex.glsl](../../../Shaders/Rendering/Overlay/Geometry/TexelMeshVertex.glsl) | 예 | 4 | 0 | 0 |
| [Shaders/Rendering/Overlay/Shading/OverlayAppearance.glsl](../../../Shaders/Rendering/Overlay/Shading/OverlayAppearance.glsl) | 예 | 3 | 0 | 0 |
| [Shaders/Rendering/Overlay/Shading/OverlayCombined.frag](../../../Shaders/Rendering/Overlay/Shading/OverlayCombined.frag) | 예 | 1 | 0 | 0 |
| [Shaders/Rendering/Overlay/Shading/OverlayLava.frag](../../../Shaders/Rendering/Overlay/Shading/OverlayLava.frag) | 예 | 1 | 0 | 0 |
| [Shaders/Rendering/Overlay/Shading/OverlayMud.frag](../../../Shaders/Rendering/Overlay/Shading/OverlayMud.frag) | 예 | 1 | 0 | 0 |
| [Shaders/Rendering/Overlay/Shading/OverlaySurface.glsl](../../../Shaders/Rendering/Overlay/Shading/OverlaySurface.glsl) | 예 | 8 | 0 | 0 |
| [Shaders/Rendering/Scene/Gizmo.frag](../../../Shaders/Rendering/Scene/Gizmo.frag) | 예 | 2 | 0 | 0 |
| [Shaders/Rendering/Scene/Gizmo.vert](../../../Shaders/Rendering/Scene/Gizmo.vert) | 예 | 2 | 0 | 0 |
| [Shaders/Rendering/Scene/StaticMesh.frag](../../../Shaders/Rendering/Scene/StaticMesh.frag) | 예 | 5 | 0 | 0 |
| [Shaders/Rendering/Scene/StaticMesh.vert](../../../Shaders/Rendering/Scene/StaticMesh.vert) | 예 | 2 | 0 | 0 |
| [Shaders/Rendering/Scene/WorldReference.frag](../../../Shaders/Rendering/Scene/WorldReference.frag) | 예 | 2 | 0 | 0 |
| [Shaders/Rendering/Scene/WorldReference.vert](../../../Shaders/Rendering/Scene/WorldReference.vert) | 예 | 2 | 0 | 0 |
| [Shaders/Rendering/Surface/Lighting/BaseSurfaceLit.frag](../../../Shaders/Rendering/Surface/Lighting/BaseSurfaceLit.frag) | 예 | 1 | 0 | 0 |
| [Shaders/Rendering/Surface/Lighting/Effects/AbsorbedWetness.glsl](../../../Shaders/Rendering/Surface/Lighting/Effects/AbsorbedWetness.glsl) | 예 | 1 | 0 | 0 |
| [Shaders/Rendering/Surface/Lighting/Effects/BurnScorched.glsl](../../../Shaders/Rendering/Surface/Lighting/Effects/BurnScorched.glsl) | 예 | 1 | 0 | 0 |
| [Shaders/Rendering/Surface/Lighting/Effects/Heat.glsl](../../../Shaders/Rendering/Surface/Lighting/Effects/Heat.glsl) | 예 | 1 | 0 | 0 |
| [Shaders/Rendering/Surface/Lighting/Effects/Lava.glsl](../../../Shaders/Rendering/Surface/Lighting/Effects/Lava.glsl) | 예 | 1 | 0 | 0 |
| [Shaders/Rendering/Surface/Lighting/Effects/Mud.glsl](../../../Shaders/Rendering/Surface/Lighting/Effects/Mud.glsl) | 예 | 1 | 0 | 0 |
| [Shaders/Rendering/Surface/Lighting/Effects/WaterFilm.glsl](../../../Shaders/Rendering/Surface/Lighting/Effects/WaterFilm.glsl) | 예 | 1 | 0 | 0 |
| [Shaders/Rendering/Surface/Lighting/Lighting.glsl](../../../Shaders/Rendering/Surface/Lighting/Lighting.glsl) | 예 | 4 | 0 | 0 |
| [Shaders/Rendering/Surface/Lighting/MaterialParameters.glsl](../../../Shaders/Rendering/Surface/Lighting/MaterialParameters.glsl) | 예 | 1 | 0 | 0 |
| [Shaders/Rendering/Surface/Lighting/SurfaceLit.frag](../../../Shaders/Rendering/Surface/Lighting/SurfaceLit.frag) | 예 | 1 | 0 | 0 |
| [Shaders/Rendering/Surface/Lighting/SurfaceLit.glsl](../../../Shaders/Rendering/Surface/Lighting/SurfaceLit.glsl) | 예 | 5 | 0 | 0 |
| [Shaders/Rendering/Surface/Lighting/SurfaceLit.vert](../../../Shaders/Rendering/Surface/Lighting/SurfaceLit.vert) | 예 | 2 | 0 | 0 |
| [Shaders/Rendering/Surface/Lighting/TexelSurfaceLit.frag](../../../Shaders/Rendering/Surface/Lighting/TexelSurfaceLit.frag) | 예 | 1 | 0 | 0 |
| [Shaders/Rendering/Surface/Lighting/TexelSurfaceLit.vert](../../../Shaders/Rendering/Surface/Lighting/TexelSurfaceLit.vert) | 예 | 1 | 0 | 0 |
| [Shaders/Rendering/Surface/Smoothing/HeightFieldSmoothing.comp](../../../Shaders/Rendering/Surface/Smoothing/HeightFieldSmoothing.comp) | 예 | 8 | 0 | 0 |
| [Shaders/Rendering/Surface/StateTexture/RenderStateSampling.glsl](../../../Shaders/Rendering/Surface/StateTexture/RenderStateSampling.glsl) | 예 | 3 | 0 | 0 |
| [Shaders/Rendering/Surface/StateTexture/RenderStateTexture.comp](../../../Shaders/Rendering/Surface/StateTexture/RenderStateTexture.comp) | 예 | 2 | 0 | 0 |
| [Shaders/Rendering/Surface/StateTexture/RenderStateTextureSmoothing.comp](../../../Shaders/Rendering/Surface/StateTexture/RenderStateTextureSmoothing.comp) | 예 | 7 | 0 | 0 |
| [Shaders/Rendering/Surface/StateTexture/StateSampling.glsl](../../../Shaders/Rendering/Surface/StateTexture/StateSampling.glsl) | 예 | 5 | 0 | 0 |
| [Shaders/Shared/SurfaceState/SurfaceStateData.glsl](../../../Shaders/Shared/SurfaceState/SurfaceStateData.glsl) | 예 | 4 | 0 | 0 |
| [Shaders/Shared/SurfaceState/SurfaceTypes.glsl](../../../Shaders/Shared/SurfaceState/SurfaceTypes.glsl) | 예 | 1 | 0 | 0 |
| [Shaders/Simulation/Geometry/SurfaceAccumulationHeight.comp](../../../Shaders/Simulation/Geometry/SurfaceAccumulationHeight.comp) | 예 | 5 | 0 | 0 |
| [Shaders/Simulation/Geometry/SurfaceDynamicGeometryUpdate.comp](../../../Shaders/Simulation/Geometry/SurfaceDynamicGeometryUpdate.comp) | 예 | 4 | 0 | 0 |
| [Shaders/Simulation/Geometry/SurfaceDynamicWeightsUpdate.comp](../../../Shaders/Simulation/Geometry/SurfaceDynamicWeightsUpdate.comp) | 예 | 6 | 0 | 0 |
| [Shaders/Simulation/Input/SurfaceInput.comp](../../../Shaders/Simulation/Input/SurfaceInput.comp) | 예 | 0 | 0 | 1 |
| [Shaders/Simulation/Scheduling/SurfaceSparseScheduleReset.comp](../../../Shaders/Simulation/Scheduling/SurfaceSparseScheduleReset.comp) | 예 | 9 | 0 | 0 |
| [Shaders/Simulation/Solver/FluxCache.glsl](../../../Shaders/Simulation/Solver/FluxCache.glsl) | 예 | 2 | 1 | 0 |
| [Shaders/Simulation/Solver/GeometryDrive.glsl](../../../Shaders/Simulation/Solver/GeometryDrive.glsl) | 예 | 3 | 0 | 0 |
| [Shaders/Simulation/Solver/ReactionBindings.glsl](../../../Shaders/Simulation/Solver/ReactionBindings.glsl) | 예 | 1 | 0 | 0 |
| [Shaders/Simulation/Solver/SolverBindings.glsl](../../../Shaders/Simulation/Solver/SolverBindings.glsl) | 예 | 6 | 0 | 0 |
| [Shaders/Simulation/Solver/SparseSchedule.glsl](../../../Shaders/Simulation/Solver/SparseSchedule.glsl) | 예 | 6 | 0 | 0 |
| [Shaders/Simulation/Solver/StateFinalize.glsl](../../../Shaders/Simulation/Solver/StateFinalize.glsl) | 예 | 1 | 0 | 0 |
| [Shaders/Simulation/Solver/StateMath.glsl](../../../Shaders/Simulation/Solver/StateMath.glsl) | 예 | 5 | 0 | 0 |
| [Shaders/Simulation/Solver/SurfaceReaction.comp](../../../Shaders/Simulation/Solver/SurfaceReaction.comp) | 예 | 6 | 0 | 0 |
| [Shaders/Simulation/Solver/SurfaceSolverCommon.glsl](../../../Shaders/Simulation/Solver/SurfaceSolverCommon.glsl) | 예 | 1 | 0 | 0 |
| [Shaders/Simulation/Solver/SurfaceSolverPass1.comp](../../../Shaders/Simulation/Solver/SurfaceSolverPass1.comp) | 예 | 7 | 0 | 0 |
| [Shaders/Simulation/Solver/SurfaceSolverPass2.comp](../../../Shaders/Simulation/Solver/SurfaceSolverPass2.comp) | 예 | 5 | 0 | 0 |
| [Shaders/Simulation/Solver/Transport.glsl](../../../Shaders/Simulation/Solver/Transport.glsl) | 예 | 3 | 0 | 0 |
| [Source/Application/Application.cpp](../../../Source/Application/Application.cpp) | 유지 | 3 | 0 | 0 |
| [Source/Application/Application.h](../../../Source/Application/Application.h) | 유지 | 0 | 0 | 0 |
| [Source/Application/BenchmarkOptions.cpp](../../../Source/Application/BenchmarkOptions.cpp) | 예 | 1 | 0 | 0 |
| [Source/Application/BenchmarkOptions.h](../../../Source/Application/BenchmarkOptions.h) | 예 | 0 | 0 | 0 |
| [Source/Application/EngineConfig.cpp](../../../Source/Application/EngineConfig.cpp) | 유지 | 1 | 0 | 0 |
| [Source/Application/EngineConfig.h](../../../Source/Application/EngineConfig.h) | 유지 | 0 | 0 | 0 |
| [Source/Application/FrameCoordinator.cpp](../../../Source/Application/FrameCoordinator.cpp) | 예 | 3 | 0 | 0 |
| [Source/Application/FrameCoordinator.h](../../../Source/Application/FrameCoordinator.h) | 예 | 0 | 0 | 0 |
| [Source/Application/PerformanceSettings.h](../../../Source/Application/PerformanceSettings.h) | 예 | 0 | 0 | 0 |
| [Source/Application/SceneFileDialog.cpp](../../../Source/Application/SceneFileDialog.cpp) | 유지 | 1 | 0 | 0 |
| [Source/Application/SceneFileDialog.h](../../../Source/Application/SceneFileDialog.h) | 유지 | 0 | 0 | 0 |
| [Source/Application/Window.cpp](../../../Source/Application/Window.cpp) | 유지 | 3 | 0 | 0 |
| [Source/Application/Window.h](../../../Source/Application/Window.h) | 유지 | 0 | 0 | 0 |
| [Source/AssetManager/Assets/MaterialAsset.cpp](../../../Source/AssetManager/Assets/MaterialAsset.cpp) | 유지 | 1 | 0 | 0 |
| [Source/AssetManager/Assets/MaterialAsset.h](../../../Source/AssetManager/Assets/MaterialAsset.h) | 유지 | 0 | 0 | 0 |
| [Source/AssetManager/Assets/MeshAsset.cpp](../../../Source/AssetManager/Assets/MeshAsset.cpp) | 유지 | 1 | 0 | 0 |
| [Source/AssetManager/Assets/MeshAsset.h](../../../Source/AssetManager/Assets/MeshAsset.h) | 유지 | 0 | 0 | 0 |
| [Source/AssetManager/Assets/MeshSourceData.h](../../../Source/AssetManager/Assets/MeshSourceData.h) | 유지 | 0 | 0 | 0 |
| [Source/AssetManager/Assets/SRProfileAsset.cpp](../../../Source/AssetManager/Assets/SRProfileAsset.cpp) | 유지 | 1 | 0 | 0 |
| [Source/AssetManager/Assets/SRProfileAsset.h](../../../Source/AssetManager/Assets/SRProfileAsset.h) | 유지 | 0 | 0 | 0 |
| [Source/AssetManager/Assets/TextureAsset.cpp](../../../Source/AssetManager/Assets/TextureAsset.cpp) | 유지 | 3 | 0 | 0 |
| [Source/AssetManager/Assets/TextureAsset.h](../../../Source/AssetManager/Assets/TextureAsset.h) | 유지 | 0 | 0 | 0 |
| [Source/AssetManager/Core/Asset.cpp](../../../Source/AssetManager/Core/Asset.cpp) | 유지 | 1 | 0 | 0 |
| [Source/AssetManager/Core/Asset.h](../../../Source/AssetManager/Core/Asset.h) | 유지 | 0 | 0 | 0 |
| [Source/AssetManager/Core/AssetManager.cpp](../../../Source/AssetManager/Core/AssetManager.cpp) | 유지 | 4 | 0 | 0 |
| [Source/AssetManager/Core/AssetManager.h](../../../Source/AssetManager/Core/AssetManager.h) | 유지 | 0 | 0 | 0 |
| [Source/AssetManager/Loaders/MTLLoader.cpp](../../../Source/AssetManager/Loaders/MTLLoader.cpp) | 유지 | 1 | 0 | 0 |
| [Source/AssetManager/Loaders/MTLLoader.h](../../../Source/AssetManager/Loaders/MTLLoader.h) | 유지 | 0 | 0 | 0 |
| [Source/AssetManager/Loaders/OBJLoader.cpp](../../../Source/AssetManager/Loaders/OBJLoader.cpp) | 유지 | 2 | 0 | 0 |
| [Source/AssetManager/Loaders/OBJLoader.h](../../../Source/AssetManager/Loaders/OBJLoader.h) | 유지 | 0 | 0 | 0 |
| [Source/AssetManager/Loaders/SRProfileLoader.cpp](../../../Source/AssetManager/Loaders/SRProfileLoader.cpp) | 유지 | 1 | 0 | 0 |
| [Source/AssetManager/Loaders/SRProfileLoader.h](../../../Source/AssetManager/Loaders/SRProfileLoader.h) | 유지 | 0 | 0 | 0 |
| [Source/AssetManager/Loaders/SurfaceProfileDistributionLoader.cpp](../../../Source/AssetManager/Loaders/SurfaceProfileDistributionLoader.cpp) | 유지 | 2 | 0 | 0 |
| [Source/AssetManager/Loaders/SurfaceProfileDistributionLoader.h](../../../Source/AssetManager/Loaders/SurfaceProfileDistributionLoader.h) | 유지 | 0 | 0 | 0 |
| [Source/AssetManager/Loaders/TextureLoader.cpp](../../../Source/AssetManager/Loaders/TextureLoader.cpp) | 유지 | 1 | 0 | 0 |
| [Source/AssetManager/Loaders/TextureLoader.h](../../../Source/AssetManager/Loaders/TextureLoader.h) | 유지 | 0 | 0 | 0 |
| [Source/DebugUI/Core/DebugUI.cpp](../../../Source/DebugUI/Core/DebugUI.cpp) | 예 | 4 | 0 | 0 |
| [Source/DebugUI/Core/DebugUI.h](../../../Source/DebugUI/Core/DebugUI.h) | 유지 | 14 | 0 | 0 |
| [Source/DebugUI/Core/DebugUIShared.h](../../../Source/DebugUI/Core/DebugUIShared.h) | 예 | 2 | 0 | 0 |
| [Source/DebugUI/Editor/DebugUIEditor.cpp](../../../Source/DebugUI/Editor/DebugUIEditor.cpp) | 예 | 4 | 0 | 0 |
| [Source/DebugUI/Panels/DebugUIContactProfile.cpp](../../../Source/DebugUI/Panels/DebugUIContactProfile.cpp) | 예 | 2 | 0 | 0 |
| [Source/DebugUI/Panels/DebugUILog.cpp](../../../Source/DebugUI/Panels/DebugUILog.cpp) | 예 | 0 | 0 | 0 |
| [Source/DebugUI/Panels/DebugUIRender.cpp](../../../Source/DebugUI/Panels/DebugUIRender.cpp) | 예 | 1 | 0 | 0 |
| [Source/DebugUI/Panels/DebugUISimulation.cpp](../../../Source/DebugUI/Panels/DebugUISimulation.cpp) | 예 | 1 | 0 | 0 |
| [Source/DebugUI/Panels/DebugUIViewSettings.cpp](../../../Source/DebugUI/Panels/DebugUIViewSettings.cpp) | 예 | 3 | 0 | 0 |
| [Source/DebugUI/Panels/DebugUIViewport.cpp](../../../Source/DebugUI/Panels/DebugUIViewport.cpp) | 예 | 3 | 0 | 0 |
| [Source/DebugUI/Panels/RenderViewDescriptions.h](../../../Source/DebugUI/Panels/RenderViewDescriptions.h) | 유지 | 0 | 0 | 0 |
| [Source/DebugUI/Profiling/DebugUIPerformanceTab.cpp](../../../Source/DebugUI/Profiling/DebugUIPerformanceTab.cpp) | 예 | 0 | 0 | 0 |
| [Source/DebugUI/Profiling/DebugUIProfiling.cpp](../../../Source/DebugUI/Profiling/DebugUIProfiling.cpp) | 예 | 1 | 0 | 0 |
| [Source/DebugUI/Profiling/ProfilingUIState.cpp](../../../Source/DebugUI/Profiling/ProfilingUIState.cpp) | 예 | 1 | 0 | 0 |
| [Source/DebugUI/Profiling/ProfilingUIState.h](../../../Source/DebugUI/Profiling/ProfilingUIState.h) | 유지 | 2 | 0 | 0 |
| [Source/GPU/Pass/ComputePass.cpp](../../../Source/GPU/Pass/ComputePass.cpp) | 유지 | 0 | 0 | 0 |
| [Source/GPU/Pass/ComputePass.h](../../../Source/GPU/Pass/ComputePass.h) | 유지 | 0 | 0 | 0 |
| [Source/GPU/Pass/GraphicsPass.cpp](../../../Source/GPU/Pass/GraphicsPass.cpp) | 유지 | 1 | 0 | 0 |
| [Source/GPU/Pass/GraphicsPass.h](../../../Source/GPU/Pass/GraphicsPass.h) | 유지 | 0 | 0 | 0 |
| [Source/GPU/Vulkan/Command/VulkanCommand.cpp](../../../Source/GPU/Vulkan/Command/VulkanCommand.cpp) | 유지 | 3 | 0 | 0 |
| [Source/GPU/Vulkan/Command/VulkanCommand.h](../../../Source/GPU/Vulkan/Command/VulkanCommand.h) | 유지 | 0 | 0 | 0 |
| [Source/GPU/Vulkan/Pipeline/GraphicsPipeline.cpp](../../../Source/GPU/Vulkan/Pipeline/GraphicsPipeline.cpp) | 유지 | 3 | 0 | 0 |
| [Source/GPU/Vulkan/Pipeline/GraphicsPipeline.h](../../../Source/GPU/Vulkan/Pipeline/GraphicsPipeline.h) | 유지 | 0 | 0 | 0 |
| [Source/GPU/Vulkan/Render/Framebuffer.cpp](../../../Source/GPU/Vulkan/Render/Framebuffer.cpp) | 유지 | 3 | 0 | 0 |
| [Source/GPU/Vulkan/Render/Framebuffer.h](../../../Source/GPU/Vulkan/Render/Framebuffer.h) | 유지 | 0 | 0 | 0 |
| [Source/GPU/Vulkan/Render/RenderPass.cpp](../../../Source/GPU/Vulkan/Render/RenderPass.cpp) | 유지 | 2 | 0 | 0 |
| [Source/GPU/Vulkan/Render/RenderPass.h](../../../Source/GPU/Vulkan/Render/RenderPass.h) | 유지 | 0 | 0 | 0 |
| [Source/GPU/Vulkan/Resource/GPUBuffer.cpp](../../../Source/GPU/Vulkan/Resource/GPUBuffer.cpp) | 유지 | 4 | 0 | 0 |
| [Source/GPU/Vulkan/Resource/GPUBuffer.h](../../../Source/GPU/Vulkan/Resource/GPUBuffer.h) | 유지 | 0 | 0 | 0 |
| [Source/GPU/Vulkan/Resource/GPUImage.cpp](../../../Source/GPU/Vulkan/Resource/GPUImage.cpp) | 유지 | 3 | 0 | 0 |
| [Source/GPU/Vulkan/Resource/GPUImage.h](../../../Source/GPU/Vulkan/Resource/GPUImage.h) | 유지 | 0 | 0 | 0 |
| [Source/GPU/Vulkan/Resource/GPUImageView.cpp](../../../Source/GPU/Vulkan/Resource/GPUImageView.cpp) | 유지 | 3 | 0 | 0 |
| [Source/GPU/Vulkan/Resource/GPUImageView.h](../../../Source/GPU/Vulkan/Resource/GPUImageView.h) | 유지 | 0 | 0 | 0 |
| [Source/GPU/Vulkan/Resource/GPUResourceMemoryStats.h](../../../Source/GPU/Vulkan/Resource/GPUResourceMemoryStats.h) | 유지 | 0 | 0 | 0 |
| [Source/GPU/Vulkan/Resource/GPUSampler.cpp](../../../Source/GPU/Vulkan/Resource/GPUSampler.cpp) | 유지 | 1 | 0 | 0 |
| [Source/GPU/Vulkan/Resource/GPUSampler.h](../../../Source/GPU/Vulkan/Resource/GPUSampler.h) | 유지 | 0 | 0 | 0 |
| [Source/GPU/Vulkan/Swapchain/Swapchain.cpp](../../../Source/GPU/Vulkan/Swapchain/Swapchain.cpp) | 유지 | 4 | 0 | 0 |
| [Source/GPU/Vulkan/Swapchain/Swapchain.h](../../../Source/GPU/Vulkan/Swapchain/Swapchain.h) | 유지 | 0 | 0 | 0 |
| [Source/GPU/Vulkan/VulkanContext.cpp](../../../Source/GPU/Vulkan/VulkanContext.cpp) | 유지 | 3 | 0 | 0 |
| [Source/GPU/Vulkan/VulkanContext.h](../../../Source/GPU/Vulkan/VulkanContext.h) | 유지 | 0 | 0 | 0 |
| [Source/GPU/Vulkan/VulkanDevice.cpp](../../../Source/GPU/Vulkan/VulkanDevice.cpp) | 유지 | 2 | 0 | 0 |
| [Source/GPU/Vulkan/VulkanDevice.h](../../../Source/GPU/Vulkan/VulkanDevice.h) | 유지 | 0 | 0 | 0 |
| [Source/GPU/Vulkan/VulkanInstance.cpp](../../../Source/GPU/Vulkan/VulkanInstance.cpp) | 유지 | 3 | 0 | 0 |
| [Source/GPU/Vulkan/VulkanInstance.h](../../../Source/GPU/Vulkan/VulkanInstance.h) | 유지 | 0 | 0 | 0 |
| [Source/GPU/Vulkan/VulkanQueue.cpp](../../../Source/GPU/Vulkan/VulkanQueue.cpp) | 유지 | 2 | 0 | 0 |
| [Source/GPU/Vulkan/VulkanQueue.h](../../../Source/GPU/Vulkan/VulkanQueue.h) | 유지 | 0 | 0 | 0 |
| [Source/InputSystem/InputSystem.cpp](../../../Source/InputSystem/InputSystem.cpp) | 유지 | 1 | 0 | 0 |
| [Source/InputSystem/InputSystem.h](../../../Source/InputSystem/InputSystem.h) | 유지 | 0 | 0 | 0 |
| [Source/InputSystem/Raycaster.cpp](../../../Source/InputSystem/Raycaster.cpp) | 유지 | 1 | 0 | 0 |
| [Source/InputSystem/Raycaster.h](../../../Source/InputSystem/Raycaster.h) | 유지 | 0 | 0 | 0 |
| [Source/Logger/Logger.cpp](../../../Source/Logger/Logger.cpp) | 유지 | 3 | 0 | 0 |
| [Source/Logger/Logger.h](../../../Source/Logger/Logger.h) | 유지 | 0 | 0 | 0 |
| [Source/Rendering/Core/RenderContext.cpp](../../../Source/Rendering/Core/RenderContext.cpp) | 유지 | 3 | 0 | 0 |
| [Source/Rendering/Core/RenderContext.h](../../../Source/Rendering/Core/RenderContext.h) | 유지 | 0 | 0 | 0 |
| [Source/Rendering/Core/RenderSettings.h](../../../Source/Rendering/Core/RenderSettings.h) | 예 | 0 | 0 | 0 |
| [Source/Rendering/Core/Renderer.cpp](../../../Source/Rendering/Core/Renderer.cpp) | 예 | 5 | 0 | 0 |
| [Source/Rendering/Core/Renderer.h](../../../Source/Rendering/Core/Renderer.h) | 유지 | 14 | 0 | 0 |
| [Source/Rendering/Core/RendererCommands.cpp](../../../Source/Rendering/Core/RendererCommands.cpp) | 예 | 7 | 0 | 0 |
| [Source/Rendering/Core/RendererSettings.cpp](../../../Source/Rendering/Core/RendererSettings.cpp) | 예 | 2 | 0 | 0 |
| [Source/Rendering/Core/RendererShared.h](../../../Source/Rendering/Core/RendererShared.h) | 예 | 6 | 0 | 0 |
| [Source/Rendering/Overlay/AccumulationOverlaySides.cpp](../../../Source/Rendering/Overlay/AccumulationOverlaySides.cpp) | 유지 | 3 | 0 | 0 |
| [Source/Rendering/Overlay/AccumulationOverlaySides.h](../../../Source/Rendering/Overlay/AccumulationOverlaySides.h) | 유지 | 0 | 0 | 0 |
| [Source/Rendering/Overlay/Geometry/TexelGeometryPreview.cpp](../../../Source/Rendering/Overlay/Geometry/TexelGeometryPreview.cpp) | 유지 | 3 | 0 | 0 |
| [Source/Rendering/Overlay/Geometry/TexelGeometryPreview.h](../../../Source/Rendering/Overlay/Geometry/TexelGeometryPreview.h) | 유지 | 0 | 0 | 0 |
| [Source/Rendering/Profiling/RendererBenchmark.cpp](../../../Source/Rendering/Profiling/RendererBenchmark.cpp) | 예 | 10 | 0 | 0 |
| [Source/Rendering/Profiling/RendererTiming.cpp](../../../Source/Rendering/Profiling/RendererTiming.cpp) | 예 | 1 | 0 | 0 |
| [Source/Rendering/Scene/FrustumCulling.h](../../../Source/Rendering/Scene/FrustumCulling.h) | 예 | 0 | 0 | 0 |
| [Source/Rendering/Scene/RendererMaterials.cpp](../../../Source/Rendering/Scene/RendererMaterials.cpp) | 예 | 2 | 0 | 0 |
| [Source/Rendering/Scene/WorldReferenceGeometry.h](../../../Source/Rendering/Scene/WorldReferenceGeometry.h) | 예 | 1 | 0 | 0 |
| [Source/Rendering/Surface/DemoSurfaceEffects.h](../../../Source/Rendering/Surface/DemoSurfaceEffects.h) | 유지 | 0 | 0 | 0 |
| [Source/Rendering/Surface/HeightFieldSmoothing.cpp](../../../Source/Rendering/Surface/HeightFieldSmoothing.cpp) | 유지 | 3 | 0 | 0 |
| [Source/Rendering/Surface/HeightFieldSmoothing.h](../../../Source/Rendering/Surface/HeightFieldSmoothing.h) | 유지 | 0 | 0 | 0 |
| [Source/Rendering/Surface/RenderStateTexture.cpp](../../../Source/Rendering/Surface/RenderStateTexture.cpp) | 유지 | 3 | 0 | 0 |
| [Source/Rendering/Surface/RenderStateTexture.h](../../../Source/Rendering/Surface/RenderStateTexture.h) | 유지 | 0 | 0 | 0 |
| [Source/Scene/Camera.cpp](../../../Source/Scene/Camera.cpp) | 유지 | 3 | 0 | 0 |
| [Source/Scene/Camera.h](../../../Source/Scene/Camera.h) | 유지 | 0 | 0 | 0 |
| [Source/Scene/DemoAnimation.cpp](../../../Source/Scene/DemoAnimation.cpp) | 유지 | 4 | 0 | 0 |
| [Source/Scene/DemoAnimation.h](../../../Source/Scene/DemoAnimation.h) | 유지 | 0 | 0 | 0 |
| [Source/Scene/Scene.cpp](../../../Source/Scene/Scene.cpp) | 유지 | 5 | 0 | 0 |
| [Source/Scene/Scene.h](../../../Source/Scene/Scene.h) | 유지 | 0 | 0 | 0 |
| [Source/Scene/SceneLoader.cpp](../../../Source/Scene/SceneLoader.cpp) | 유지 | 2 | 0 | 0 |
| [Source/Scene/SceneLoader.h](../../../Source/Scene/SceneLoader.h) | 유지 | 0 | 0 | 0 |
| [Source/Scene/StaticMeshInstance.cpp](../../../Source/Scene/StaticMeshInstance.cpp) | 유지 | 1 | 0 | 0 |
| [Source/Scene/StaticMeshInstance.h](../../../Source/Scene/StaticMeshInstance.h) | 유지 | 0 | 0 | 0 |
| [Source/Scene/Transform.h](../../../Source/Scene/Transform.h) | 유지 | 0 | 0 | 0 |
| [Source/SurfaceState/Cache/SurfaceCache.cpp](../../../Source/SurfaceState/Cache/SurfaceCache.cpp) | 유지 | 4 | 0 | 0 |
| [Source/SurfaceState/Cache/SurfaceCache.h](../../../Source/SurfaceState/Cache/SurfaceCache.h) | 유지 | 0 | 0 | 0 |
| [Source/SurfaceState/Contact/SurfaceContactApplication.cpp](../../../Source/SurfaceState/Contact/SurfaceContactApplication.cpp) | 예 | 1 | 0 | 0 |
| [Source/SurfaceState/Debug/DebugData.cpp](../../../Source/SurfaceState/Debug/DebugData.cpp) | 유지 | 0 | 0 | 0 |
| [Source/SurfaceState/Debug/DebugData.h](../../../Source/SurfaceState/Debug/DebugData.h) | 유지 | 0 | 0 | 0 |
| [Source/SurfaceState/Debug/TexelInspector.cpp](../../../Source/SurfaceState/Debug/TexelInspector.cpp) | 유지 | 3 | 0 | 0 |
| [Source/SurfaceState/Debug/TexelInspector.h](../../../Source/SurfaceState/Debug/TexelInspector.h) | 유지 | 0 | 0 | 0 |
| [Source/SurfaceState/GPU/Abi/SurfaceGPUAbi.h](../../../Source/SurfaceState/GPU/Abi/SurfaceGPUAbi.h) | 예 | 0 | 0 | 0 |
| [Source/SurfaceState/GPU/Abi/SurfaceGPUDescriptorBinding.h](../../../Source/SurfaceState/GPU/Abi/SurfaceGPUDescriptorBinding.h) | 예 | 0 | 0 | 0 |
| [Source/SurfaceState/GPU/Packing/ProfilePacking.cpp](../../../Source/SurfaceState/GPU/Packing/ProfilePacking.cpp) | 예 | 1 | 0 | 0 |
| [Source/SurfaceState/GPU/Packing/SharedGeometryPacking.cpp](../../../Source/SurfaceState/GPU/Packing/SharedGeometryPacking.cpp) | 예 | 1 | 0 | 0 |
| [Source/SurfaceState/GPU/Packing/SparseMetadataLayout.cpp](../../../Source/SurfaceState/GPU/Packing/SparseMetadataLayout.cpp) | 예 | 0 | 0 | 0 |
| [Source/SurfaceState/GPU/Packing/SurfaceGPUResourceLayout.h](../../../Source/SurfaceState/GPU/Packing/SurfaceGPUResourceLayout.h) | 유지 | 0 | 0 | 0 |
| [Source/SurfaceState/GPU/Packing/TransferWeightPacking.cpp](../../../Source/SurfaceState/GPU/Packing/TransferWeightPacking.cpp) | 예 | 1 | 0 | 0 |
| [Source/SurfaceState/GPU/Resources/DescriptorResources.cpp](../../../Source/SurfaceState/GPU/Resources/DescriptorResources.cpp) | 예 | 1 | 0 | 0 |
| [Source/SurfaceState/GPU/Resources/DescriptorResources.h](../../../Source/SurfaceState/GPU/Resources/DescriptorResources.h) | 유지 | 0 | 0 | 0 |
| [Source/SurfaceState/GPU/Resources/InstanceResources.cpp](../../../Source/SurfaceState/GPU/Resources/InstanceResources.cpp) | 예 | 1 | 0 | 0 |
| [Source/SurfaceState/GPU/Resources/InstanceResources.h](../../../Source/SurfaceState/GPU/Resources/InstanceResources.h) | 유지 | 0 | 0 | 0 |
| [Source/SurfaceState/GPU/Resources/ProfileResources.cpp](../../../Source/SurfaceState/GPU/Resources/ProfileResources.cpp) | 예 | 1 | 0 | 0 |
| [Source/SurfaceState/GPU/Resources/ProfileResources.h](../../../Source/SurfaceState/GPU/Resources/ProfileResources.h) | 유지 | 0 | 0 | 0 |
| [Source/SurfaceState/GPU/Resources/ResourceBufferHelpers.h](../../../Source/SurfaceState/GPU/Resources/ResourceBufferHelpers.h) | 예 | 0 | 0 | 0 |
| [Source/SurfaceState/GPU/Resources/SharedGeometryResources.cpp](../../../Source/SurfaceState/GPU/Resources/SharedGeometryResources.cpp) | 예 | 1 | 0 | 0 |
| [Source/SurfaceState/GPU/Resources/SharedGeometryResources.h](../../../Source/SurfaceState/GPU/Resources/SharedGeometryResources.h) | 유지 | 1 | 0 | 0 |
| [Source/SurfaceState/GPU/Resources/SurfaceGPUBufferPool.h](../../../Source/SurfaceState/GPU/Resources/SurfaceGPUBufferPool.h) | 예 | 0 | 0 | 0 |
| [Source/SurfaceState/GPU/Resources/SurfaceGPUResourceManager.cpp](../../../Source/SurfaceState/GPU/Resources/SurfaceGPUResourceManager.cpp) | 유지 | 3 | 0 | 0 |
| [Source/SurfaceState/GPU/Resources/SurfaceGPUResourceManager.h](../../../Source/SurfaceState/GPU/Resources/SurfaceGPUResourceManager.h) | 유지 | 0 | 0 | 0 |
| [Source/SurfaceState/GPU/Resources/SurfaceGPUResources.h](../../../Source/SurfaceState/GPU/Resources/SurfaceGPUResources.h) | 예 | 0 | 0 | 0 |
| [Source/SurfaceState/Geometry/MesoGeometryBuilder.cpp](../../../Source/SurfaceState/Geometry/MesoGeometryBuilder.cpp) | 유지 | 2 | 0 | 0 |
| [Source/SurfaceState/Geometry/MesoGeometryBuilder.h](../../../Source/SurfaceState/Geometry/MesoGeometryBuilder.h) | 유지 | 0 | 0 | 0 |
| [Source/SurfaceState/Geometry/SharedSurfaceGeometryData.cpp](../../../Source/SurfaceState/Geometry/SharedSurfaceGeometryData.cpp) | 유지 | 2 | 0 | 0 |
| [Source/SurfaceState/Geometry/SharedSurfaceGeometryData.h](../../../Source/SurfaceState/Geometry/SharedSurfaceGeometryData.h) | 유지 | 0 | 0 | 0 |
| [Source/SurfaceState/Geometry/SurfaceGeometryBuilder.cpp](../../../Source/SurfaceState/Geometry/SurfaceGeometryBuilder.cpp) | 유지 | 1 | 0 | 0 |
| [Source/SurfaceState/Geometry/SurfaceGeometryBuilder.h](../../../Source/SurfaceState/Geometry/SurfaceGeometryBuilder.h) | 유지 | 0 | 0 | 0 |
| [Source/SurfaceState/Geometry/SurfaceGeometryUpdate.cpp](../../../Source/SurfaceState/Geometry/SurfaceGeometryUpdate.cpp) | 유지 | 0 | 0 | 0 |
| [Source/SurfaceState/Geometry/SurfaceGeometryUpdate.h](../../../Source/SurfaceState/Geometry/SurfaceGeometryUpdate.h) | 유지 | 0 | 0 | 0 |
| [Source/SurfaceState/Geometry/SurfaceTexelMeshBuilder.cpp](../../../Source/SurfaceState/Geometry/SurfaceTexelMeshBuilder.cpp) | 유지 | 2 | 0 | 0 |
| [Source/SurfaceState/Geometry/SurfaceTexelMeshBuilder.h](../../../Source/SurfaceState/Geometry/SurfaceTexelMeshBuilder.h) | 유지 | 0 | 0 | 0 |
| [Source/SurfaceState/Mapping/NormalMapTransferNormalBuilder.cpp](../../../Source/SurfaceState/Mapping/NormalMapTransferNormalBuilder.cpp) | 유지 | 2 | 0 | 0 |
| [Source/SurfaceState/Mapping/NormalMapTransferNormalBuilder.h](../../../Source/SurfaceState/Mapping/NormalMapTransferNormalBuilder.h) | 유지 | 0 | 0 | 0 |
| [Source/SurfaceState/Mapping/SurfaceMappingBuilder.cpp](../../../Source/SurfaceState/Mapping/SurfaceMappingBuilder.cpp) | 유지 | 2 | 0 | 0 |
| [Source/SurfaceState/Mapping/SurfaceMappingBuilder.h](../../../Source/SurfaceState/Mapping/SurfaceMappingBuilder.h) | 유지 | 0 | 0 | 0 |
| [Source/SurfaceState/Mapping/SurfaceMappingData.h](../../../Source/SurfaceState/Mapping/SurfaceMappingData.h) | 유지 | 0 | 0 | 0 |
| [Source/SurfaceState/Mapping/SurfaceMappingValidation.cpp](../../../Source/SurfaceState/Mapping/SurfaceMappingValidation.cpp) | 유지 | 2 | 0 | 0 |
| [Source/SurfaceState/Mapping/SurfaceMappingValidation.h](../../../Source/SurfaceState/Mapping/SurfaceMappingValidation.h) | 유지 | 0 | 0 | 0 |
| [Source/SurfaceState/Runtime/SurfaceDataManager.cpp](../../../Source/SurfaceState/Runtime/SurfaceDataManager.cpp) | 유지 | 3 | 0 | 0 |
| [Source/SurfaceState/Runtime/SurfaceDataManager.h](../../../Source/SurfaceState/Runtime/SurfaceDataManager.h) | 유지 | 0 | 0 | 0 |
| [Source/SurfaceState/Runtime/SurfaceRuntimeData.cpp](../../../Source/SurfaceState/Runtime/SurfaceRuntimeData.cpp) | 유지 | 1 | 0 | 0 |
| [Source/SurfaceState/Runtime/SurfaceRuntimeData.h](../../../Source/SurfaceState/Runtime/SurfaceRuntimeData.h) | 유지 | 0 | 0 | 0 |
| [Source/SurfaceState/Solver/SimulationClock.h](../../../Source/SurfaceState/Solver/SimulationClock.h) | 유지 | 0 | 0 | 0 |
| [Source/SurfaceState/Solver/SurfaceStateSolver.cpp](../../../Source/SurfaceState/Solver/SurfaceStateSolver.cpp) | 유지 | 4 | 0 | 0 |
| [Source/SurfaceState/Solver/SurfaceStateSolver.h](../../../Source/SurfaceState/Solver/SurfaceStateSolver.h) | 유지 | 1 | 0 | 0 |
| [Source/SurfaceState/Solver/SurfaceStepRecording.cpp](../../../Source/SurfaceState/Solver/SurfaceStepRecording.cpp) | 예 | 1 | 0 | 0 |
| [Source/SurfaceState/Solver/SurfaceTimestep.cpp](../../../Source/SurfaceState/Solver/SurfaceTimestep.cpp) | 예 | 1 | 0 | 0 |
| [Source/SurfaceState/State/SurfaceInput.h](../../../Source/SurfaceState/State/SurfaceInput.h) | 유지 | 0 | 0 | 0 |
| [Source/SurfaceState/State/SurfaceInstanceStateData.cpp](../../../Source/SurfaceState/State/SurfaceInstanceStateData.cpp) | 유지 | 2 | 0 | 0 |
| [Source/SurfaceState/State/SurfaceInstanceStateData.h](../../../Source/SurfaceState/State/SurfaceInstanceStateData.h) | 유지 | 0 | 0 | 0 |
| [Source/SurfaceState/SurfaceStateSystem.cpp](../../../Source/SurfaceState/SurfaceStateSystem.cpp) | 유지 | 3 | 0 | 0 |
| [Source/SurfaceState/SurfaceStateSystem.h](../../../Source/SurfaceState/SurfaceStateSystem.h) | 유지 | 6 | 0 | 0 |
| [Source/SurfaceState/Types/SurfaceMappingTypes.h](../../../Source/SurfaceState/Types/SurfaceMappingTypes.h) | 유지 | 0 | 0 | 0 |
| [Source/SurfaceState/Types/SurfaceReactions.cpp](../../../Source/SurfaceState/Types/SurfaceReactions.cpp) | 예 | 0 | 0 | 0 |
| [Source/SurfaceState/Types/SurfaceSolverRates.h](../../../Source/SurfaceState/Types/SurfaceSolverRates.h) | 유지 | 0 | 0 | 0 |
| [Source/SurfaceState/Types/SurfaceStateRegistry.cpp](../../../Source/SurfaceState/Types/SurfaceStateRegistry.cpp) | 유지 | 2 | 0 | 0 |
| [Source/SurfaceState/Types/SurfaceStateRegistry.h](../../../Source/SurfaceState/Types/SurfaceStateRegistry.h) | 유지 | 0 | 0 | 0 |
| [Source/SurfaceState/Types/SurfaceStateTypes.cpp](../../../Source/SurfaceState/Types/SurfaceStateTypes.cpp) | 유지 | 3 | 0 | 0 |
| [Source/SurfaceState/Types/SurfaceStateTypes.h](../../../Source/SurfaceState/Types/SurfaceStateTypes.h) | 유지 | 0 | 0 | 0 |
| [Source/main.cpp](../../../Source/main.cpp) | 유지 | 1 | 0 | 0 |
| [Tests/NormalMapTransferTests.cpp](../../../Tests/NormalMapTransferTests.cpp) | 유지 | 6 | 0 | 0 |
| [Tests/SceneResourceTests.cpp](../../../Tests/SceneResourceTests.cpp) | 유지 | 7 | 0 | 0 |
| [Tests/SharedSurfaceGeometryTests.cpp](../../../Tests/SharedSurfaceGeometryTests.cpp) | 유지 | 9 | 0 | 0 |
| [Tests/SimulationTransportTests.cpp](../../../Tests/SimulationTransportTests.cpp) | 유지 | 5 | 0 | 0 |
| [Tests/SurfaceCacheTests.cpp](../../../Tests/SurfaceCacheTests.cpp) | 유지 | 4 | 0 | 0 |
| [Tests/SurfaceDebugRenderingTests.cpp](../../../Tests/SurfaceDebugRenderingTests.cpp) | 유지 | 4 | 0 | 0 |
| [Tests/SurfaceGPUResourceTests.cpp](../../../Tests/SurfaceGPUResourceTests.cpp) | 유지 | 19 | 0 | 0 |
| [Tests/SurfaceMappingTests.cpp](../../../Tests/SurfaceMappingTests.cpp) | 유지 | 11 | 0 | 0 |
| [Tests/SurfaceStateContractTests.cpp](../../../Tests/SurfaceStateContractTests.cpp) | 유지 | 7 | 0 | 0 |
| [run.ps1](../../../run.ps1) | 예 | 0 | 0 | 0 |
| [run.sh](../../../run.sh) | 예 | 0 | 0 | 0 |

별도 구성 파일: `CMakeLists.txt`, `CMake/ShaderManifest.cmake`, `Scripts/Research/MesoStudy.cmake`, `Scripts/Research/ModularShaders.cmake`. 역할 주석 한 줄 외 기존 바이트 동일.
