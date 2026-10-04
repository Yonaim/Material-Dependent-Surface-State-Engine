# Development History

> **한 줄 요약:** 현재 Architecture에 흡수된 과거 개발 작업을 날짜별 요약으로만 보존한다.

## 2026-09-29 ~ 2026-09-30

- 작업 구간: 2026-09-29 14:19부터 2026-09-30 03:08까지 (KST)
- 범위: 시뮬레이션 단위·시간 처리, 디버그 표시 형상, Lit 데모, 관련 UI와 문서

### 시뮬레이션과 Profile

- Scene별 State Registry와 Profile GPU 자원 수명을 정리했다. State 종류와 ID는 로드된 `.SRProfile`에서 결정하며, Wetness·Heat·Burn·Mud는 데모 예시로 유지한다.
- `.SRProfile` version 2에 정규화 전달 계수를 도입하고 셰이더 파일을 역할별 디렉터리로 정리했다.
- State는 텍셀별 총량으로 유지한다. 텍셀 면적을 기준 면적과 비교해 Capacity, 외부 입력, Decay를 환산한다.
- Geometry 전달에 출발 텍셀의 `State / Capacity`를 곱한다. 이 비율은 1에서 자르지 않고 SaturationDrive는 별도 항으로 유지한다.
- 실제 경과 시간을 누적해 Solver를 반복하고, 각 반복은 바로 앞 step에서 갱신한 State를 읽는다. 한 frame에서 처리하지 못한 시간은 다음 frame으로 넘기며 frame당 Solver 횟수는 최대 8회다.
- ㅊ`을 독립 옵션으로 분리했다. 기본은 Fixed timestep ON, Auto substepping OFF이며 고정 구간은 `1/60초`다. Auto substepping을 켜면 전달 제한에 맞춰 고정 구간을 더 잘게 계산한다.
- Geometry 기준 전달률을 100에서 6000으로 재보정하고, GPU Solver와 CPU 안전 간격 계산이 공용 상수를 사용하도록 했다. 기본 전달 계수 0.5 조건의 평면 시험에서 128·256·512 해상도 이동률을 비교했다.

### 디버그 표시와 적층 형상

- Raw State 표시와 Accumulation, Final Geometry 디버그 뷰를 추가했다. 적층 미리보기는 기준 면적에 맞춘 State 총량에서 높이 성분을 계산하며 실제 State를 바꾸지 않는다.
- Texel Inspector에서 선택한 texel의 State·높이·형상 계산 결과를 GPU에서 확인할 수 있게 했다.
- texel 중심을 포함하는 연결면을 만들어 높이 변화가 표면 실루엣에 보이도록 했다.
- 초기 UV chart별 연결에서 원본 메시의 triangle·정점·edge topology를 따르는 방식으로 발전시켜 UV seam을 봉합하고 원본 메시의 실제 열린 경계는 유지했다.
- 높이 표시 면에 texel 묶음 Grid를 추가했다. Grid는 색상·높이 표시용이며 삼각형 topology나 Solver 연결을 바꾸지 않는다.

### Wetness·Mud·WaterFilm 데모 렌더링

- 현재 Registry에서 `wetness`, `mud`, `waterfilm` 이름의 State ID를 찾아 데모 셰이더에 연결했다. 고정된 전역 채널 번호는 도입하지 않았다.
- 표시용 State 샘플은 면적 보정 Capacity를 기준으로 계산한다. 이 표시 변환은 GPU State와 Solver 전달량을 수정하지 않는다.
- Wetness는 색을 어둡게 하고 roughness를 낮춘다. Mud는 갈색 피복과 높이 표시를 적용하며, Mud와 Wetness가 함께 있으면 Wetness 반응을 이어 적용한다.
- WaterFilm 전용 `.SRProfile`, Surface Profile Map, `.Scene`과 Lit 효과를 추가했다.
- 반사광을 포함한 공통 Lit 경로를 만들고 State 샘플링, 효과별 재질 변화, 조명 계산을 모듈로 나눴다.
- Mud·WaterFilm 표시는 계산된 texel 형상을 사용한다. 이 형상은 렌더링 미리보기이며 Solver에 되먹임되지 않는다.

### UI와 사용성

- 해상도 선택, 텍셀 Grid·면적 진단, 시뮬레이션 제어를 UI에서 조작하고 확인할 수 있게 했다.
- 카메라 조작과 Scene별 데모 구성을 보완했다.
- Scene 선택 대화상자를 연 시간을 시뮬레이션 경과 시간에서 제외했다.
- 적용된 Surface contact 입력을 로그에 남기도록 했다.

### 검증과 남은 범위

- 관련 기록은 전체 빌드와 CTest 8개 통과, Vulkan validation 오류 없음으로 정리되어 있다. GPU 테스트에는 면적·총량 보존, cache ON/OFF, 적층 표시, Inspector readback, seam과 Lit 효과 검증이 포함된다.
- 대표 Scene의 최종 렌더링 성능과 LOD는 별도 측정이 필요하다. topology 기반 연결면은 삼각형 수와 GPU 메모리 사용량이 늘 수 있다.
- 물리적 다중 layer 합성, Surface별 실제 높이 기준, Solver의 동적 Geometry 피드백은 구현 범위에 포함되지 않는다.
- 범용 State ID→렌더 효과 매핑은 후속 설계로 남겨 두고, 현재 Lit 데모는 Wetness·Mud·WaterFilm을 명시적으로 연결한다.
- 이 노트를 작성한 시점의 작업 트리에는 `Shaders/Rendering/SurfaceLit.glsl`의 미커밋 변경이 있다. 계산된 변위 법선에 Normal Map의 접선 공간 세부 법선을 다시 적용하는 작업이다.

### 관련 문서

- Surface State
- Surface State Update
- Surface State Rendering
- [[0009_Texel-Area-and-State-Amounts|Decision 0009: Texel 면적과 State 총량]]
- [[0013_Fixed-Timestep-and-Auto-Substepping|Decision 0013: Fixed timestep과 Auto substepping]]
- [[0014_Accumulation-Debug-and-Texel-Inspector|Decision 0014: 적층 디버그와 Texel Inspector]]
- [[0015_Texel-Geometry-Preview|Decision 0015: Texel 연결면 미리보기]]
- [[0016_Texel-Grid-and-Demo-Lit-Effects|Decision 0016: Grid와 데모 Lit 효과]]
- [[0017_Source-Topology-Seam-Stitching|Decision 0017: Source topology 기반 seam 봉합]]

셰이더 핫패스 최적화의 상세 결과는 Solver Performance로 통합했다.
