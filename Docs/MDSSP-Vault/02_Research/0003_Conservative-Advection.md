 밀도·속도·유량과 보존형 이동

> **한 줄 요약:** 흐르는 속도와 경계의 밀도로 유량을 정하고, 나간 양과 들어온 양을 맞추면 물질을 보존하며 이동을 계산할 수 있다.

- 상태: **개념 조사·적용 검토 — 현재 Solver 수식 변경 없음**
- 작성·최종 확인: 2026-09-29
- 조사 범위: Clawpack의 Advection 장 전체와 책 소개, LeVeque의 유한체적법 설명 §1.2. 아래 격자·면적 예시는 원리를 설명하기 위한 계산이며 엔진 실험 결과는 아니다.

## References

| 항목 | 내용 |
|---|---|
| 연구 제목 | Advection — Riemann Problems and Jupyter Solutions: Theory and Approximate Solvers for Hyperbolic PDEs의 개념 설명 장 |
| 저자 | David I. Ketcheson, Randall J. LeVeque, Mauricio J. del Razo |
| 공개 연도 / 버전 | 책 출판 2020, SIAM. 온라인 Advection 장 확인 2026-09-29 |
| 공식 페이지 / 원문 | [책 소개](https://www.clawpack.org/riemann_book/), [Advection 장](https://www.clawpack.org/riemann_book/html/Advection.html) |
| 개념 설명 | [Advection의 유량·특성선·Riemann 문제](https://www.clawpack.org/riemann_book/html/Advection.html) |
| 보존식 배경 | Randall J. LeVeque, [Finite Volume Methods for Hyperbolic Problems, §1.2](https://www.clawpack.org/fvmhp_materials/sample.pdf) |

## 1. 어떤 문서이고 왜 읽는가?

**물질의 분포가 흐름을 따라 이동하는 가장 단순한 상황을 설명하는 교재의 한 장이다.** 새로운 GPU 알고리즘이나 MDSS 전용 해법을 제시하는 논문은 아니다. 책은 수식과 Jupyter 예제를 함께 제공한다. [책 소개](https://www.clawpack.org/riemann_book/)

Advection은 **흐름이 물질을 실어 나르는 이동**을 뜻한다. 긴 관 속의 물질이 일정한 속도로 이동한다고 생각하면 된다. 이 장은 그 상황에서 밀도·속도·유량이 어떻게 연결되는지 설명한다. [Advection 장](https://www.clawpack.org/riemann_book/html/Advection.html)

프로젝트에서는 “거리와 Δt를 이미 사용해도 왜 이동 속도가 해상도에 따라 달라질 수 있는가?”를 검토하는 배경으로 읽는다. 원문 설명과 현재 Solver에 대한 판단은 아래에서 구분한다.

## 2. 밀도, 속도, 유량은 서로 다른 값이다

먼저 1차원 관을 가정한다. `q`는 길이당 물질의 양, `u`는 이동 속도, `f`는 한 지점을 1초 동안 통과하는 물질의 양이다. 원문의 기본 관계는 `f=u q`다. [Advection, 보존식 유도](https://www.clawpack.org/riemann_book/html/Advection.html)

아래 숫자는 직접 구성한 설명용 예시다.

| 값 | 예시 | 뜻 |
|---|---|---|
| 밀도 `q` | 2 g/cm | 1 cm마다 물질 2 g이 들어 있다. |
| 속도 `u` | 3 cm/s | 물질이 1초 동안 3 cm 이동한다. |
| 유량 `f=u q` | 6 g/s | 한 지점을 1초 동안 6 g이 지나간다. |

0.5초라면 한 지점을 통과하는 양은 `6×0.5=3 g`이다. **이동 속도가 같아도 밀도가 두 배면 지나가는 양도 두 배다.** 이동 거리와 이동량은 같은 개념이 아니다.

표면에서는 길이당 밀도 대신 **면적당 양**을 사용할 수 있다. 이때 경계를 통과하는 총량을 계산하려면 경계 길이까지 고려해야 한다. 이 부분은 원문의 관 예시에서 출발한 표면 적용 검토다.

## 3. 보존식은 “들어온 양 − 나간 양”이다

관의 한 구간만 관찰하자. 왼쪽으로 들어온 유량이 6 g/s, 오른쪽으로 나간 유량이 4 g/s이고 0.5초가 지났다면 다음과 같다.

```text
들어온 양: 6 × 0.5 = 3 g
나간 양:   4 × 0.5 = 2 g
구간에 늘어난 양: 3 − 2 = 1 g
```

내부에서 물질을 만들거나 없애지 않는 조건에서, 원문은 이 장부를 보존식으로 표현한다. [Advection, 보존식 유도](https://www.clawpack.org/riemann_book/html/Advection.html)

$$
q_t+f_x=0,\qquad f=u q
$$

`q_t`는 한 위치의 밀도가 시간에 따라 얼마나 변하는지, `f_x`는 위치에 따라 유량이 얼마나 달라지는지를 나타낸다. 나가는 유량이 더 크면 밀도가 줄고, 들어오는 유량이 더 크면 밀도가 늘어난다.

외부 입력이나 감쇠가 있으면 그 양도 장부에 넣어야 한다. “보존”은 항상 총량이 고정된다는 뜻이 아니라 **총량 변화가 실제 출입·생성·손실과 맞아야 한다**는 뜻이다.

## 4. 일정한 속도라면 분포 전체가 같은 거리만큼 이동한다

속도를 일정한 `a`로 두면 원문의 식과 해는 다음처럼 단순해진다. `q₀`는 시작할 때의 밀도 분포다. [Advection, 일정 속도의 해](https://www.clawpack.org/riemann_book/html/Advection.html)

$$
q_t+a q_x=0,\qquad q(x,t)=q_0(x-at)
$$

`x-at`는 **현재 위치에 도착한 물질이 시작할 때 있었던 위치**다. 설명용으로 오른쪽 이동 속도 3 cm/s, 경과 시간 2초를 대입하면 이동 거리는 6 cm다.

```text
현재 x=10 cm의 값 = 시작할 때 x=4 cm의 값
시작할 때 [0, 2] cm에 있던 덩어리 → 2초 뒤 [6, 8] cm
```

이 단순한 연속 모델에서는 모양과 높이가 그대로 이동한다. 격자 계산에서 분포가 퍼진다면 그것이 실제 확산인지 수치 계산의 오차인지 구분할 기준이 된다.

### 특성선은 물질의 이동 경로다

원문은 `x-at=일정`인 경로를 **특성선(characteristic)**이라고 부른다. 이 일정 속도 모델에서는 경로를 따라 값이 유지된다. [Advection, 특성선](https://www.clawpack.org/riemann_book/html/Advection.html)

예를 들어 x=4 cm에서 출발한 값은 1초 뒤 x=7 cm, 2초 뒤 x=10 cm에 있다. 목적지에서 경로를 거슬러 출발점을 찾는 생각이 [[0002_Semi-Lagrangian-Transport|Semi-Lagrangian 방식]]과 연결된다. 다만 원문의 정확한 연속 해와 실제 격자의 역추적·보간 결과는 구분해야 한다.

### Riemann 문제는 왼쪽과 오른쪽 값이 다른 출발 조건이다

처음에 x=0 왼쪽은 밀도 1, 오른쪽은 0이라고 두면 경계가 날카로운 계단 모양이다. 오른쪽 속도 3 cm/s라면 2초 뒤 경계는 x=6 cm에 있다. 이처럼 한 경계를 사이에 두고 양쪽 값이 다른 초기조건을 **Riemann 문제**라고 부른다. 단순한 선형 이동에서는 이 경계도 일정 속도로 이동한다. [Advection, Riemann 문제](https://www.clawpack.org/riemann_book/html/Advection.html)

## 5. 작은 칸으로 계산하면 거리와 시간이 어디에 들어갈까?

유한체적법은 각 칸의 평균값을 저장하고, 공유 경계를 통과하는 유량으로 갱신한다. [유한체적법 §1.2](https://www.clawpack.org/fvmhp_materials/sample.pdf)

여기부터는 **균일한 1차원 격자, 일정한 오른쪽 속도, 입력·감쇠 없음**을 가정한 설명용 계산이다. Clawpack 장이 현재 엔진의 이산 수식을 제시한 것은 아니다.

칸 길이가 `h`, 밀도가 `qᵢ`이면 보유량은 `qᵢh`다. 경계의 밀도를 보내는 칸 값으로 근사하면, 한 step에 보내는 양은 `a qᵢ Δt`다.

```text
보낼 양 / 보유량 = (a × qᵢ × Δt) / (qᵢ × h) = a Δt / h
```

즉 이 단순한 모델의 **보내는 비율**에는 `속도 × 시간 / 칸 길이`가 들어간다. `qᵢ=0`이면 보낼 양도 0이며 위 비율의 나눗셈은 하지 않는다.

속도 1 cm/s, Δt=0.1초를 대입해 보자.

| 칸 길이 `h` | 한 step의 보내는 비율 | 한 칸 이동 거리 × 비율 |
|---:|---:|---:|
| 1 cm | 0.1 = 10% | 0.1 cm |
| 0.5 cm | 0.2 = 20% | 0.1 cm |

한 칸에 모인 양 중 일부만 오른쪽 이웃으로 보낸다면, 마지막 열은 한 step 뒤 **총량의 중심이 이동한 거리**다. 해상도가 높아져 칸이 작아질수록 보내는 비율이 커져야 이 거리도 유지된다. 두 계산의 분포 모양과 장기 오차까지 같다는 뜻은 아니다.

### 한 step에 보유량보다 많이 보내야 한다면?

이 단순한 명시적 식은 `aΔt/h>1`이면 보유량을 초과해 보낸다. 비음수를 유지하려면 이 조건에 맞춰 시간 간격을 줄이거나 다른 방법을 써야 한다.

예를 들어 h=1 cm, 속도=10 cm/s, 진행할 시간=0.2초이면 `aΔt/h=2`다. 0.1초씩 두 번 계산하면 매번 한 칸씩 전달하고, 첫 계산에서 받은 양을 두 번째 계산에서 다시 보낼 수 있다. **나눈 시간의 합은 원래 0.2초여야 한다.**

이 비율이 이 단순 모델의 CFL 조건을 읽는 기준이다. “어떤 시간 단위가 항상 한 텍셀 이동”이라는 규칙은 아니며, 칸 크기와 속도에 따라 허용 시간이 달라진다. 다차원, 확산, 비균일 표면에서는 다른 조건도 고려해야 한다. 이 예를 현재 Solver의 적정 Δt로 사용하지 않는다.

## 6. 표면의 면적당 값과 총량은 어떻게 연결할까?

다음은 원문의 1차원 장부를 표면에 적용할 때 검토할 표현이다. 현재 엔진의 채택 수식은 아니다.

```text
텍셀 총량 mᵢ = 텍셀 면적 Aᵢ × 면적당 값 sᵢ
다음 sᵢ = 현재 sᵢ + (받은 총량 − 보낸 총량) / Aᵢ
```

설명용으로 면적 2인 칸에서 면적 1인 칸으로 총량 0.2를 옮기면, 출발점의 면적당 값은 0.1 줄고 도착점은 0.2 늘어난다. 총량 기준으로는 양쪽 모두 0.2다.

총량 `m`을 저장해도 되고 면적당 값 `s`를 저장해도 된다. **같은 물리량을 일관되게 계산하면 둘 다 가능한 표현이다.** 면적당 값을 저장한다면 Capacity·입력·감쇠도 그 기준에 맞춰야 하며, 비교할 총량은 `ΣAᵢsᵢ`다.

표면 경계의 속도를 쓸 경우 전달 총량은 대략 `경계에 수직인 속도 × 경계의 면적당 값 × 경계 길이 × Δt`로 구성할 수 있다. 현재의 텍셀 이웃 graph를 실제 칸 경계와 어떻게 연결할지는 별도 설계가 필요하다.

## 7. 현재 엔진을 볼 때 주의할 점

다음은 원문 주장이 아니라 현재 구현과의 비교다. 구체적인 구현 확인 내용은 [[0002_Semi-Lagrangian-Transport|Semi-Lagrangian 노트 §7]]과 Architecture에 기록되어 있다.

| 현재 요소 | 이 문서의 속도 모델과 비교 |
|---|---|
| `DistanceWeight=clamp(dRef/d,0,1)` | 평균 거리 대비 상대 보정이다. 모든 거리가 절반이 되면 비율은 유지된다. |
| GeometryDrive의 높이차 | 높이차가 들어 있다는 사실만으로 일정한 월드 속도 `a`를 정의한 것은 아니다. |
| `rawFlux` 계산의 `Δt` 반영 | 시간에 따른 전달량을 반영하지만, 긴 step에서 중간 유입량을 다시 보내는 계산까지 수행하지는 않는다. |
| source `alpha` | 가진 양보다 많이 보내는 것을 제한한다. 큰 계수를 사용해도 한 step의 이웃 전달 거리를 늘리는 장치는 아니다. |
| State 합 보존 | 현재 State는 총량이며 보존 장부는 `ΣState`다. 밀도 `sᵢ=Stateᵢ/Aᵢ`로 표현하면 동일한 양이 `ΣAᵢsᵢ`다. |

따라서 기존 이동량에 `1/d`만 추가하면 해결된다고 단정할 수 없다. 무엇을 밀도로 저장하고 어떤 값이 실제 이동 속도인지 정한 뒤, 전달량·면적·시간을 함께 맞춰야 한다. 현재 Geometry 전달 계수는 이 장의 속도 `a`와 단위부터 다르다. [[../05_ADR/0029-Normalized-Transport-Factors|ADR 0029]]

초기 Fixed ON은 렌더 frame마다 한 step이었다. 현재는 실제 시간×배속을 누적한다. 기본 Fixed ON·Auto OFF의 15 FPS·배속 1에서는 frame당 1/60초씩 4 step으로 실제 1초를 계산한다. Auto ON에서만 Transport 상한으로 구간을 세분화한다. 한 frame 8회 한도를 넘긴 시간과 미완료 구간은 이월하므로 GPU 과부하에서는 지연이 남을 수 있다. Δt를 곱하는 것과 실제 경과 시간을 따라잡는 실행 구조는 별개다. [[../05_ADR/0034-Fixed-Timestep-and-Auto-Substepping|ADR 0034]] [[../04_Architecture/0010_UI-Interface|시간 설정]] [[0004_Substepping-and-Adaptive-Time-Stepping|Substepping 용어 조사]]

## 8. 무엇을 실험으로 확인할까?

**같은 월드 조건과 같은 시뮬레이션 시간**을 맞춘 뒤 해상도에 따른 차이를 비교한다. 아래는 계획이며 측정 결과가 아니다.

| 질문 | 비교 방법 |
|---|---|
| 월드 이동 속도가 유지되는가? | 128·256·512에서 같은 위치·면적·총량으로 시작해 총량 중심의 이동 거리 비교 |
| 양이 보존되는가? | 입력·감쇠 없는 닫힌 영역에서 저장 의미에 맞는 `Σm` 또는 `ΣA s` 비교 |
| 시간 간격에 따라 달라지는가? | 같은 총 시간을 한 번에 계산한 경우와 여러 step으로 나눈 경우 비교 |
| 모양이 얼마나 퍼지는가? | 최대값·분포 폭과 총량 오차를 각각 확인 |
| FPS가 결과에 섞이는가? | 실제 경과 시간과 실행한 Solver Δt 합을 함께 기록 |

시험 조건과 지표는 [[../06_Development/Experiments/0005_Resolution-and-Timestep-Dependence|해상도·시간 간격 실험 초안]]에서 구체화한다. 이 노트만으로 보존형 속도 모델의 채택이나 성능 개선을 확정하지 않는다.

## 관련 문서

- [[0000_Research-Index|연구 색인]]
- [[0001_Bound-Preserving-Transport|포화 상한을 보존하는 전달 계산]]
- [[0002_Semi-Lagrangian-Transport|Semi-Lagrangian Transport]]
- [[../04_Architecture/0006_Surface-State-Update|State Update]]
- [[../05_ADR/0015-Geometry-Driven-Transport|Geometry-Driven Transport]]
- [[../05_ADR/0016-Transport-Transfer-Weights|Transfer Weights]]
- [[../05_ADR/0029-Normalized-Transport-Factors|Normalized Transport Factors]]
- [[../06_Development/Experiments/0005_Resolution-and-Timestep-Dependence|실험 초안]]
