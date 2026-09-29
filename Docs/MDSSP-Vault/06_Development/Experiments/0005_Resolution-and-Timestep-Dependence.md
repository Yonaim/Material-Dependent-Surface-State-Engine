# 실험 초안 — 해상도와 시간 간격에 따른 Transport

> **한 줄 요약:** 128·256·512에서 동일한 월드 초기조건과 시뮬레이션 시간을 비교해 거리 가중치, 면적, 시간 간격의 영향을 분리한다.

- 상태: **계획 / 미실시**
- 작성·마지막 소스 확인: 2026-09-29
- 범위: 현재 Solver의 기준 측정, 공간·시간 의존성 분석, 후속 후보 비교 설계
- 제외: 이번 초안에서 Solver 수식·단위·스키마 변경, 알고리즘 채택, 실험 실행

## 가설

| ID | 가설 | 확인 방법 |
|---|---|---|
| H1 | 상대 거리 `dRef/d`는 전체 텍셀 간격 축소를 상쇄하므로 월드 이동 속도의 정규화와 다름 | 일정 경사의 analytic Surface에서 거리·높이차·RawFlux·이동 거리 측정 |
| H2 | 실제 DeltaTime 사용은 총 진행 시간을 맞추지만 큰 한 step과 작은 여러 step의 결과까지 같게 만들지는 않음 | 같은 총 시간의 dt와 dt/2 비교, alpha와 재전달 횟수 기록 |
| H3 | 같은 수치로 뺀 양을 더하는 현재 방식의 `ΣState` 보존은 비균일 면적에서 `ΣArea×State` 보존과 다름 | 면적 비율 1:4인 두 칸과 UV 왜곡 Surface 비교 |
| H4 | 같은 연속적인 모델을 거리·면적으로 이산화하면 해상도 증가 시 이동 거리·분포 오차가 수렴할 수 있음 | 후속 후보의 동일 계수를 128·256·512에 고정하고 비교 |
| H5 | 필요한 substep 증가는 흐름을 개선하더라도 GPU 비용을 늘릴 수 있음 | 같은 총 시간·허용 오차에서 dispatch 수와 GPU 시간 측정 |

H4의 후보는 현재 계약의 구현 수정으로 확정하지 않았다. 새 모델과 기존 모델의 결과 차이는 동일 수식의 오차 감소로 해석하지 않는다.

## 현재 구현에서 확인한 사항

다음은 소스 검사로 확인한 기준이다. 실험 결과나 해상도 독립성 보장은 아니다.

| 대상 | 확인한 처리 | 코드 |
|---|---|---|
| 거리 | 유효 월드 endpoint 거리, 양 endpoint 평균 이웃 간격의 평균, `clamp(dRef/d,0,1)` | `Source/SurfaceStateSystem/GPU/SurfaceGPUResourceLayout.cpp` |
| GeometryDrive | 절대 월드 높이차 × 중력 투영 방향 정렬도 | `Shaders/Simulation/SurfaceSolver/SurfaceSolverCommon.glsl` |
| 실제 Rate | version 2 Factor에 기준 Rate를 곱함, Wetness Geometry는 `0.5×100=50` | 같은 shader, [[../../05_ADR/0029-Normalized-Transport-Factors\|ADR 0029]] |
| DeltaTime | transport 및 decay에 Δt를 곱함 | 같은 shader |
| 기본 시간 모드 | Fixed 기본 ON, step당 1/60초 × 배속. OFF에서는 `steady_clock`의 실제 경과 시간 × 배속 | `Application.cpp`, `DebugUI.cpp/.h` |
| Fixed 비교 모드 | 렌더 frame당 1/60초 × 배속, 현재 accumulator 없음 | `DebugUI.cpp`, `Renderer.cpp` |
| source 제한 | 감쇠 후 보유량을 넘는 outgoing은 alpha로 축소 | `SurfaceSolverPass1.comp` |
| 반복 수 | 실행 frame마다 instance당 Solver step 한 번, 새 유입은 다음 step부터 전달 | `Renderer.cpp`, `SurfaceStateSystem.cpp`, `SurfaceSolverPass2.comp` |
| 면적 | transport, alpha, Next 및 Contact 입력에 셀 면적을 사용하지 않음 | 위 Solver 파일, `SurfaceStateSystem.cpp` |
| 면적 표시 | fragment의 UV/월드 미분 비율을 해상도로 나눈 면적 Heatmap | `Shaders/Debug/SurfaceDebug.frag` |

면적 Heatmap은 기본 Mesh와 instance scale을 이용하는 표시용 근사다. 부분적으로 덮인 texel, seam 소유권, Virtual Height의 표면 면적까지 포함한 Solver용 제어 면적을 제공하는 것은 아니다.

### 거리 항의 구분

```text
현재: DistanceWeight = clamp(dRef / d, 0, 1)
간격을 절반으로: dRef도 절반, d도 절반 → 비율은 유지
같은 경사의 HeightDrive: 높이차는 대략 절반
```

현재 Geometry RawFlux에 `1/d`를 추가하면 높이차의 거리 비례를 상쇄하는 후보가 되지만, 이동 비율이 `vΔt/d`인 모델로 자동 변환되는 것은 아니다. 기존 Rate는 이동 속도의 단위가 아니고, 보유량·면적·여러 방향 유출과도 관계가 있다. `1/d` 또는 `1/d²`를 전체 Transport에 일괄 적용하는 변경은 채택하지 않는다.

### State 표현과 측정 장부

총량 `m_i` 저장과 면적당 값 `s_i` 저장은 모두 가능하며 `m_i=A_i s_i`로 연결된다. 후속 물리 모델에서는 보존량과 Capacity·입력·감쇠의 단위를 함께 정의해야 한다.

- 총량 표현의 전체 양: `Σm_i`.
- 면적당 표현의 전체 양: `ΣA_i s_i`.
- 현재 Solver가 직접 보존하는 값: 입력·감쇠 없는 닫힌 지원 graph에서 `ΣState_i`.

현재 State를 면적당 값으로 가정해 구한 `ΣA_i State_i`는 진단용 지표다. 이것의 변화만으로 현재 구현이 자신의 State 합 보존 계약을 위반했다고 판정하지 않는다. Wetness를 포화 비율로 해석한다면 면적 외에도 면적당 보유 용량이 질량 환산에 필요하다. Registry의 모든 State를 같은 물리 단위로 취급하지 않는다.

## 측정 조건과 방법

### 1. 재현 조건을 고정한다

- 측정 시 commit, 해당 working diff, Shader binary/hash, Profile version·Factor·기준 Rate, GPU·driver·validation·build 구성을 기록한다.
- cache ON을 기준으로 사용하고 OFF는 대표 조건의 결과 동등성 확인에만 사용한다. 성능 비교에는 mode 전환 전후 평균을 섞지 않는다.
- 해상도 변경은 State를 초기화하므로 매 run마다 동일한 analytic 초기조건을 다시 생성한다. UI 클릭 횟수나 같은 texel 좌표를 초기조건으로 삼지 않는다.
- 첫 기준은 1 Registry channel, 같은 Profile, identity transform, 입력·감쇠 OFF, 중립 Normal/Boundary/Curvature 가중치다. DistanceWeight는 ON/OFF를 구분한다.
- macro normal과 analytic 높이를 사용해 해상도별 Normal Map·Virtual Height 재생성 오차를 배제한다. 실제 에셋은 후속 단계에서 평가한다.

### 2. 공간과 시간 영향을 분리한다

| 단계 | 조건 | 목적 |
|---|---|---|
| A | 같은 경사의 평면, 128·256·512, 충분히 작은 공통 Δt | 현재 수식의 해상도 의존 측정 |
| B | 한 해상도, 총 시간 고정, Δt=1/30·1/60·1/120·1/240초 | 시간 간격에 따른 분포·alpha 비교 |
| C | 같은 총 시간·입력, 렌더 cadence 15·30·60 FPS에 해당하는 시간 스케줄 | 실제 dt, 기존 Fixed, 후속 accumulator/substep 후보 비교 |
| D | 면적 1:4의 두 셀 및 같은 월드 평면의 서로 다른 UV 배치 | State 합과 면적을 곱한 양의 차이 |
| E | UV seam, Profile 경계, unsupported channel, 비균일 instance scale | 연결·면적·경계 가중치 영향 |
| F | 실제 Cube Wetness Scene, 128·256·512 | 이산화·전처리·성능이 함께 작용하는 실제 조건 |

각 스케줄은 진행할 총 시간 `T`를 정확히 맞춘다. 마지막 step은 남은 시간만 진행한다. 순수 transport는 먼저 입력 없이 검사하고, Contact 입력량의 해상도 의존은 별도 run으로 분리한다.

기존 Fixed 모드의 wall-clock 속도는 별도 보고한다. 정상 실행·배속 1에서 15 frame × 1/60초는 0.25초의 시뮬레이션 시간이다. 실제 dt 모드는 경과 시간의 합이 같아도 step 크기에 따른 결과 차이가 남을 수 있다.

### 3. 초기조건과 면적을 만든다

월드 공간의 같은 중심·반경·분포로 smooth patch를 정의한다. 경계에 도달하기 전 구간에서 이동 거리와 분포를 비교한다. 초기값을 각 해상도의 texel 표현으로 적분하거나 표본화하고, 사용한 방식을 기록한다.

후속 면적당 후보의 경우 `s_i=M₀ w_i / Σ(A_i w_i)`로 목표 총량 `M₀`를 맞춘다. 총량 후보는 `m_i=A_i s_i`로 초기화한다. 현재 fixed Profile Capacity를 총량 표현의 면적별 Capacity로 간주하지 않는다.

면적 `A_i`는 analytic fixture에서는 정확한 값으로, Mesh fixture에서는 triangle과 UV-cell의 겹침 및 월드 변환으로 계산하는 시험용 경로를 준비한다. invalid/부분 texel, seam의 중복 소유, 비균일 scale을 검사하고 표면 총면적과 합계를 비교한다. macro 면적과 Virtual Height를 포함한 유효 면적은 별도 실험 조건으로 둔다.

### 4. substep 후보를 평가한다

진행할 시간 `T_frame`을 여러 `dt_sub`로 나누고, 매번 Current→Next를 계산해 교환한다. `Σdt_sub=T_frame`이다. 한 번의 큰 RawFlux를 나눠 쓰는 방식으로 대체하지 않는다.

```text
설명용 예시: T_frame=0.06초, v=0.5 world-unit/s, d=0.01 world-unit
총 이동 거리: 0.03 world-unit = 3칸
substep 6회 × 0.01초: 각 substep의 이동 거리 = 0.005 world-unit = 0.5칸
```

이 예시의 단순 1차원 이류에서는 `|v|dt_sub/d≤C_target`을 기준으로 분할할 수 있다. 한 substep이 정확히 한 칸 이동하도록 강제하는 규칙은 아니다. 여러 방향 유출·확산·비균일 graph의 제한은 따로 정해야 한다. 현재 GeometryTransferRate를 `v`로 대입하지 않는다.

초기에는 현재 수식으로 dt-halving과 고정 substep 수를 비교한다. alpha가 자주 1보다 작은 조건도 함께 측정한다. 이는 시간 간격 민감도 진단이며 현재 수식의 CFL 조건을 증명하는 실험은 아니다.

후속 구현에서는 매 substep의 A/B 교환·barrier·RawFlux 갱신을 확인한다. Event Input은 정한 시점에 한 번만 소비하고, 연속 입력률만 dt_sub에 따라 적분한다. GPU query는 frame의 전체 Solver 비용과 substep별 비용을 구분한다. 처리량 부족으로 step 수를 제한한다면 미처리 시간과 지연을 기록하며 버린 시간을 숨기지 않는다.

### 5. 후속 공간 수식 후보를 비교한다

다음은 현행 수식과 구별되는 검토 후보다. 평면 fixture부터 시작하고 Surface graph에서 유효한 제어 면적·경계 길이를 먼저 정의한다.

| 후보 | 정의할 내용 | 비교 시 주의 |
|---|---|---|
| 현행 기준 | 상대 DistanceWeight, 절대 HeightDrive, source alpha | 기준 결과이며 속도장 모델로 간주하지 않음 |
| 높이차/거리 후보 | 같은 경사의 slope로 Drive를 표현 | 기존 DistanceWeight와 역할·단위 재검토, 속도 불변을 자동 보장하지 않음 |
| 보존형 이류 후보 | 월드 속도와 면적당 양에서 경계 통과 총량 계산 | 단순 균일 1D의 이동 비율은 `vΔt/d` |
| 보존형 확산 후보 | 밀도 차의 거리 기울기·경계 길이·면적으로 계산 | 단순 균일 격자의 변화 계수는 `DΔt/d²`, 이류와 구분 |
| Semi-Lagrangian 후보 | 속도에 따라 출발점 역추적·보간, 별도 보존 처리 | 여러 칸 이동, seam 추적, 질량·비음수·보간 비용 검사 |

검토용 표면 장부에서는 이류 전달 총량을 `v_normal × source density × 공유 경계 길이 × Δt`로, 확산 전달 총량을 `D × 밀도 차 / 거리 × 공유 경계 길이 × Δt`로 구성할 수 있다. 양쪽 셀은 같은 전달 총량을 사용하고, 면적당 값을 저장하면 각자 자기 면적으로 나눠 갱신한다. 임의의 8-neighbor 관계가 이 경계 표현을 이미 갖춘 것으로 가정하지 않는다.

## 측정 지표와 판정

| 지표 | 기록 방법 / 목적 |
|---|---|
| 진행 시간 | `ΣΔt`, wall-clock, Solver step 수, 지연 시간 |
| 이동 거리 | 정의한 총량으로 가중한 중심의 downhill 이동, 월드 단위 |
| 분포 변화 | 같은 월드 평가 영역으로 환산한 L1/L2 차이, 폭·최대값 |
| 보존 장부 | `ΣState`와 `ΣA State`를 모두 기록, 입력·sink·경계 출입을 별도 합산 |
| 유출 제한 | 활성 source의 alpha 최소·평균 및 alpha<1 비율 |
| 공간 계수 | 실제 d, dRef, HeightDrive, DistanceWeight, RawOutgoing 분포 |
| 값의 유효성 | 최소 State, 음수, NaN/Inf, unsupported/invalid로의 누출 |
| 입력 의존성 | 한 world-radius Contact의 texel 수, State 합 증가, 면적 가중 증가 |
| 비용 | Pass 1·2 및 frame 전체 Solver GPU 시간, substep 수, CPU 준비, 추가 자원 |

현재 수식에는 일반적인 일정 속도 이동의 정답을 강제하지 않는다. analytic 정답 `x(T)=x(0)+vT`는 같은 일정 속도 이류 모델을 구현한 후보에만 사용한다. 해상도별 Normal Map 전처리 결과가 다른 실제 Scene 비교는 공간 수식의 순수 오차와 구분한다.

평가 기준은 수치 실험 전에 고정한다. 초기 후보 기준은 이동 거리 차이 5% 이내, 닫힌 보존 실험의 상대 총량 오차 `1e-4` 이내, 비음수·finite 유지로 두되 **잠정 기준**으로 기록한다. 기준량·누적 시간·GPU float32 오차를 보고 이유 없이 완화하지 않는다. 작은 dt 기준 자체도 dt-halving으로 수렴 여부를 확인한다. 성능·허용 오차의 최종 채택 기준은 미확정이다.

## 결과

**미실시.** 이번 문서 작성에서는 source 검사만 수행했다. 위 숫자 예시는 계산 원리를 설명하며 엔진 실행 결과가 아니다.

| Fixture / 모델 | 해상도 | Δt / substep | 총 시간 | 이동 거리 | State 합 오차 | 면적 가중량 오차 | alpha<1 비율 | GPU ms |
|---|---:|---|---:|---:|---:|---:|---:|---:|
| 측정 대기 | — | — | — | — | — | — | — | — |

## 해석과 한계

- DeltaTime을 곱하는 것은 시간 단위를 맞추지만 수치 결과의 timestep 독립성을 보장하지 않는다.
- 상대 거리 감쇠, 이류의 공간 이산화, 확산의 공간 이산화는 같은 목적의 `1/d` 연산이 아니다.
- source alpha의 비음수·State 합 보존과 의도한 월드 속도 유지·면적 가중량 보존을 구분한다.
- 해상도를 올렸을 때 모든 texel 값이 동일해지는 것을 목표로 삼지 않는다. 같은 월드 초기조건·물리 모델에 대한 결과 수렴을 평가한다.
- 캐싱의 전달 결과 동등성 검증은 수식의 해상도 의존성 검증을 대체하지 않는다.

## 후속 작업

1. 현재 수식의 analytic 평면 및 면적 차이 fixture, readback 측정 harness 준비.
2. 단계 A·B로 공간·시간 의존 분리, 단계 C로 기본 Fixed 모드와 실제 프레임 dt 모드의 차이 확인.
3. 보존할 State의 단위, 셀 면적·경계 길이와 후보 계수를 결정한 뒤 공간 후보 평가.
4. 실제 Scene과 성능 비용을 확인하고 채택할 변경만 ADR 및 Architecture에 반영.

## 근거 자료

- [[../../02_Research/0002_Semi-Lagrangian-Transport|Semi-Lagrangian 조사]]
- [[../../02_Research/0001_Bound-Preserving-Transport|Bound-Preserving Transport]]
- [[../../05_ADR/0015-Geometry-Driven-Transport|ADR 0015]]
- [[../../05_ADR/0016-Transport-Transfer-Weights|ADR 0016]]
- [[../../05_ADR/0020-State-Overcapacity-Transport|ADR 0020]]
- [[../../05_ADR/0023-Simulation-Resolution-Presets|ADR 0023]]
- [[../../05_ADR/0029-Normalized-Transport-Factors|ADR 0029]]
- [[../../04_Architecture/0006_Surface-State-Update|State Update]]
- [[../../04_Architecture/0010_UI-Interface|UI Interface]]
- [Clawpack의 보존형 이류 설명](https://www.clawpack.org/riemann_book/html/Advection.html)
- [LeVeque, 유한체적법 개요 §1.2](https://www.clawpack.org/fvmhp_materials/sample.pdf)
