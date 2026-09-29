# Scene Resource 통합 테스트

상태: **통과** · 구현: `Tests/SceneResourceTests.cpp` · CTest: `MDSS_SceneResources`

관련 결정: [[05_ADR/0027-Scene-State-Registry-and-Shared-Profile-Table|ADR 0027]]

작은 Native GLFW window와 실제 Vulkan context에서 SceneLoader·AssetManager·Renderer·SurfaceStateSystem의 자원 수명을 검증한다. 테스트는 임시 디렉터리에 최소 OBJ·`.SRProfile`·`.SurfaceProfileMap`·`.Scene`을 생성해 실제 loader로 읽고 종료 시 입력 파일을 제거한다. `.Surface` 캐시는 빌드 디렉터리의 `SceneTestCache`에 둔다.

| 조건 | 기대 결과 |
|---|---|
| 외부 Profile을 먼저 캐시한 뒤 Wetness Scene 시작 | 현재 Scene의 Wetness 채널만 Registry에 포함 |
| 다음 Scene의 프로파일 로드 | 활성 Registry의 ID 유지 |
| Wetness → Mud Scene 전환 | Registry를 Mud 채널 하나로 교체 |
| 서로 다른 Mesh·Map 및 역순 로컬 Profile 테이블을 사용하는 mixed Scene | 고유 Profile 두 개를 한 GPU 테이블에 저장하고 모든 descriptor가 같은 Parameters·Supported buffer에 연결 |
| 동일 Profile의 Runtime 튜닝 | 단일 공유 record 갱신 |
| 새 Scene으로 전환 | 이전 숫자 State ID 기반 override 제거 |
| Wetness 지원 Mesh 두 개와 Mud Mesh에 Wetness 주입 | 로컬 Profile 순서와 무관하게 Wetness Mesh에만 입력 반영, instance State 독립 유지 |
| 동일 Scene 해상도 변경 | Registry ID와 Profile 튜닝 유지 |
| storage-buffer range를 초과하는 Scene 로드 | 이전 Registry·GPU handle·튜닝 값 복원 |
| 빈 Scene 및 이전 Scene으로 재전환 | 빈 Registry 처리 및 실패한 로드의 캐시 State 제외 |
| 생성·Compute·파괴 | Vulkan validation error 없음 |

`Tests/SurfaceDebugRenderingTests.cpp`는 같은 Vulkan context에서 실제 `SurfaceDebug.vert`·`SurfaceDebug.frag`를 float32 RGBA offscreen attachment에 렌더링하고 GPU 출력 픽셀을 읽는다. 삼각형 비율 계산을 CPU에서 반복하는 대신 알려진 단위 정사각형의 표시 결과를 검증한다.

| 렌더 조건 | 기대 결과 |
|---|---|
| 단위 정사각형, 기준 `1 / 256²`, 해상도 128·256·512 | 각각 빨강·초록·파랑 |
| 동일 화면 크기를 유지한 월드 2배 확대 | 텍셀당 면적 4배로 빨강 |
| 비균일 scale `(2, 0.5, 3)`의 XY 표면 | 표면 면적은 같으므로 초록 |
| 원근 투영 및 비스듬한 Camera | 기준 면적 색 유지 |
| UV의 두 축을 1/2로 축소 | 텍셀당 면적 4배로 빨강 |
| 반전 UV / 퇴화 UV | 정상 면적 / 계산 불가 분홍색 |
| 격자 8×8 → 16×16 변경 | 개별 텍셀 경계 위치를 유지하고 묶음 경계 변경 |
| 화면에서 구분 불가능한 격자 | 평균 중립색으로 축소 패턴 숨김 |
| invalid simulation texel | 두 기하 진단 뷰는 원본 mesh를 계속 표시 |
| 실제 Renderer·ImGui frame에서 Grid·Area·Transfer Weight·Meso Displacement 표시 | pipeline·UI context 정상 동작, Vulkan validation error 없음 |
| 해상도 128 → 256 → 128 | Grid 묶음 크기와 Area 색 기준 유지 |

Scene 전환은 Loader와 Renderer API로 실행한다. UI 파일 다이얼로그의 클릭 동작은 이 CTest의 검증 범위에 포함하지 않는다. Native window 또는 Vulkan context를 만들 수 없는 환경에서는 return code 77로 skip한다.

```sh
cmake --build Build --parallel 4
ctest --test-dir Build --output-on-failure
```

2026-09-29 로컬 실행에서 Scene Resource 통합 테스트를 포함한 CTest 7개가 통과했다.
