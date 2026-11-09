# Contact · State · Debug Readback

사용자 입력·Scene event가 `InputDelta`로 변환되어 Pass 2에서 소비되는 경로와 Debug readback을 추적한다.

**연결 문서**
- [[02_Architecture/00_Flows/01_Frame/01_Contact-to-InputDelta|Contact Flow]]
- [[02_Architecture/02_Source/00_CPP/05_Solver|Solver]]

## Debug readback

| 파일 | 주요 선언·코드 | 역할과 데이터 이동 |
|---|---|---|
| [DebugData.h](../../../../../../Source/SurfaceState/Debug/DebugData.h) | CPU debug selection·readback 자료 | Debug Renderer와 Texel Inspector가 공유하는 표시 metadata |
| [DebugData.cpp](../../../../../../Source/SurfaceState/Debug/DebugData.cpp) | debug data 초기화·전달 | Scene/renderer 쪽 debug 상태 준비 |
| [TexelInspector.h](../../../../../../Source/SurfaceState/Debug/TexelInspector.h) | selection/snapshot 형식, `TTexelInspector::Record` | texel 하나의 Solver 입력·파생량을 GPU에서 복사하는 readback API |
| [TexelInspector.cpp](../../../../../../Source/SurfaceState/Debug/TexelInspector.cpp) | compute dispatch·staging readback | `Debug/TexelInspector.comp`로 선택 texel의 계산 항목을 snapshot buffer에 기록 |

검증 후보(이번 첨부에 없는 파일): `Tests/SurfaceMappingTests.cpp`, `Tests/SharedSurfaceGeometryTests.cpp`, `Tests/NormalMapTransferTests.cpp`, `Tests/SurfaceStateContractTests.cpp`, `Tests/SceneResourceTests.cpp`에서 시작한다. Contact 크기와 적용 시점은 [[02_Architecture/01_Contracts/01_State-and-Profile#Contact Input|Contact Input]], geometry 의미는 [[02_Architecture/01_Contracts/03_Geometry|Geometry]]가 기준이다.

## Contact가 실제 입력이 되는 지점

`SurfaceStateSystem.cpp`의 `SubmitContact`는 요청을 queue에 보관한다. `SurfaceContactApplication.cpp`의 `ApplyPendingContacts`가 실행 step 전에 대상 instance·State·반경·강도·shape의 유효성을 검사하고, simulation mapping 또는 월드 위치를 이용해 영향을 받는 texel을 찾는다. texel별 기여를 instance의 `InputDelta`에 누적하며 sparse 실행에 필요한 입력 group과 channel mask도 준비한다. `SurfaceStepRecording.cpp`가 이 입력 활성화를 Solver에 전달하고 Pass 2가 delta를 한 번 소비한다.

| 관찰된 현상 | 확인할 코드와 자료 |
|---|---|
| 입력이 다른 texel에 도달 | `Raycaster.cpp`의 hit UV → `SurfaceMappingBuilder.cpp`의 texel 대응 → `SurfaceContactApplication.cpp`의 영향 범위 |
| seam 근처 전달 누락 | `SurfaceMappingBuilder.cpp`의 seam 연결 → `ValidateSurfaceMapping` → packed neighbor record |
| geometry 변경 뒤 결과 차이 | 정적 geometry → world area/weight packing → 동적 geometry shader의 다음 step 반영 |
| reset 후 상태 | `SurfaceStateSystem.cpp`의 `ResetState`/`RestartState`, Scene initial contact 재등록 |

## ApplyPendingContacts의 정확한 예외·업로드 경계

[SurfaceContactApplication.cpp](../../../../../../Source/SurfaceState/Contact/SurfaceContactApplication.cpp)에 따라:

- 빈 queue 또는 Registry channel 0이면 조기 종료한다. Contact별 Instance·State·WorldPosition·Radius·Strength·Falloff·shape/planar 관련 유효성을 확인하고 잘못된 요청은 경고 후 건너뛴다.
- `bHasSimulationMapping`이면 hit triangle과 Simulation UV로 Surface Range의 중심 texel을 계산한다. 그 texel이 hit triangle에 해당하지 않으면 제한된 범위에서 같은 triangle의 유효 texel을 찾고, 실패하면 diagnostic을 출력하며 접촉을 거절한다. **UV만 가까운 다른 triangle에 임의로 주입하지 않는다.**
- 영향을 받는 texel은 instance model transform을 사용한 world position 및 입력 도형(Circle/기타 shape, planar option)에 따라 선택한다. Texel별 Profile이 해당 State를 지원하는지 확인하고 `InputFactor`·RuntimeProfileOverride를 적용한다.
- instance별 CPU `InputDeltas`, `InputGroupFlags`, `InputGroupMasks`, `InputChannelMasks`를 만들고 GPU InputDelta/CPU-known group metadata를 올린다. Sparse scheduling에서 입력이 누락되면 이 mask 업로드 경로를 먼저 본다.
- Pass 2 shader는 `InputDelta.Values[StateValueIndex]`를 읽고 0으로 지운다. [SurfaceSolverPass2.comp](../../../../../../Shaders/Simulation/Solver/SurfaceSolverPass2.comp).

**구현 확인:** CPU 입력 처리 자체를 수행하는 GPU `SurfaceInput.comp` dispatch는 이번 소스에서 확인되지 않았다. `SurfaceInput.comp`는 placeholder 내용이며 현재 Contact 경로는 위 C++ 구현이다.
