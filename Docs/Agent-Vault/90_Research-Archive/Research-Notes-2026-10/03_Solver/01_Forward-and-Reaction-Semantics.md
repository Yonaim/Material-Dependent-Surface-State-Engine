# Forward 계산과 반응·수송 의미

현재 계산 계약과 inflowFraction, 방향·거리·분할 및 반응 경쟁의 제한 모델 분석을 묶었다. 제안된 의미 변경을 현행 코드 동작으로 읽지 않는다.

## 먼저 볼 결론

- 현행 `inflowFraction`은 신규 유입을 기준으로 반응을 요청하고, 경쟁 공급량은 전체 보유 State에서 제한한다. 제안한 ‘신규분만 소비’ 의미와 구별한다.
- 같은 순수송이라도 gross incoming이나 경로 분할이 다르면 유입 기반 반응이 달라질 수 있다. 제한 모델 분석이며 현 GPU 오류 실측은 아니다.
- 거리 보정, 방향 가중 gather, 표준 kinetics·자원 배분으로 충분한지를 먼저 판별한다.

## 이 문서의 원본 절

- [[90_Research-Archive/Research-Notes-2026-10/03_Solver/01_Forward-and-Reaction-Semantics#source-solver-10-forward-model-and-controls|20_Solver-Semantics/10_Forward-Model-and-Controls.md]]
- [[90_Research-Archive/Research-Notes-2026-10/03_Solver/01_Forward-and-Reaction-Semantics#source-solver-20-transport-reaction-semantics-and-refinement|20_Solver-Semantics/20_Transport-Reaction-Semantics-and-Refinement.md]]

원본 25개 파일의 수정 전 사본과 SHA-256 목록은 `Original-25-Notes.zip`에 있다. 아래 절은 원문의 수식과 당시 판단을 보존하며, 링크와 제목 수준만 새 위치에 맞췄다.

---

## source-solver-10-forward-model-and-controls

원본: `20_Solver-Semantics/10_Forward-Model-and-Controls.md`

### 공통 forward 모델과 코드 근거

마지막 갱신: 2026-10-27. [[04_Development/08_Research-Directions-123|최초 통합 검토]]에서 분리한 후속 연구 정본이다. 이 파일의 내용은 제안·수식 분석이며 새 구현/실험 결과가 아니다. 로컬 앱·GPU·빌드 실행 중지를 유지한다.

문헌 출처는 [[90_Research-Archive/Research-Notes-2026-10/04_Literature/01_Related-Work#source-reference-07-related-work|문헌 확인 수준]], 이 계산 구조에서 이어진 연구는 [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-reference-09-analytical-findings|분석 노트]]를 따른다.


표면 graph를 고정하고 State 총량을 q, Profile/재질 계수를 θ, 사건 입력을 u, 파생 형상을 G라 한다. Dense·고정 시간 간격 경로를 연구 기준으로 삼으면 개념적으로 다음과 같다.

\[
q_{n+1}=\mathcal R_\theta(\mathcal T_\theta(q_n,G_n,u_n),\;\text{positiveInput}_n,\text{incoming}_n),
\qquad G_{n+1}=\mathcal B_\theta(q_{n+1}).
\]

`T`에는 Input/Transport/Decay, `R`에는 유입 비율 전환과 연속 반응, `B`에는 Height/Normal/Concavity/Weights가 포함된다. 실제 cache·scheduling의 의미는 동일 forward 경로로 검증해야 한다. 모든 GPU scratch가 수학적 물질 상태인 것은 아니다.

| 기능 | [C] 근거 | 1·2·3에 주는 조건 |
|---|---|---|
| 포화도·형상 기반 국소 flux | `Shaders/Simulation/Solver/Transport.glsl::rawFlux` | 양의 부분, cavity 저항 clamp, source/target 의존성이 존재 |
| outgoing 공급량 제한 | `SurfaceSolverPass1.comp::main` | `min`/분기에서 미분 불연속 또는 미분 불능 가능 |
| 실제 incoming·입력 장부 | `SurfaceSolverPass2.comp::main`, ReactionScratch plane | Reaction 생성물과 이웃 유입은 의미가 다름 |
| 선언 반응·경쟁·시간 세분화 | `SurfaceReaction.comp::reactionRequest/main` | threshold, supply limiter, 내부 반복 횟수 변화까지 forward 의미에 포함 |
| State→형상 | `StateMath.glsl::accumulationHeight/effectiveLocalPosition/effectiveLocalNormal` | 형상 기여의 Capacity clamp와 저장 State의 무상한 계약을 구분 |
| 형상→다음 수송 | `SurfaceDynamicGeometryUpdate.comp::main`, `SurfaceDynamicWeightsUpdate.comp::buildTransferWeight` | 목표 State를 수정하면 다른 채널의 전파도 달라질 수 있음 |
| 전체 기록 순서 | `Source/SurfaceState/Solver/SurfaceStateSolver.cpp::RecordSteps` | 다중 State·Reaction·Geometry 연산의 전/후 순서를 보존해야 함 |
| 화면 관측 | `Shaders/Rendering/Surface/Lighting/SurfaceLit.glsl`, `StateTexture/StateSampling.glsl` | 표시 clamp·재질 합성 때문에 관측이 State를 유일하게 정하지 않을 가능성. 실제 장면 반례는 미검증 |
| 입력·최소 계측 | `Contact/SurfaceContactApplication.cpp::ApplyPendingContacts`, `Rendering/Profiling/RendererBenchmark.cpp::WriteAuthoringCheckpoint` | 접촉 입력·같은 Step readback 재사용 가능. 미분·역문제용 자동 반복 인터페이스는 구현한 바 없음 |

[E] PERF-041의 한 판에서 Geometry D/E가 분포를 바꾼 것은 결합 경로가 작동한다는 증거다. 그 값이 gradient에서 중요하거나 역설계를 어렵게 한다는 증거는 아니다. PERF-042는 동일 수치 모델 내부 조정 구조의 확장 부담 우위를 지지하지 않았다. 이 결과를 새 최적화 방법의 Baseline 성능으로 재사용하지 않는다.

---

## source-solver-20-transport-reaction-semantics-and-refinement

원본: `20_Solver-Semantics/20_Transport-Reaction-Semantics-and-Refinement.md`

### 수송 기반 반응의 의미·방향성·격자 분할



작성: **2026-10-28 / Asia/Seoul**. [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-18-research-landscape-and-selection|18]]의 B를 심화한다.



**코드·문헌·기호 분석만 수행했다. 아래 반례는 명시된 제한 모델의 유도이며 GPU 실측이 아니다.** 제품 코드 변경·앱·빌드·수치 실험·학습 실행은 없다. 일반 MDSSP의 수렴/방향 불변성을 증명하지 않았다.



#### 1. 이번에 더 명확해진 문제



18에서는 hop 수에 따른 전환과 거리 보정을 살펴봤다. 더 깊이 보면 다음 세 문제가 분리된다.



1. **이동의 의미:** 같은 순수송을 나타내는 서로 다른 방향별 flux가 같은 반응을 만들어야 하는가?

2. **공간 방향:** 거리 보정 후에도 격자 방향에 따라 전환량이 달라질 수 있는가?

3. **규칙 조합:** 단일 반응의 분할 일관성이 여러 반응의 공급 경쟁에서도 유지되는가?



현재 구조에서는 세 항목 모두 자동으로 보장되지 않는다. 이것이 버그라는 뜻은 아니다. 현상학적 모델이 어떤 이동·사건을 전환의 원인으로 정의했는지에 따라 올바른 결과가 달라진다.



연구 질문은 **‘해상도를 바꿔도 같은 f를 쓰자’보다 ‘전환을 유발하는 양을 명시하고 그 의미를 다른 격자·규칙 조합에서도 유지하자’**로 좁히는 편이 낫다. 다만 각 부분의 표준 해법을 적용한 것만으로 새 알고리즘이 되는 것은 아니다.



#### 2. 코드 계약



- [Transport.glsl::rawFlux](../../../../../Shaders/Simulation/Solver/Transport.glsl): positive saturation difference와 source saturation×GeometryDrive를 더하고 weight·retention·dt를 곱한다.

- [Pass1](../../../../../Shaders/Simulation/Solver/SurfaceSolverPass1.comp): source의 모든 raw outgoing 합에서 공급 비율을 정한다.

- [Pass2](../../../../../Shaders/Simulation/Solver/SurfaceSolverPass2.comp): 제한 후 incoming과 outgoing을 각각 더한다. State 수지는 차이를 쓰지만 Reaction scratch에는 incoming 합을 따로 남긴다.

- [SurfaceReaction.comp::reactionRequest / main](../../../../../Shaders/Simulation/Solver/SurfaceReaction.comp): inflow request는 지정된 Input/Incoming의 fraction이며, 여러 request는 동일 snapshot의 공급량으로 제한된다.



따라서 순수지와 incoming 장부는 다른 정보다. 아래에서 F는 별도 표시가 없으면 **공급량 제한 이후의 실제 directed flux**다.



#### 3. 같은 순수송이 같은 반응을 뜻하지 않는 반례



두 texel i,j 사이에 F_ij=p, F_ji=q가 있다고 하자. 수송만의 변화는 `(q−p, p−q)`다. 양쪽 flux에 같은 c≥0을 더하면 수송 변화는 그대로다.



하지만 새 유입의 fraction f를 immobile residue로 바꾸는 단일 rule은, 다른 반응·Input·Decay가 없고 공급이 충분할 때 두 texel 각각에서 전환량이 fc만큼 증가한다. 특히



$$

(F_{ij},F_{ji})=(c,c)\quad\hbox{vs.}\quad(0,0)

$$



는 **동일한 수송 직후 State**와 **다른 반응 이후 State**를 갖는다. 순유량을 보존하는 재구성이 incoming 기반 반응까지 보존한다는 주장은 성립하지 않는다.



현재 raw 식에서도 낮은 위치의 높은 saturation이 위쪽으로 퍼지고, 위쪽의 낮은 saturation이 중력으로 아래로 흐르는 상반된 성분을 허용한다. 계수·상태를 맞추면 두 방향이 균형을 이루는 경우를 구성할 수 있다. 이 설명은 코드 수식의 가능성이지, 실제 asset에서 그 상황의 빈도·크기를 측정한 것은 아니다. limiter가 활성화된 경우에는 실제 F가 달라지므로 별도로 감사해야 한다.



##### 선택해야 하는 의미



| 전환의 기준 | 균형을 이룬 두 방향 이동에서의 의미 | 장점 / 제한 |

|---|---|---|

| 방향별 실제 교환량 | 순수송이 0이어도 전환 | 현재 incoming 장부와 가까움. flux 표현 방식 자체가 모델 일부 |

| edge의 순수송량 | 반대 방향이 상쇄되면 전환 없음 | 상쇄에 대해 불변. 교환·혼합으로 생기는 효과를 버릴 수 있음 |

| 연속 표면 flux vector의 크기 | 복원한 순방향 이동을 기준 | 좌표 독립 모델의 출발점. 복원·기작 분리·비음수 갱신 필요 |

| 체류 노출 | 제자리에 있어도 전환 가능 | 기존 continuous reaction과 가까움. ‘새로 이동한 물만 남긴다’와 다른 의미 |



어느 하나를 물리적으로 정답이라고 정하지 않는다. 예술적 제어에서도 규칙의 의미는 선택해야 한다. 모델 변경 후 이전 preset과 다른 결과가 나온 것을 계산 오차 개선으로만 보고해서는 안 된다.



#### 4. 거리 보정을 해도 남을 수 있는 방향 의존성



##### 4.1 네 이웃 제한 모델의 국소 유도



2D 정사각 격자 간격 h, cell area h², 동일 capacity density, static geometry·Gravity/Decay OFF·비활성 limiter를 가정한다. 단위를 정규화해 q_i=h²s_i라 하고



$$

F_{j\to i}=D\Delta t\,(s_j-s_i)_+

$$



인 네 축방향 이웃을 쓴다. 이는 코드의 saturation positive-part 계열을 단순화한 모델이다. **현재 여덟 방향/UV seam/동적 weight 전체를 그대로 표현한 것이 아니다.**



cell 주변의 smooth affine field가 `s(x,y)=s₀+g_x x+g_y y`이면 정확히



$$

I_i=D\Delta t\,h(|g_x|+|g_y|).

$$



거리 보정 `f(h)=1−e^(−κh)=κh+O(h²)`를 적용한 잔류의 면적당 초기 생성률은



$$

\frac{\Delta a_i}{h^2\Delta t}

=\kappa D(|g_x|+|g_y|)+O(h).

$$



한편 반응 없는 수송의 정미 연산은 표준 discrete Laplacian이며 smooth limit에서 DΔs를 근사한다. **수송 쪽의 등방적인 극한과 incoming 전환의 등방성은 별개**다.



같은 크기의 gradient가 축과 나란하면 위 leading term은 κDg이고, 45도 방향이면 κD√2g다. 이는 한정한 네 이웃 모델의 분석값이다. MDSSP의 관측 오차가 √2배라고 주장하지 않는다. 한 Step/국소 생성률 비교이며 이후의 반응·분포까지 같게 유지한 실험도 아니다.



##### 4.2 여덟 방향이면 자동 해결되는가



고정된 유한 방향과 양의 가중치에서 비슷한 구성은 `Σ_d b_d |d·∇s|` 형태의 방향 함수를 만든다. 상수 b_d에 대해 이는 piecewise linear 함수이므로 2D의 모든 방향에서 비례상수×Euclidean norm과 정확히 같을 수 없다. 방향 수 증가로 차이는 작아질 수 있지만 자동으로 없어지는 것은 아니다.



이것도 알려진 stencil/anisotropy 성질의 적용이다. 실제 weight가 gradient에 적응하거나 reconstructed gradient의 norm을 쓰는 방법까지 불가능하다고 말하는 것이 아니다. 비정규 UV metric·동적 coefficient의 경우에는 별도 유도가 필요하다.



**실무적 함의:** 해상도 비교에 chart 회전/격자 방향 비교를 따로 포함해야 한다. 재질 자체의 의도된 방향성과 UV sampling에서 생긴 방향성도 구분해야 한다.



#### 5. 여러 반응을 각각 보정하면 조합도 일관될까



##### 5.1 두 개의 경쟁 전환에 대한 반례



동일한 이동 물량 Q를 소비하는 `W→A`, `W→B` 두 rule에 각각 거리 지수 fraction을 넣는 안을 생각한다. 두 계수는 κ로 같고, 다른 공급·반응·잔류 이동은 없다고 한다.



전체 경로 길이 ℓ에 대해 `t=exp(−κℓ/2)`라 두고 `1/√2<t<1`로 제한하면 아래 계산에서 공급 limiter는 활성화되지 않는다.



- 전체 구간에서 각 rule의 fraction은 `1−t²`이고 남는 물은 `Q(2t²−1)`.

- 반 구간마다 각 rule의 fraction은 `1−t`이고 두 구간 뒤 남는 물은 `Q(2t−1)²`.



두 결과 차이는



$$

Q\big[(2t-1)^2-(2t^2-1)\big]=2Q(t-1)^2>0.

$$



즉 **각 규칙을 단독으로는 분할 일관되게 보정해도, 같은 물량에 대한 병렬 request를 합치면 그 성질이 유지되지 않는다.** 이것은 제안된 거리 보정 방식의 반례다. 현재 고정 fraction 시스템의 오류를 재현했다고 기록하지 않는다.



##### 5.2 표준 경쟁 반응 해법이 해결하는 범위



상수 1차 거리 반응 `dW/dℓ=−(κ_A+κ_B)W`, `dA/dℓ=κ_AW`, `dB/dℓ=κ_BW`를 선택하면 K=κ_A+κ_B>0에 대해



$$

W_{out}=Qe^{-K\ell},\quad

\Delta A=Q\frac{\kappa_A}{K}(1-e^{-K\ell}),\quad

\Delta B=Q\frac{\kappa_B}{K}(1-e^{-K\ell}).

$$



이 통합 전환은 분할에 대해 합성된다. **병렬 1차 반응의 표준 해이며 신규 알고리즘이 아니다**(R55). 상수 선형 반응망이면 matrix exponential 같은 기존 방법이 다음 대조다.



추가 reactant의 공급 경쟁, nonlinear modifier, 서로 다른 이동/체류 기준, 동적 Geometry에서는 위 식을 그대로 적용할 수 없다. 하지만 이 복잡함만으로 새 Contribution이 생기지는 않는다. 기존 reaction network solver와 operator splitting 분석(R56)을 먼저 상대해야 한다.



#### 6. 표준 방법이 제공하는 더 강한 대안



등방적인 거리 노출을 원한다면 연속 표면 flux J를 기준으로



$$

\partial_t\rho+\nabla_S\!\cdot J=-\kappa\|J\|,

\qquad\partial_t a=\kappa\|J\|

$$



같은 모델을 먼저 비교할 수 있다. 이는 **미채택 모델 제안**이며 현 코드와 같은 계산이 아니다. J가 net flux인지 기작별 flux인지도 정의해야 한다. 접촉 Input의 사건 전환과 정지한 상태의 continuous reaction은 별도로 둔다.



J=−D∇ρ, D 상수인 평면의 제한 경우에는



$$

\partial_t\rho=D\Delta\rho-\kappa D\|\nabla\rho\|

$$



가 되어 기존 **gradient absorption / viscous Hamilton–Jacobi** 계열과 연결된다(R57–R58). 따라서 ‘flux 크기로 반응한다’는 식을 발명한 것처럼 주장할 수 없다. 여기서는 표준 대조를 더 강하게 만든 것이다.



이 모델을 GPU에 넣기 위해 필요한 positivity·면적 수지·불연속 재질/경계·입력 분할·Geometry 결합은 아직 설계/검증하지 않았다. 임의 dt의 명시 sink를 쓰면 저장량보다 많이 빼는 문제가 생길 수 있으므로 식 하나만으로 안정적 solver가 완성됐다고 하지 않는다. κ의 단위는 inverse length이며 임의 예술 계수라 해도 그 의미를 유지해야 한다.



또한 dynamic Geometry를 바꾸면 계수·방향도 변하므로 reaction/transport 연산 순서가 영향을 준다. 일반적인 splitting 비가환성은 기존 문제다. 단순 step 세분화·Strang·기존 implicit coupling으로 충분한지 먼저 비교한다. Geometry update를 한 번 더 하는 것 자체를 새 방법으로 세지 않는다.



#### 7. 남는 연구 후보를 좁힌 명세



**후보 주장 — 아직 가설:**



> 사건·이동·체류에 의해 유발되는 표면 전환의 의미를 분리하고, 재질별 반응 경쟁과 State-Driven Geometry Coupling에서도 선택한 의미를 일관되게 적용하는 표면 모델 및 제작 규칙을 제시한다.



이 문장만으로는 범용 framework의 통합 주장과 같다. 논문이 되려면 다음 중 하나 이상의 실질적인 차이가 필요하다.



- **방법 기여:** 기존 보존 수송+반응 solver로는 같은 비용/제어 조건에서 유지하기 어려운 의미 일관성을, 구체적인 새 discrete coupling으로 해결.

- **시스템 기여:** 사건/거리/체류 선택이 중요하고 기존 제작 방식의 반복 조정을 줄인다는 비교를 확보. 모든 시스템에 같은 규칙 의미를 허용한 대조 필요.

- **분석 기여:** 특정 MDSSP만의 결함 수정이 아니라 여러 합리적 수송/반응 구성에서 반복되는 실패와 적용 가능한 해결/선택 기준을 재현 가능하게 정리. 위 작은 반례들만으로 충분하다고 보지 않음.



현재는 세 주장 모두 입증 전이다. 단순 가중 gather·병렬 kinetics·기존 FV가 충분하다면 모델 개선으로 채택할 수 있어도 독립적인 SIGGRAPH 알고리즘 기여 평가는 낮춘다.



#### 8. 향후 가장 작은 판별 묶음



| 구분 | 고정할 것 | 비교/관측 | 현재 상태 |

|---|---|---|---|

| B0 단일 경로 분할 | 경로·총거리·입력·단일 rule | 기존 fraction vs 거리 보정, 최종량·공통 좌표 잔류 | 대수식만 있음 |

| B1 균형 교환 | 같은 순수송, 충분한 공급 | directed/gross vs net 의미의 반응 차이 | 제한 수지 반례만 있음 |

| B2 방향 회전 | 같은 연속 gradient·재질·표면 | 격자 방향별 생성률, reconstructed-gradient 대조 | 네 이웃의 유도만 있음 |

| B3 경쟁 rule 분할 | 같은 두 반응·길이 | 개별 fraction vs joint kinetics | 대수식만 있음 |

| B4 결합 | 같은 선택 의미·입력·Geometry 모델 | metric FV+표준 반응/시간 분할 vs 새 방법이 필요할 경우 그 방법 | 미설계·미실행 |



순서는 B0–B3에서 표준 해법이 무엇을 해결하는지 고정한 뒤 B4다. 최종 비교는 같은 세계 좌표 관측, 시간 오차 분리, seam 연결 감사, 공통 단위·Profile 의미를 요구한다. 새 모델과 기존 preset의 결과 차이를 동일 모델 정확성 차이로 취급하지 않는다.



#### 9. 최신 판정



B는 이제 **hop 수 보정 하나**보다 넓은 구조적 문제를 갖는다. 순유량과 교환량의 구별, incoming 전환의 방향 의존성, 경쟁 반응의 합성 실패를 구체적으로 유도했다. 문제 정의의 근거는 강화됐다.



동시에 병렬 kinetics·gradient absorption PDE·기존 splitting이 강한 대조로 드러났다. 따라서 **문제가 더 명확해졌다는 이유로 해법 신규성도 높아졌다고 평가하지 않는다.** 지금 가장 가치 있는 다음 비실행 작업은 표준 모델로 해결되는 범위를 끝까지 정리하는 것이다. 그 이후에도 Geometry 결합·선언 규칙의 중요한 공백이 남을 때 새 알고리즘을 선택한다.



#### 후속: 신규 유입과 실제 소비 상한의 구분



[[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-21-constructive-controls-and-reaction-contracts|21]]은 여러 inflowFraction rule이 신규 유입보다 많이 소비할 수 있음을 현 loader/validator/packing/shader 경로에서 유도했다. 전체 보유량 제한은 지키므로 이를 곧바로 버그로 부르지 않는다. 신규 유입의 trigger 의미와 eligible-pool 의미, 기존 rule 불변을 요구하는 조합성의 불가능성, net flux를 유지하는 표준 상쇄가 다루는 범위를 구분했다. 새 모델은 구현하지 않았다.

---
