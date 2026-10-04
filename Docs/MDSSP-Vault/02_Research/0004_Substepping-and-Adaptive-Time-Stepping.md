# Substepping과 Adaptive Time Stepping

> **한 줄 요약:** Fixed timestep은 계산할 기본 시간 구간을 정하고, Auto substepping은 필요할 때 그 구간을 더 작은 계산으로 나눈다.

- 상태: **용어 조사 완료 / Fixed timestep·Auto substepping 분리 구현**
- 작성·최종 확인: 2026-09-29
- 조사 범위: Unreal의 Physics Sub-Stepping 설명·설정, PETSc의 Error control via variable time-stepping, Clawpack의 Time stepping 설정. 각 엔진의 전체 Solver 구현이나 본 프로젝트의 일반적인 안정성 증명은 포함하지 않는다.

## References

| 항목 | 내용 |
|---|---|
| 게임 엔진의 공식 문서 | Epic Games, [Physics Sub-Stepping](https://dev.epicgames.com/documentation/unreal-engine/physics-sub-stepping-in-unreal-engine) |
| 수치해석의 공식 문서 | PETSc, [TS: Scalable ODE and DAE Solvers — Error control via variable time-stepping](https://petsc.org/main/manual/ts/#error-control-via-variable-time-stepping) |
| CFL에 따른 간격 조정 | Clawpack 5.11.x, [Specifying classic run-time parameters — dt_variable·cfl_desired](https://www.clawpack.org/v5.11.x/setrun.html) |
| 확인 시점 | 2026-09-29. 온라인 문서는 갱신될 수 있으므로 특정 출판 연도로 표현하지 않는다. |
| 프로젝트 결정 | [[05_Decisions/0013_Fixed-Timestep-and-Auto-Substepping|Decision 0013]], [[03_Architecture/0006_Surface-State-Update|State Update]] |

## 1. 두 이름은 무엇을 뜻하는가?

**Substepping**은 한 시간 구간을 더 작은 step 여러 개로 나누는 것이다. 예를 들어 0.02초를 0.01초씩 두 번 계산한다. Unreal도 frame 시간을 여러 substep으로 나눠 물리 계산을 반복하는 기능을 이 이름으로 설명한다. 작은 substep은 정확도와 안정성에 도움이 될 수 있지만 계산 비용이 늘어난다. [Unreal 공식 문서](https://dev.epicgames.com/documentation/unreal-engine/physics-sub-stepping-in-unreal-engine)

**Adaptive time stepping**은 계산 조건에 따라 시간 간격을 자동으로 바꾸는 것이다. 무엇을 기준으로 바꿀지는 별도 선택이다. PETSc는 추정 오차를 허용 범위에 맞추도록 간격을 조정하고, Clawpack은 목표 Courant 수에 따라 간격을 조정하는 설정을 제공한다. 두 기준은 같은 것이 아니다. [PETSc 공식 문서](https://petsc.org/main/manual/ts/#error-control-via-variable-time-stepping), [Clawpack 공식 문서](https://www.clawpack.org/v5.11.x/setrun.html)

프로젝트 UI의 **Auto substepping**은 필요하면 계산 구간을 자동으로 나눈다는 뜻이다. 특정 논문의 알고리즘 이름이나 업계의 단일 표준 설정명을 그대로 채택한 것은 아니다.

## 2. Fixed timestep은 무엇을 고정하는가?

기본 시간 구간을 **1/60초**로 고정한다. 실제 경과 시간에 배속을 곱해 누적하고, 1/60초가 모이면 한 구간을 처리한다. 남은 시간은 다음 frame까지 기다린다.

배속 1에서 충분한 GPU 처리량이 있다면 60 FPS는 frame당 약 한 구간, 30 FPS는 두 구간, 15 FPS는 네 구간을 처리한다. 모두 실제 1초에 시뮬레이션 1초를 계산한다. 배속은 누적되는 시간에 적용하고 1/60초 자체를 바꾸지 않는다.

**Auto substepping이 OFF이면 Profile 계수나 형상으로 dt를 바꾸지 않는다.** 각 구간에서 Flux와 alpha를 다시 계산한다.

## 3. 자동으로 나누면 무엇이 달라지는가?

현재 Solver는 한 step의 이전 State를 읽어 다음 State를 만든다. B가 A에서 이번에 받은 양은 다음 step부터 C로 보낼 수 있다.

다음은 원리를 보여주는 일방향 설명용 예시다. 실제 Cube의 측정 결과나 일반적인 속도 보장은 아니다. 초기 분포는 A=1, B=0, C=0이고 입력·감쇠는 없다고 하자. 1/60초의 원시 유출이 보유량의 두 배가 되는 조건을 가정한다.

| 계산 방법 | 결과 A, B, C | 이유 |
|---|---|---|
| 1/60초 한 번 | 0, 1, 0 | alpha가 A의 전달을 보유량 1로 제한하고, B는 이전 State가 0이라 아직 C로 보내지 못함 |
| 1/120초 두 번 | 0, 0, 1 | 첫 step에서 A→B, 갱신된 State를 읽는 두 번째 step에서 B→C |

**계산한 총 시간은 동일하다.** 작은 step은 중간 유입을 다시 전달할 기회를 늘린다. 작은 간격을 쓰는 목적은 빠른 흐름이 alpha 때문에 의도보다 느리게 이동하는 현상을 줄이는 것이다. 이 예시는 100% 전달 조건이고, 현재 자동 제한은 원시 유출 비율 90% 이하를 목표로 더 보수적인 간격을 사용한다.

## 4. 현재 Auto substepping은 무엇을 기준으로 나누는가?

현재 Profile·면적·Weight·높이차로 source의 원시 유출 비율 상한을 계산한다. 그 비율이 한 step에 90%를 넘지 않도록 시간 간격을 정한다. 현재 State를 GPU에서 읽어오는 작업은 추가하지 않는다. 조건이 같으면 상한을 재사용하고 Profile·term·transform 변경 시 갱신한다.

이는 **Transport 상한에 따른 시간 간격 조정**이다. PETSc의 오차 추정기를 구현한 것이 아니고, 본 graph Solver에 대해 엄밀한 CFL 안정성·정확도를 증명한 것도 아니다. 따라서 UI 이름을 CFL로 두지 않는다. alpha는 Auto substepping ON/OFF 모두에서 유지한다.

## 5. 두 옵션의 조합

| Fixed timestep | Auto substepping | 동작 |
|---|---|---|
| ON | OFF | **기본값.** 1/60초가 모일 때마다 dt=1/60초로 계산 |
| ON | ON | 1/60초가 모이면 Transport 상한 이하의 step으로 나눔. 마지막 짧은 step까지 합쳐 그 구간을 완료 |
| OFF | OFF | 누적된 경과 시간을 한 번의 dt로 계산 |
| OFF | ON | 누적 시간을 Transport 상한 이하로 나누고 마지막 잔여 시간까지 계산 |

Fixed ON·Auto ON에서는 1/60초보다 적은 시간만 모였다고 먼저 작은 step을 실행하지 않는다. 이미 시작한 구간이 frame당 8회 한도 때문에 미완료라면, 나머지는 다음 frame에서 계속 처리한다. 미처리 시간은 폐기하지 않는다.

옵션 변경은 State와 누적 시간을 초기화하지 않는다. Auto를 끄는 시점에 시작된 고정 구간이 남아 있으면 그 잔여 구간을 한 번 마무리하고, 이후 새 구간은 1/60초씩 계산한다. Fixed를 끄면 남은 시간을 variable 방식으로 소비한다.

## 6. 비용과 검증 범위

작은 step을 많이 쓰면 매번 2-Pass와 State 교환을 실행해야 하므로 GPU 비용이 늘어난다. 처리량이 부족하면 실제 시간을 모두 계산하지 못한 backlog가 증가한다. Auto OFF도 alpha 제한이 강해져 흐름이 느려지거나 시간 오차가 커질 수 있다. 어느 쪽이 적절한지는 같은 초기 분포와 같은 시뮬레이션 시간으로 비교한다.

CPU 회귀 검사는 네 옵션 조합, FPS cadence, 고정 구간의 대기·마지막 substep, 반복 한도 이후 재개, pause·수동 step·reset·옵션 전환을 확인한다. 전체 Cube에서의 이동·분포·GPU 비용 비교는 실험 초안을 따른다.
