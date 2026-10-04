# 성능 실험 기록

> **한 줄 요약:** 성능 실험의 조건·실행 정보·원자료를 한 형식으로 남겨 서로 재현하고 비교한다.

- 상태: **기록 체계 및 첫 GPU benchmark 경로 구현**
- 마지막 확인: 2026-10-04

## 목적과 위치

이 디렉터리는 성능 실험의 공통 실행 조건과 기계 판독 가능한 run 기록을 관리한다. 가설, 비교 설계, 결과 해석은 기존 `../Experiments/`의 실험 문서에 기록하고, 여기에는 반복 실행의 원자료와 재현 정보를 둔다. 실험 설명을 두 군데에 복제하지 않는다.

기존 실험은 계속 `../Experiments/`에서 관리한다. 예를 들어 [[../Experiments/0003_Pass1-Cost-Analysis|Pass 1 비용 분석]]은 원인 분석·해석을, 여기의 run 자료는 각 측정 회차의 상세값을 담당한다.

## 기록 흐름

1. 실험 문서에서 비교 가설, 한 번에 바꿀 변수, 성공 기준을 정한다.
2. 각 조건을 별도 run으로 실행한다. 비교 조건 사이에 Scene·입력·해상도·Profile·Solver 설정을 고정하고, 순서를 번갈아 편향을 줄인다.
3. 준비 구간을 버린 뒤 고정 Solver step 수만큼 원자료를 수집한다. 기본 matrix는 run당 30 warm-up frame과 180 measurement frame을 사용한다.
4. 코드 revision과 변경 상태, 실행 환경, Scene 및 Solver 조건, 측정값을 같은 run 기록에 남긴다.
5. 결과 해석에는 반복별 값과 중앙값·산포를 제시한다. 평균 FPS 한 값만으로 개선을 결론내리지 않는다.
6. 개선 후보는 동일한 조건에서 기준 버전과 번갈아 실행하고, 결과 정확도·보존성 검사도 함께 기록한다.

## 현재 수집 가능한 값과 한계

benchmark CLI는 프레임별 GPU Solver 총시간, Pass 1·2, geometry feedback, transfer weight 시간과 workload 메타데이터를 JSONL로 저장한다. 앱은 run마다 정확히 1/60초의 고정 simulation step을 실행하고 Scene animation을 일시정지한다. Vulkan timestamp query를 지원하지 않는 장치에서는 benchmark를 시작하지 않고 오류를 반환한다.

Renderer의 Solver 및 Pass 시간은 한 프레임에 실행된 모든 Solver step과 instance/dispatch 구간의 합계다. 기본 benchmark 설정은 1 frame당 1 step이지만 instance 수에 따른 timestamp를 합산하므로 Mountain 행은 세 instance 합계다. UI가 표시하는 1초 평균·구간 최댓값은 자동 보고서의 per-frame median·p95와 다른 통계다.

## 측정 조건 체크리스트

- Build configuration 및 profiling/validation 설정
- Git commit, dirty 여부, 변경 diff 식별 정보
- GPU 이름, OS, Vulkan/MoltenVK 및 드라이버 버전
- Scene, mesh instance 수, simulation resolution, Registry channel 수
- Profile, State 분포, Input 상태, 실행할 frame/시뮬레이션 시간
- Fixed timestep, Auto substepping, max step 설정, 실제 Solver step 수
- render/overlay/debug 설정 및 cache warm/cold 조건
- warm-up·측정 구간, 반복 횟수, 실행 순서
- frame time, Solver/Pass GPU 시간 및 필요 시 관련 메모리
- 정확도·총량 보존·NaN/Inf 등 결과 유효성 지표

조건이 실험에 해당하지 않으면 `N/A`로 남긴다. 성능 수치에는 단위와 집계 범위를 쓴다.

## 준비된 Scene 선택

현재 세 `.Scene`은 모두 기본 256×256이고, 해당 profile map의 SRProfile은 각자 wetness·mud·waterfilm·lava 네 state를 정의한다. 따라서 장면 간 측정 비교는 같은 workload가 아니며, 목적에 따라 Scene을 선택한다.

| Scene | 구성 | 적합한 용도 | 주의점 |
|---|---|---|---|
| `Assets/Scenes/BrickCube.Scene` | Cube instance 1개, Surface 6개, OBJ face 12개, lava initial contact 1개 | **기본 GPU Solver workload**. 다중 Surface와 4 state channel을 가진 일반 baseline | 단순 형상이라 복잡한 mesh mapping/normal 비용 대표성은 낮다. 다른 두 Scene보다 텍셀 슬롯 수가 많다. |
| `Assets/Scenes/Mountain.Scene` | 같은 Mountain mesh의 instance 3개, instance마다 Surface 1개, lava initial contact 3개 | **다중 instance workload**. instance 합산 비용과 다른 회전/기울기 변환을 볼 때 | Mountain OBJ는 3,481 face record로 Bunny보다 작다. 3 instance의 합계를 단일 instance 값처럼 해석하지 않는다. |
| `Assets/Scenes/Bunny.Scene` | Bunny instance 1개, Surface 1개, 원본 PLY 69,451 face, lava initial contact 1개 | **복잡 mesh 타깃 workload**. mesh/UV mapping, geometry, render 경로 차이를 볼 때 | 단일 instance·Surface라 다중 Surface/instance의 GPU workload를 대표하지 않는다. 현재 OBJ는 원본 Stanford scan에서 UV atlas를 생성한 mesh다. |

기본 scene set은 [`Config/Performance/scene-set.json`](../../../../Config/Performance/scene-set.json)에서 관리한다. `BrickCube`를 solver baseline으로, `Mountain`을 multi-instance workload로 사용한다. `Bunny`는 mesh 복잡도나 전처리/렌더링 비용이 가설에 포함된 실험에만 추가한다. 모든 Scene을 매 matrix마다 무조건 돌려 조합 수를 늘리지 않는다.

세 Scene 모두 lava initial contact를 포함하므로 State가 비어 있는 workload가 아니다. Benchmark 모드는 이 접촉에서 매 실행 동일하게 시작하고 animation을 일시정지한다. CLI에서 해상도 override를 할 수 있으며 `.Scene` 파일은 수정하지 않는다.

## 자동화 단계

### 실행 방법

저장소 루트에서 아래 명령으로 기본 matrix(BrickCube/Mountain × 128/256/512, 각각 3회)를 실행한다. Release로 빌드하고 Vulkan validation layer를 끈다.

```sh
Scripts/run_performance_benchmarks.sh
```

기존 실행 파일을 사용할 때는 `--skip-build`를 지정한다. 조건은 `Config/Performance/solver-resolution-matrix.json`, 씬 역할과 경로는 `Config/Performance/scene-set.json`에서 수정한다. 실행 계획만 확인하려면 `--dry-run`을 지정한다.

앱 단독 실행에는 다음 옵션을 쓴다.

```sh
Build/bin/MDSS --benchmark-scene Assets/Scenes/BrickCube.Scene \
  --benchmark-resolution 256 --benchmark-warmup-frames 30 \
  --benchmark-measure-frames 180 \
  --benchmark-output /tmp/brickcube-frames.jsonl
```

각 run set은 `manifest.json`, `raw/*.jsonl`, 앱 출력 `raw/*.log`, 자동 생성 `summary.md`를 둔다. summary만 다시 만들려면 `python3 Scripts/Performance/summarize.py --manifest <run-set>/manifest.json`을 실행한다. 정식 가설·실험 결론은 기존 `../Experiments/` 문서에 기록한다.

## 구현할 파일

자동화 파일과 첫 수직 구현의 현재 범위는 다음과 같다.

| 파일 | 추가 / 수정 | 책임 |
|---|---|---|
| `Config/Performance/solver-resolution-matrix.json` | 추가됨 | 씬, 128/256/512 해상도, 반복 수, warm-up/측정 frame, Release/validation 설정 |
| `Config/Performance/scene-set.json` | 추가됨 | BrickCube/Mountain 씬 ID, 파일 경로와 workload 역할 |
| `Source/Application/BenchmarkOptions.h/.cpp` | 추가됨 | benchmark CLI 인자 검증 및 frame-limit 계산 |
| `Source/main.cpp` | 수정됨 | benchmark scene·resolution·capture 옵션 처리. 기존 `--frames` 유지 |
| `Source/Scene/SceneLoader.h/.cpp` | 수정됨 | `.Scene` 원본을 바꾸지 않고 로딩 시 해상도 override 적용 |
| `Source/Application/Application.h/.cpp` | 수정됨 | scene 선택, fixed-step benchmark loop 및 animation 고정 |
| `Source/DebugUI/DebugUI.h` | 수정됨 | fixed timestep ON·Auto substepping OFF·time scale 1 benchmark mode |
| `Source/Rendering/Renderer.h/.cpp` | 수정됨 | GPU timestamp와 해당 submitted frame의 step/workload를 JSONL로 기록 |
| `Scripts/run_performance_benchmarks.sh` | 추가됨 | matrix 자동화 실행기를 시작하고 인자를 전달하는 최상위 쉘 진입점 |
| `Scripts/Performance/run_matrix.py` | 추가됨 | build, scene/resolution/repeat 순회, revision/environment manifest 및 raw 수집 |
| `Scripts/Performance/summarize.py` | 추가됨 | raw 자료에서 median/p95/반복 범위를 Markdown으로 생성 |
| `Docs/MDSSP-Vault/06_Development/Performance/Results/` | 자동 생성 | 각 실행의 manifest, JSONL 원자료, 앱 로그, summary |

현재 첫 수직 구현은 **Scene + 해상도 + 고정 simulation step 수**를 지원한다. Matrix에서 solver term/cache toggle, 임의 초기 State, 재생 가능한 접촉 input을 아직 바꿀 수 없다. Solver 비교에서 해당 조건이 필요한 경우 다음 확장 항목이다.

결과 디렉터리의 run에는 `manifest.json`(revision, dirty diff hash, GPU/runtime, matrix condition), `raw/*.jsonl`(프레임별 GPU/workload), `raw/*.log`(앱 출력), `summary.md`(자동 생성)를 둔다. run 결과는 재생성할 수 있도록 함께 보존한다.

## 결과 해석 규칙

- 기준과 후보를 같은 머신·빌드·Scene에서 번갈아 실행한다.
- 초기 로딩·shader compile·cache 구축 시간은 steady-state 프레임 시간과 분리한다.
- 중앙값과 p95를 기본으로 보고, 반복 간 범위 또는 IQR을 함께 기록한다.
- 속도 향상과 함께 총량 오차·분포 오차·메모리 사용량을 비교한다.
- 한 run에서 나온 개선을 일반적인 개선률로 표현하지 않는다.
- UI 수동 측정은 `capture_method: "ui_manual"`로 표시한다. 자동 실행 결과와는 별도 자료로 유지한다.

## 관련 자료

- [[../Experiments/0003_Pass1-Cost-Analysis|Pass 1 비용 분석]]
- [[../Experiments/0004_RawFlux-Cache-Comparison|RawFlux cache 비교]]
- [[../Experiments/0005_Resolution-and-Timestep-Dependence|해상도·시간 간격 실험 계획]]
- [[../../04_Architecture/0007_Simulation-Optimization|Simulation Optimization]]
