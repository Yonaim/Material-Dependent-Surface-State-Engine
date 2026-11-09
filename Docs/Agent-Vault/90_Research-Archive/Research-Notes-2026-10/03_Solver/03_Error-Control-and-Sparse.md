# 오차 제어와 Sparse 갱신

미래 잔류 반응 budget, 물·잔류 공동 오차, 형상 민감도, 저비용 충분조건과 열린 경계 한계를 묶었다. 실험으로 검증된 가속이나 새 알고리즘 주장이 아니다.

## 먼저 볼 결론

- 제한 모델에서 잔류뿐 아니라 물 분포의 오차를 묶는 상한과 계산 생략 충분조건을 유도했다. 실제 속도나 일반 정확도 보장은 아니다.
- 열린 경계에서는 내부 outgoing 합이 limiter를 통해 경계 Flux를 바꿀 수 있고, 물질 이동이 없어도 공유 형상이 외부 수송을 바꿀 수 있다.
- 단순 block 상한 합성은 느슨해 주력안에서 보류했다. 판정 비용이 fine 갱신만큼 크거나 대부분의 Step을 깨우면 이 방향을 축소해야 한다.

## 이 문서의 원본 절

- [[90_Research-Archive/Research-Notes-2026-10/03_Solver/03_Error-Control-and-Sparse#source-analysis-14-residual-budget-critical-review|30_Deep-Analysis/14_Residual-Budget-Critical-Review.md]]
- [[90_Research-Archive/Research-Notes-2026-10/03_Solver/03_Error-Control-and-Sparse#source-analysis-15-joint-state-error-and-geometry-bounds|30_Deep-Analysis/15_Joint-State-Error-and-Geometry-Bounds.md]]
- [[90_Research-Archive/Research-Notes-2026-10/03_Solver/03_Error-Control-and-Sparse#source-analysis-16-low-cost-certificates-and-sleep-horizons|30_Deep-Analysis/16_Low-Cost-Certificates-and-Sleep-Horizons.md]]
- [[90_Research-Archive/Research-Notes-2026-10/03_Solver/03_Error-Control-and-Sparse#source-analysis-17-open-boundary-contracts-and-composition|30_Deep-Analysis/17_Open-Boundary-Contracts-and-Composition.md]]

원본 25개 파일의 수정 전 사본과 SHA-256 목록은 `Original-25-Notes.zip`에 있다. 아래 절은 원문의 수식과 당시 판단을 보존하며, 링크와 제목 수준만 새 위치에 맞췄다.

---

## source-analysis-14-residual-budget-critical-review

원본: `30_Deep-Analysis/14_Residual-Budget-Critical-Review.md`

### 미래 반응 오차를 제한하는 계산 생략 — 연구 고찰

작성: **2026-10-28 / Asia/Seoul**. 사용자 요청: 문서 작성 및 해당 부분의 고찰, 이어서 작업. ‘해당 부분’은 직전 결론인 **내부 계산을 생략해도 앞으로 생길 반응 결과의 차이를 얼마나 제한할 수 있는가**로 해석했다.

이 문서는 [[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#source-analysis-12-exact-submodels-and-limits|최소 모델의 유도]]와 [[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#source-analysis-13-method-blueprints-and-decision|방법 초안]]를 검토하고 추가 추론을 기록한다. **코드 읽기·문헌 확인·기호 유도·문서 작성만 수행했다. 앱/GPU/빌드/수치 실험/학습 미실행, 제품 코드 변경 없음.** 코드 기준 HEAD는 `880af1ebcffe601f0bac27413cad94055ce92812` 및 기존 작업 트리다. 아래 [D]는 조건부 이상 산술 유도이며 신규성/실측/기계 검증의 증거가 아니다.

#### 1. 현재 판단

유망한 지점은 ‘새로운 잔류량 공식을 만들었다’가 아니라 **허용된 미래 변화 아래 계산 생략의 영향을 제한하면서, 그 판정을 실제 계산보다 싸게 수행할 수 있는가**다.

다만 지금의 bound는 주로 물→잔류의 단일 전환에 대한 것이다. 잔류 오차가 작아도 물의 경로·Ice 융해·Heat·Geometry 오차가 클 수 있다. 따라서 현재 식만으로 MDSSP 전체를 잠재우는 것은 정당화되지 않는다.

이번 분석에서는 (i) 외부 유입 budget, (ii) 중력/비대칭 flux에도 쓸 수 있는 경로 potential, (iii) 순환 graph에서 bound가 무의미해지는 조건, (iv) 유한 시간에서 이를 완화하는 식, (v) 잔류량 bound와 두 실행 간 오차 bound의 차이를 구체화했다. 기본 원리는 기존 수지·Bellman 부등식이며 새 수학으로 세지 않는다.

#### 2. 세 가지 다른 주장을 구분한다

| 주장 | 필요한 근거 |
|---|---|
| 앞으로 생성될 잔류량이 B 이하 | 한 trajectory의 비음수 누적량과 potential/수지 |
| 계산을 생략한 실행과 reference의 잔류량 차이가 B 이하 | 두 trajectory 모두에 유효한 공통 가정·입력 범위·bound |
| 생략해도 시각적/다중 State 결과가 허용오차 안 | 물·잔류의 위치·다른 State·Geometry·후속 반응까지의 오차 전파 |

이전의 닫힌 모델에서는 잔류를 그대로 두는 근사와 비교하므로 첫 번째가 두 번째로 바로 이어졌다. 열린 시스템에서 경계 유입과 반응을 일부 계속 계산하면 그 연결을 다시 증명해야 한다. 세 번째는 어느 경우에도 자동으로 따라오지 않는다.

**가장 간단한 반례:** f=0이면 잔류 생성량은 항상 0이지만 물은 얼마든지 이동할 수 있다. f가 작아도 같은 문제가 남는다. 잔류 bound만 만족했다는 이유로 물 수송을 멈추면, 목표 관측 하나만 맞고 실제 표면 거동은 틀릴 수 있다.

#### 3. 외부 유입을 넣으면 닫힌 bound는 어떻게 바뀌는가

이전 saturation-only·단일 Incoming 전환 조건을 유지한다. block의 현재 총 물 M₀, 총 Capacity CΣ, 미래 기간의 gross boundary inflow 상한 I_max를 둔다. 내부 초기 saturation과 모든 외부 경계 saturation의 검증된 하한을 l이라 하고, 같은 convex-combination 조건이 성립한다고 하자. 직접 입력·다른 반응은 우선 제외한다.

수지는 \(M_t+A_t=M_0+A_0+I_t-O_t\)이고 \(M_t\ge lC_\Sigma\), \(O_t\ge0\)이므로

\[
0\le A_t-A_0\le M_0+I_{\max}-lC_\Sigma.
\]

이것은 조건부 열린 영역 상한이다. 외부 saturation이 0까지 가능하면 l=0이 되어 거의 총 공급량 상한만 남는다. I_max도 reference의 실제 미래 유입을 미리 실행해 얻는 값이면 예측 알고리즘의 공짜 입력이 아니다.

State-Driven Geometry Coupling에서는 내부 생략이 외부 flux까지 바꿀 수 있다. 따라서 기존 실행에서 관측한 boundary flux를 이후에도 그대로 쓰는 것은 일반적으로 정당하지 않다. 외부 입력을 고정한 별도 문제를 풀거나, **reference와 근사 모두를 포함하는 경계 범위**를 별도로 유지해야 한다.

이 식은 외부 유입을 고려할 수 있다는 출발점이지만, 실제로 좁고 싸게 계산되는 bound라는 증거는 없다.

#### 4. 중력 이동도 허용하는 별도의 경로 potential

##### 4.1 실제 이동량을 사용한 정식화

이 절은 saturation maximum principle에 의존하지 않는다. 내부 edge의 **공급량 제한 이후 실제 이동량**을 Fᵢⱼ≥0, 물을 qᵢ, recipient j의 유입 전환 비율을 0≤fⱼ≤1이라 한다. 유일한 물질 반응은 물→immobile 잔류의 1:1 전환이고 외부 입력/Decay/다른 반응은 우선 없다.

각 source의 총 outgoing≤qᵢ이고, 받은 양을 같은 Step에 다시 보내지 않는다고 가정한다. 이는 [SurfaceSolverPass2.comp](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Solver/SurfaceSolverPass2.comp:148)의 기존 State 기반 outgoing과 [reactionRequest](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Solver/SurfaceReaction.comp:18)의 Incoming 전환에 대응한다. Geometry가 F를 어떻게 바꾸는지는 고정하지 않는다.

모든 미래에 허용되는 edge i→j에 대해 0≤φᵢ≤1이고

\[
\boxed{\phi_i\ge f_j+(1-f_j)\phi_j}
\]

를 만족하는 potential을 고른다. \(V=\sum_i\phi_iq_i\), 총 잔류를 A라 두면

\[
\Delta(V+A)=\sum_{i\to j}F_{ij}
\{f_j+(1-f_j)\phi_j-\phi_i\}\le0.
\]

따라서 닫힌 영역에서 \(A_t-A_0\le V_0\). source별 outgoing limiter가 활성화돼도 실제 F가 동일하게 차감/가산되고 공급량 조건을 지키면 이 수지는 유지된다. FP/cache 오차는 별도다.

이 bound는 중력·비대칭 계수·변하는 Geometry를 허용할 수 있지만, **그 변화로 생길 수 있는 모든 이동 방향**에 potential 조건이 성립해야 한다. 현재 활성 edge만 검사하고 무기한 보장할 수는 없다.

##### 4.2 비순환 경로에서는 의미 있는 값이 나온다

고정된 허용 edge graph가 DAG이고 모든 f가 같다고 하자. 앞으로 최대 L번 이동할 수 있는 node에서

\[
\phi=1-(1-f)^L
\]

를 쓰면 충분하다. 더 움직일 수 없는 sink에서는 φ=0이다. sink에 들어오는 순간의 전환 f는 직전 edge에서 이미 계산한다.

예를 들어 f=0.1, 최대 두 번 이동하는 위치의 물은 현재 양의 0.19 이하만 이후 잔류로 바뀔 수 있다. **기호식의 대입 예이며 실험 결과가 아니다.** 이 결과는 체류 시간이나 이동 속도를 예측하지 않고 누적량만 제한한다.

##### 4.3 순환을 모두 허용하면 bound가 붕괴한다

한 directed cycle을 따라 부등식을 합성하면

\[
\phi_i\ge1-\Pi+\Pi\phi_i,
\qquad\Pi=\prod_{j\in cycle}(1-f_j).
\]

cycle에 양의 f가 하나라도 있으면 Π<1이어서 φᵢ≥1이다. φ≤1과 합치면 cycle 및 그곳으로 도달할 수 있는 node의 φ는 1이 된다. 결과는 V=M, 즉 전체 물량이라는 자명한 상한이다.

이것은 현재 saturation spread에 중요한 반론이다. 미래의 상대 saturation을 제한하지 않고 이웃 양방향 이동을 모두 허용하면, 실제로는 계속 왕복하지 않는 경우에도 이 보수적인 graph 모델은 cycle을 포함한다. **동적 형상을 전부 허용한 대가로 bound가 쓸모없어질 수 있다.**

따라서 이 무한시간 potential만으로 범용 MDSSP 생략 알고리즘을 만들겠다는 방향은 추천하지 않는다. 가능한 돌파는 허용 방향을 증명된 구간으로 한정하거나, 아래처럼 유한 시간과 outgoing 비율을 함께 사용하는 것이다.

#### 5. 유한 시간·공급량 비율을 포함하면 더 나아갈 수 있다

##### 5.1 시간별 potential

앞으로 h Step가 남았을 때의 potential을 φᵢ⁽ʰ⁾라 하고 φᵢ⁽⁰⁾=0으로 둔다. 한 Step 동안 source i에서 내부 edge로 나갈 수 있는 총량이 **모든 허용 상태에서** αᵢqᵢ 이하라고 하자. 0≤αᵢ≤1이다. 안전한 기본값은 1이고, 더 작은 값을 쓰려면 rate·dt·Capacity·Geometry 범위로 증명해야 한다.

\[
b_j^{(h-1)}=f_j+(1-f_j)\phi_j^{(h-1)},
\]

\[
\boxed{\phi_i^{(h)}=\phi_i^{(h-1)}+
\alpha_i\max\left(0,\max_{j:i\to j}
\{b_j^{(h-1)}-\phi_i^{(h-1)}\}\right).}
\]

outgoing이 없는 node의 max 항은 0이다. 현재 가지고 있는 물을 그대로 둘 때의 potential에, 최대 αᵢ만큼을 가장 큰 증가를 주는 edge로 보낼 수 있다고 완화한 식이다. 실제 flux 분배보다 자유로운 선택을 허용하므로 상한이다.

한 Step 뒤 potential과 그 Step의 잔류 증가를 합하면

\[
A_{n+1}-A_n+\sum_i\phi_i^{(h-1)}q_i^{n+1}
\le\sum_i\phi_i^{(h)}q_i^n.
\]

귀납적으로 H Step의 잔류 증가는 \(\sum_i\phi_i^{(H)}q_i^0\) 이하이다. 실제 수송이 비선형/Geometry 의존이어도 같은 허용 edge와 α 범위 안에 있으면 성립한다. 별도 물 생성/소비·여러 반응 경쟁은 포함하지 않는다.

모든 node에 공통 f와 공통 outgoing 상한 α를 적용하면 topology를 버린 더 단순한 상한은

\[
\boxed{A_H-A_0\le M_0\{1-(1-\alpha f)^H\}.}
\]

αfH가 작을 때는 무한시간 M₀ 상한보다 유용할 수 있고, H가 커지면 다시 M₀로 접근한다. 이 또한 표준 반복 감쇠/동적 계획법의 적용이지 새 정리가 아니다. 현재 기본 dt/FP16 경로에서 유용한 α를 확보한 것은 아니다.

##### 5.2 열린 경계의 유입 budget

Step t에서 외부가 node i로 보내는 gross incoming을 Iᵢᵗ라 한다. 그 물은 먼저 fᵢ만큼 반응하고, 남은 양은 이후 H−t−1 Step 동안 움직일 수 있다. 직접 contact input은 이 절에서 제외한다.

\[
A_H-A_0\le\sum_i\phi_i^{(H)}q_i^0+
\sum_{t=0}^{H-1}\sum_i
\{f_i+(1-f_i)\phi_i^{(H-t-1)}\}I_i^t.
\]

boundary outflow는 비음수 potential을 제거하므로 상한에서 버릴 수 있다. 실제 I를 미리 모르면 검증된 유입 budget으로 우변을 최대화한다. 단순한 gross 총 budget만 있으면 각 budget에 가능한 최대 가중치를 적용한다. 유입과 유출이 상쇄된 **순유량**을 I 대신 쓰면 안 된다.

이 식은 열린 영역·중력·동적 계수까지 다룰 수 있는 조건부 누적량 bound다. 그러나 경계 budget을 얻는 비용과 보수성은 해결하지 못했다. 외부가 같은 coupled simulation의 일부라면 두 실행 모두에 유효한 budget을 만들어야 한다.

##### 5.3 어떤 원리가 기존인가

[P] 누적 cost/reward를 남은 value와 합쳐 제한하는 방식은 Bellman 부등식과 potential 방법에 속한다. Bertsekas–Tsitsiklis 및 Rosenberg 등의 관련 결과 [R34–R35]를 대조했다. 본문은 결정론적 양의 물량 장부에 대해 직접 증명했으며, MDSSP를 확률적 MDP라고 가정하거나 해당 논문의 학습 알고리즘을 구현한 것이 아니다.

새 기여가 되려면 이러한 표준 부등식을 소개하는 것 이상으로, **실제 MDSSP의 상태·형상·입력 범위에서 충분히 작은 budget을 저비용으로 유지하는 새 방법**이 필요하다.

#### 6. 이 상한이 실제 생략 오차를 보장하는 조건

같은 초기 분포에서 시작한 reference와 근사에 대해, 두 실행의 총 잔류 증가가 모두 [0,B] 안이라는 공통 인증이 있으면

\[
|A_{\rm reference}(H)-A_{\rm approx}(H)|\le B.
\]

잔류는 각 texel에서 비음수로만 증가하므로 **공간 L1 차이**에는 느슨하게 2B가 필요하다. 근사가 잔류를 완전히 고정하는 경우에는 B다. 총량 차이와 공간 차이를 같은 지표로 쓰지 않는다.

공통 인증에는 다음이 필요하다.

- 같은 초기 물/잔류 상태, 고정된 f와 반응 의미.
- 두 실행 모두 source budget·비음수·장부 일관성 유지.
- 실제 및 근사 flux 모두 허용 edge/α 범위 안에 포함.
- 외부 incoming은 두 실행을 포함하는 공통 범위로 제한.
- 미포함 Reaction·input·모델 변경이 일어나기 전에 인증 만료/재판정.

여전히 **물 상태·외부로 나간 양·다른 State·Geometry 보호**는 별도다. 잔류가 작다는 이유로 정상적인 물의 이동을 생략하는 오류는 이 인증이 막아 주지 못한다.

#### 7. ‘다시 켜면 된다’가 해결하지 못하는 문제

새 입력에서 block을 깨우는 것은 이후 계산을 시작할 뿐, 생략 동안 누락된 이력이나 외부로 전파된 오차를 지우지 않는다. 특히 다음을 구분해야 한다.

1. **메모리 축소:** fine 분포를 폐기한다. 재구성/기록/replay 문제가 생긴다.
2. **연산 생략:** fine 분포를 저장하지만 일부 연산을 건너뛴다. 복원 불확실성은 줄어도 저장된 상태가 reference와 다른 것은 남는다.
3. **정확한 수면:** 앞으로 아무 변화가 없음을 증명한다. 반응·중력·경계·Geometry 경로 모두 inactive여야 한다.

첫 후속 방법은 2부터 검토하는 것이 현실적이다. 메모리 절감을 함께 주장하지 않는다. 매 재활성화 때 이전 오차를 0으로 리셋해서는 안 되고, reference를 포함하는 상태 범위나 누적 오차 장부를 이어가야 한다. 이 때문에 실제 구현은 단순 dirty flag보다 복잡해질 수 있다.

여러 block의 scalar budget도 무조건 더할 수 없다. 경계 flux의 차이가 다음 block의 반응을 증폭하거나 다른 State의 threshold를 넘길 수 있다. local-to-global 조합에는 interface flux 오차·State/Geometry 전달 영향의 추가 분석이 필요하다.

#### 8. 문서에서 제안하는 가장 작은 연구 목표

> 유입 전환을 포함하는 표면 수송에서, 저장된 fine State를 유지하면서 제한된 시간 동안 일부 내부 계산을 생략하고, 지정한 반응량과 수송 관측의 오차 budget을 유지할 수 있는가?

첫 범위는 단일 수송 State+immobile 잔류, 고정 Capacity, 명시적 시간 H와 외부 입력 범위다. 동적 Geometry를 포함할 때도 가능한 coefficient/방향의 범위를 제한한다. Heat–Ice–Waterfilm 전체를 처음부터 해결하지 않는다. 이 제한이 실제 제작 과제로서 의미가 없으면 연구 범위를 억지로 유지하지 않는다.

필수 관측은 **총 잔류뿐 아니라 물 분포 또는 boundary flux**를 포함해야 한다. 그래야 f가 작다는 이유만으로 모든 물 수송을 건너뛰는 무의미한 성공을 방지한다. 정상 작동하는 fine Sparse, 고정 주기 갱신, State/flux 변화량 threshold, 같은 오차 추정법을 준 표준 adaptive 방법과 비교해야 한다.

#### 9. 학술적 의미와 가장 강한 반론

학술적 의미가 생길 수 있는 이유는 활동량이 작다는 국소 관찰과 **미래에 중요한 결과가 남아 있는지**가 다를 수 있기 때문이다. 내부 이동이 순수지에서 지워져도 잔류는 남고, 변화한 형상이 이후 경로를 바꾼다. 지정 결과의 오차를 기준으로 계산을 배분하는 방법은 제작자가 허용 가능한 품질과 비용을 연결할 수 있다.

그러나 반론도 강하다.

| 반론 | 현재 판단 |
|---|---|
| 기존 a posteriori/goal-oriented adaptivity의 적용 아닌가? | 맞을 가능성이 높다. 특수한 장부/결합 때문에 필요한 실질적 새 계산을 보여야 함 |
| 잔류만 맞추고 물의 경로는 망가뜨리는 것 아닌가? | 현재 bound만으로 막지 못함. 추가 관측·interface 오차 필수 |
| 실제 모델은 중력·여러 반응·열린 경계인데 조건이 너무 좁지 않은가? | 유효한 지적. 이번 유한시간 식은 일부 확대했으나 일반 다중 반응은 미해결 |
| 잠잘 정도로 변화가 작으면 기존 Sparse도 이미 충분하지 않은가? | 아직 반박 근거 없음. 실제 비용 비교 전 우수성 주장 불가 |
| bound가 늘 물 전체량 정도면 쓸모없지 않은가? | cycle/긴 H/큰 유입 budget에서 실제로 발생. 작게 유지할 방법이 핵심 |
| 인증 비용이 생략할 계산보다 비싸지 않은가? | 가능함. graph potential의 O(H·edge 수) 갱신을 매번 수행하면 이득을 잃을 수 있음 |

최소 비용 조건은 `인증 + 경계 처리 + 재활성화/오차 갱신 < 생략한 fine 계산`이다. 별도 전처리·replay·메모리 비용도 포함한다. 이 조건을 검증하지 않은 상태에서 속도 이점을 추정치로 제시하지 않는다.

#### 10. 최종 판단과 다음 분석의 경계

**발전시킬 연구 질문은 남아 있다. 현재 결과를 바로 Contribution으로 부를 수준은 아니다.** 이번 유한시간 potential은 중력/열린 경계로의 확장이 원리적으로 가능함을 보여주지만, 유용한 수송 오차와 저비용 budget 유지까지 해결하지 않았다.

다음 이론 과제는 ‘더 많은 새 수식’보다 다음 두 가지다.

1. 선택한 production subset에서 물 분포/경계 flux 오차를 잔류 budget과 함께 제한할 수 있는가? 전역 Lipschitz가 너무 크거나 threshold 때문에 무용해지지 않는가?
2. 이미 누적된 근사 오차를 재활성화 이후에도 감당하면서, 새로운 fine oracle 없이 budget을 갱신할 수 있는가?

이 두 문제에 답하지 못하면 범용 coarse 방법으로 확대하지 않는다. 현재 문서는 그 미해결점을 정리하는 연구 산출물이며, 실험 재개·제품 구현을 승인/예약하지 않는다.

---

## source-analysis-15-joint-state-error-and-geometry-bounds

원본: `30_Deep-Analysis/15_Joint-State-Error-and-Geometry-Bounds.md`

### 물·잔류 공동 오차와 동적 형상 민감도 — 실행 없는 후속 연구

작성: **2026-10-28 / Asia/Seoul**. [[90_Research-Archive/Research-Notes-2026-10/03_Solver/03_Error-Control-and-Sparse#source-analysis-14-residual-budget-critical-review|14의 미해결점]]인 ‘물 수송도 보호할 수 있는가, 판정 비용을 줄일 수 있는가’를 이어 검토했다.

**[C] 코드 읽기, [P] 문헌 확인, [D] 기호 유도, [H] 방법 제안만 수행했다. 앱·GPU·빌드·학습·수치 실험 미실행, 제품 코드 변경 없음.** 기준은 HEAD `880af1ebcffe601f0bac27413cad94055ce92812`와 기존 작업 트리다. 아래 식은 명시된 부분 모델의 이상 산술 결과이며, FP16/FP32·cache·실제 전체 Solver의 인증이나 신규성 증거가 아니다.

#### 1. 이번에 좁혀진 연구 질문

> 내부 수송을 일부 생략하거나 형상 갱신을 늦출 때, 물과 잔류의 **공간 분포 오차**를 함께 추적하면서 그 판정을 실제 갱신보다 싸게 유지할 수 있는가?

이번 분석의 진전은 다음과 같다.

| 결과 | 확보한 것 | 확보하지 못한 것 |
|---|---|---|
| 유입 전환을 포함한 한 Step 오차 | flux 차이로 물·잔류 공동 L1 오차 제한 | 전체 다중 Reaction·장시간 품질 보장 |
| 일부 raw edge 삭제 | limiter 재분배까지 포함하는 상한 2V | 삭제할 edge의 저비용 선택·GPU 이득 |
| 서로 다른 입력 상태의 오차 전파 | 고정 계수·dt 조건에서 증폭 상한, 비수축 반례 | 임의 동적 Geometry·limiter 활성 상태의 전역 상수 |
| 코드의 Normal 복원 구조 | 정적 최소제곱 계수로 높이→Normal 민감도 사전 계산 가능 | 전체 Concavity/중력 분기의 무조건적 보장 |
| 연구 범위 | 제한된 geometry-aware 연산 생략의 구체적 설계 요건 | 선행법보다 강한 새 알고리즘·실측 우위 |

‘오차를 제어하는 적응형 계산’ 자체는 이미 확립된 분야다. 새로 해결할 후보는 **유입 장부에 의해 반응이 달라지고, 공유 형상이 후속 flux를 바꾸는 현재 계산 모델에서 좁고 싼 오차 장부를 유지하는 방법**이다. 필요성과 비용 이점은 여전히 미입증이다.

#### 2. [D] 물과 잔류를 동시에 보호하는 한 Step 식

##### 2.1 가정과 실제 코드와의 대응

물 q와 이동하지 않는 잔류 a만을 고려한다. recipient j에서 Incoming의 고정 비율 f_j∈[0,1]을 1:1로 잔류로 바꾼다. 다른 반응·Decay·직접 입력·저장량 제거는 없다. 모든 edge는 한 번 차감한 양을 정확히 한 번 가산하며, source의 총 outgoing은 이전 물량 이하이다. 같은 Step의 Incoming을 다시 보내지 않는다.

이는 [Transport.glsl의 rawFlux](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Solver/Transport.glsl:1), [Pass 2의 incoming/outgoing 장부](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Solver/SurfaceSolverPass2.comp:148), [reactionRequest의 유입 전환](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Solver/SurfaceReaction.comp:18)을 제한한 모델이다. 실제 임의 Profile이 이 가정을 만족한다는 뜻은 아니다.

공급량 제한 이후의 실제 flux를 F_ij라 하면

\[
q_i^+=q_i-\sum_jF_{ij}+(1-f_i)\sum_jF_{ji},\qquad
a_i^+=a_i+f_i\sum_jF_{ji}.
\]

오차는 같은 단위의 저장량에 대해 \(E=\|\delta q\|_1+\|\delta a\|_1\)로 정의한다. 물/잔류의 **위치 차이도 포함**한다. Heat 등 단위가 다른 State를 이 norm에 그대로 더하지 않는다. 그런 확장에는 관측별 단위·가중치를 먼저 정해야 한다.

##### 2.2 같은 시작 상태에서 두 flux를 비교

시작 q,a와 f는 같고 F와 F̂만 다르다고 하자. δF=F−F̂, δO_i=Σ_jδF_ij, δI_i=Σ_jδF_ji이다. 그러면

\[
\boxed{\|\delta q^+\|_1+\|\delta a^+\|_1
\le\|\delta O\|_1+\|\delta I\|_1
\le2\sum_{i\to j}|\delta F_{ij}|.}
\]

첫 부등식은 물의 차이 −δO+(1−f)δI와 잔류의 차이 fδI에 삼각부등식을 적용하면 된다. (1−f)+f=1이므로 유입의 분배가 새로운 계수를 만들지 않는다.

이 결과는 F가 중력·동적 형상·limiter를 통해 계산돼도 **같은 시작 상태에서 실제 최종 F의 차이를 안다면** 성립한다. 두 실행의 다음 상태가 달라진 뒤의 전파는 별도 문제다. 열린 block만 다룰 때는 외부로 보낸 차이와 돌아오는 영향까지 장부에 넣어야 한다. 여기서는 우선 전체 닫힌 graph를 대상으로 한다.

##### 2.3 일부 raw edge를 삭제하고 limiter를 다시 계산하는 경우

같은 상태/형상에서 일부 raw flux만 0으로 만들고, 나머지 raw flux는 유지한다고 하자. source i의 원래 raw 합 S_i, 삭제한 raw 합 R_i, 남긴 합 T_i=S_i−R_i, 공급량 M_i=q_i이다. 원래 배율은 λ_i=min(1,M_i/S_i), 원래 삭제 대상의 **실제** flux 합은 V_i=λ_iR_i이다. S_i=0이면 모두 0으로 정의한다.

삭제 후 남긴 edge의 실제 outgoing 합은 min(M_i,T_i)가 된다. 원래 값 λ_iT_i에서 증가한 양을

\[
D_i=\min(M_i,T_i)-\lambda_iT_i
\]

로 두면 \(0\le D_i\le V_i\)이다. 원래 limiter가 비활성이면 D_i=0이고, 활성인 경우 T_i≥M_i이면 D_i=V_i, T_i<M_i이면 D_i=(1−λ_i)T_i≤V_i이다.

**edge별 절댓값만 합치면** 삭제 V_i와 재분배 D_i 때문에 2Σ|δF|≤4ΣV_i라는 느슨한 상한이 나온다. 하지만 source 장부를 먼저 합치면 더 강한 결과를 얻는다.

- source에 남는 추가 물은 V_i−D_i이다.
- 삭제된 recipient들에서 빠지는 공동 물/잔류량은 V_i 이하이다.
- 남긴 recipient들에 추가되는 공동 물/잔류량은 D_i 이하이다.

따라서 각 source의 기여를 합쳐

\[
\boxed{\|\delta q^+\|_1+\|\delta a^+\|_1
\le2\sum_iV_i\le2\sum_iR_i.}
\]

즉 **limiter 재분배를 무시하면 안 되지만, source별 취소를 이용하면 2V 상한을 유지할 수 있다.** 이 유도에는 모든 retained edge가 같은 source 배율로 증가한다는 구조가 필요하다. retained raw flux 자체를 근사하거나 Reaction의 공급량 경쟁·다른 State 소비를 추가하면 그대로 적용할 수 없다.

이것은 한 Step local defect이다. ‘매번 2V보다 작으므로 전체 실행도 2V 이내’라고 해석하지 않는다. 또한 원래 모든 flux를 계산한 뒤 작은 것을 버리는 구현은 계산 절감의 근거가 되지 않는다.

#### 3. [D] 보존한다고 오차가 수축하지는 않는다

##### 3.1 고정 계수의 충분조건

형상·재질 계수가 고정되고 cavity retention의 State 의존성이 없다고 하자. 비음수 q에 대한 flux를 다음으로 제한한다.

\[
F_{ij}=k_{ij}(q_i/C_i-q_j/C_j)_++\gamma_{ij}q_i.
\]

k와 γ는 dt를 포함한 비음수 상수, C_i>0이다. γ는 고정 형상의 geometry drive×rate×weight/C_i에 해당한다. 빠진 방향의 k,γ는 0으로 둔다. 다음은 편리하지만 필요조건은 아닌 충분조건이다.

\[
\Lambda_i={1\over C_i}\sum_j\max(k_{ij},k_{ji})+\sum_j\gamma_{ij},
\qquad\Lambda=\max_i\Lambda_i\le1.
\]

이 조건에서는 outgoing≤q_i여서 limiter가 비활성이고, 반응 전 보존 수송 T(q)=q−O(q)+I(q)는 componentwise 단조이다. 자기 성분의 최소 기울기가 1−Λ_i 이상이고 다른 성분에 대한 기울기는 비음수이기 때문이다. kink에서도 연속 piecewise-linear 단조성이 유지된다.

T는 총량을 보존하므로 L1 비팽창이다. 직접적으로 z=min(q,q̂)를 두면 Tz≤Tq,Tq̂이고

\[
\|Tq-T\widehat q\|_1\le\sum_i(Tq_i+T\widehat q_i-2Tz_i)
=\|q-\widehat q\|_1.
\]

이 논리는 기존 order-preserving/integral-preserving mapping 성질의 적용이다. [Nussbaum & Shustin 2001 원문 p.388 전후](https://sites.math.rutgers.edu/~nussbaum/Pubs/nonexpansiveJDDE.pdf)는 이를 Crandall–Tartar 결과로 설명한다. 새로운 정리로 주장하지 않는다.

##### 3.2 유입 전환을 추가하면 상수가 달라진다

같은 unordered pair의 양방향 saturation flux 차이를 합치면

\[
|\delta F_{ij}^{sat}|+|\delta F_{ji}^{sat}|
\le\max(k_{ij},k_{ji})|\delta(q_i/C_i-q_j/C_j)|.
\]

고정 gravity 항까지 합쳐 \(\|I(q)-I(\widehat q)\|_1\le\Lambda\|q-\widehat q\|_1\)이다. 전체 한 Step은

\[
\Psi(q,a)=(T(q)-fI(q),\ a+fI(q)).
\]

따라서 f_max=max_i f_i일 때

\[
\boxed{\|\Psi(x)-\Psi(\widehat x)\|_1
\le(1+2f_{max}\Lambda)\|x-\widehat x\|_1.}
\]

이 상한은 고정 계수·dt 조건의 두 State 모델에 한정된다. **각 상태에서 Λ≤1인 것만으로 State에 의존하는 k(q,a),γ(q,a)의 전역 상한이 되지 않는다.** 형상 변화로 인한 계수 차이를 추가해야 한다. limiter가 활성인 일반 영역의 Lipschitz 상수도 여기서 증명하지 않았다.

##### 3.3 오차가 실제로 커지는 두 texel 반례

같은 C=1, gravity=0, 양방향 k=β∈(0,1/2], 같은 f>0인 두 texel을 둔다. 물이 x>y일 때 낮은 쪽 y에만 ε∈(0,x−y)를 더한 두 초기 상태를 비교한다. 초기 잔류는 같다.

한 Step 후의 차이는

\[
\delta q_1^+=\beta\varepsilon,\quad
\delta q_2^+=[1-(1-f)\beta]\varepsilon,\quad
\delta a_2^+=-f\beta\varepsilon,\quad \delta a_1^+=0.
\]

초기 공동 오차 ε가 **(1+2fβ)ε**로 커진다. 물만의 L1 오차도 (1+fβ)ε이다. 반면 signed 총량 차이는 계속 ε이고, 두 실행 각각의 상태도 비음수이다.

이유는 낮은 쪽에 물을 더 넣으면 saturation 차이가 줄어 Incoming과 잔류 생성이 감소하기 때문이다. **총량 보존·비음수성과 실행 간 분포 오차의 비증폭은 서로 다른 성질**이다. 이 반례는 코드에서 사용하는 유입 반응의 구조를 드러내지만, 그 자체를 새 수치해석 원리나 SIGGRAPH Contribution으로 세지 않는다.

#### 4. [D] 여러 Step의 오차 장부

정확 모델 x_(n+1)=Ψ_n(x_n), 근사 모델 x̂_(n+1)=Ψ̂_n(x̂_n)을 비교한다. reference는 같은 fine 이산 모델이며 물리적 정답/PDE 해가 아니다. 공통 허용 영역에서 Ψ_n의 Lipschitz 상한 L_n과 같은 입력에서의 local defect δ_n이 있으면

\[
E_{n+1}\le L_nE_n+\delta_n,
\]

\[
E_H\le E_0\prod_{r=0}^{H-1}L_r+
\sum_{n=0}^{H-1}\delta_n\prod_{r=n+1}^{H-1}L_r.
\]

고정 계수 제한 모델에서는 위 L_n과 삭제 defect 2V_n을 사용할 수 있다. 정확 계산으로 돌아온 Step의 δ_n=0이어도 기존 E_n은 사라지지 않는다. block을 깨우거나 현재 근사 상태를 새 기준으로 삼았다는 이유로 오차 장부를 0으로 초기화할 수 없다.

매 Step 전역 최대 L을 곱하면 지나치게 보수적일 수 있다. node/State별 비음수 오차 전달 행렬을 유지하는 대안은 가능하지만 저장·갱신 자체가 큰 stencil 계산이 될 수 있다. **실용적인 상한의 폭과 유지 비용이 연구 문제로 남는다.** 이 recurrence 자체는 기존 적응형 수치해석의 표준 구조다.

#### 5. [C/D] 현재 Normal 계산은 일부를 사전 계산할 수 있다

##### 5.1 동적 Normal이지만 최소제곱 행렬은 정적이다

[StateMath.glsl::effectiveLocalNormal](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Solver/StateMath.glsl:116)을 다시 확인했다.

- tangent frame U,V,N은 **정적 macro normal**에서 만든다.
- 이웃 X,Y는 **정적 Positions의 차이**를 frame에 투영한다.
- 이웃의 유효성·macro normal 필터·LengthSquared 조건도 고정 mesh/profile/topology에서는 정적이다.
- 바뀌는 것은 meso+accumulation 높이 차이 DH이다.

그러므로 최소제곱 행렬 A_i와 determinant fallback 판정은 이 조건에서 정적이다. ‘모든 Geometry 최소제곱 행렬이 State에 따라 변한다’는 일반화는 이 함수에는 맞지 않는다. nonfinite 값 발생·mesh/profile/transform 변경은 별도이다. 뒤의 **동적 Concavity 함수**와 혼동하지 않는다.

유효 이웃의 정적 가중치 w_ij와 좌표 (X_ij,Y_ij)를 쓰면

\[
A_i=\sum_jw_{ij}\begin{bmatrix}X_{ij}^2&X_{ij}Y_{ij}\\X_{ij}Y_{ij}&Y_{ij}^2\end{bmatrix},
\quad v_{ij}=A_i^{-1}w_{ij}\begin{bmatrix}X_{ij}\\Y_{ij}\end{bmatrix},
\]

\[
g_i=\sum_jv_{ij}(h_j-h_i),\qquad
n_i(g_i)={N_i-U_ig_{ix}-V_ig_{iy}\over\sqrt{1+\|g_i\|_2^2}}.
\]

##### 5.2 높이 차이의 오차로 Normal 오차를 제한

정규직교 frame에서 g→n(g)의 Jacobian 최대 특이값은 1 이하이다. 따라서

\[
\boxed{\|\delta n_i\|_2\le\|\delta g_i\|_2
\le\sum_j\|v_{ij}\|_2|\delta h_j-\delta h_i|.}
\]

각 높이 오차가 ε 이하라면 오른쪽은 2εΣ_j||v_ij|| 이하이다. **v_ij는 사전 계산 가능**하므로 bound를 만들기 위해 매번 최소제곱 역행렬을 다시 구할 필요가 없다. 하지만 이웃별 bound 합산·메모리 접근은 남는다.

공통 높이 오차는 이 Normal 식에서 상쇄된다. 따라서 절대 높이 차이만 보는 Dirty threshold보다 **높이 오차의 이웃 차이**가 더 적절한 판정량일 수 있다. 다만 curved macro normal을 따라 같은 높이를 더하면 위치 간 거리는 바뀔 수 있으므로, 모든 수송 항이 불변이라는 뜻은 아니다.

determinant fallback이 정적으로 선택되면 이 함수는 고정 Meso normal을 반환하므로 해당 branch의 높이에 대한 Normal 민감도는 0이다. 정상 branch라도 ill-conditioned A와 작은 texel 간격은 Σ||v_ij||를 크게 만들 수 있다. 해상도를 높여도 동일 ε가 동일 수송 오차를 보장하지 않는다.

고정 nonsingular NormalMatrix B에 의한 world normal에는 보수적으로

\[
\|\delta n_i^{world}\|_2\le2\,\operatorname{cond}_2(B)\|\delta n_i\|_2
\]

를 쓸 수 있다. singular transform·nonfinite·정규화 실패는 제외한다.

##### 5.3 State에서 높이로 가는 bound

모든 관련 State의 cavity contribution이 0이고 profile/면적/transform이 고정된 경우, [accumulationHeight](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Solver/StateMath.glsl:48)는 비음수 q에 대해

\[
h_i=h_i^{meso}+\sum_k c_{ik}\min(q_{ik},C_{ik}),\qquad
c_{ik}={Factor_{ik}\,Thickness_{ik}\over AreaScale_i}\,WorldToLocalHeight_i.
\]

따라서 \(|\delta h_i|\le\sum_kc_{ik}|\delta q_{ik}|\)이다. cap은 높이 민감도를 줄일 수 있지만, 그 위의 숨은 저장량이 미래 반응에 영향을 주지 않는다는 뜻은 아니다. [IceReactionLab Profile](C:/Users/yonai/Documents/MDSS_Engine/Assets/SurfaceProfiles/IceReactionLab.SRProfile)은 cavity 관련 계수가 0인 예다. 그 Profile의 **전체 Ice/Heat 반응까지 §2–4에서 증명한 것은 아니다.**

#### 6. [C/D] Transfer Weight와 중력에 연결할 때의 조건

##### 6.1 Normal·거리 가중치

[buildTransferWeight](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Geometry/SurfaceDynamicWeightsUpdate.comp:98)의 NormalWeight는 clamp(dot(n_i,n_j),0,1)이므로

\[
|\delta W_N|\le\|\delta n_i\|_2+\|\delta n_j\|_2.
\]

DistanceWeight는 clamp(R/d,0,1), R=(meanDistance_i+meanDistance_j)/2이다. 두 실행의 관련 이웃이 동일하게 유효하고 모든 거리 d≥d_min>GeometryEpsilon, R≤R_max이면

\[
|\delta W_D|\le{|\delta R|\over d_{min}}+
{R_{max}|\delta d|\over d_{min}^2}.
\]

단위 macro normal과 고정 ModelLinear M에서 \(|\delta d_{ij}|\le\|M\|_2(|\delta h_i|+|\delta h_j|)\). meanDistance의 이웃 집합이 같으면 그 오차는 거리 오차의 평균으로 제한한다. 실제 [Geometry Update](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Geometry/SurfaceDynamicGeometryUpdate.comp:31)는 epsilon보다 작은 거리를 제외하므로 **유효 이웃 수의 변경이 없다는 조건**을 확인해야 한다.

모든 인수가 [0,1]이고 Profile boundary 항은 고정이므로 곱 가중치의 차이는 각 가중치 차이의 합으로 제한할 수 있다. FP16 가중치 양자화는 별도 오차 항이다.

##### 6.2 정규화한 접선 중력에는 추가 margin이 필요하다

[prepareSourceGeometry](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Solver/GeometryDrive.glsl:6)는 t=(I−nnᵀ)g를 **길이로 정규화**한다. 따라서 접선 중력이 거의 0인 영역에서는 Normal의 작은 변화가 중력 방향의 큰 차이로 이어질 수 있다.

두 상태에서 ||t||≥g_min>GeometryEpsilon이고 같은 유효 branch에 있다면

\[
\|\delta(t/\|t\|)\|_2\le {4\|g\|_2\over g_{min}}\|\delta n\|_2.
\]

world edge vector e, ||e||≥d_min, ||e||≤d_max, SolverUp=u를 두자. 코드의 drive는 |e·u| clamp(dot(t/||t||,e/||e||),0,1)이다. H_max=||u||d_max로 놓으면 충분한 상한은

\[
|\delta Drive|\le\|u\|\|\delta e\|+
H_{max}\left({4\|g\|\over g_{min}}\|\delta n\|
+{2\over d_{min}}\|\delta e\|\right).
\]

g_min→0이면 이 bound가 나빠진다. 이는 실제 결과가 항상 불연속이라는 증명이 아니라 **이 인증 방식이 해당 영역을 값싸게 다루지 못한다는 한계**다. 중력 OFF 또는 양쪽에서 확실히 비활성인 branch는 별도로 처리할 수 있다. 애매한 branch를 임의로 정상 branch로 취급하지 않는다.

##### 6.3 아직 남는 항

[buildDynamicConcavityWeight](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Geometry/SurfaceDynamicWeightsUpdate.comp:36)는 dynamic normal·dynamic position으로 frame/샘플/행렬을 만들고 fallback을 사용한다. §5의 정적 행렬 논리가 그대로 적용되지 않는다. 첫 제한 모델에서 cavityExitResistance=0 및 Decay/보호 비활성으로 **수송/손실의 Concavity 의존성을 제거**할 수는 있다. 그것은 범위를 줄인 연구이지 전체 기능의 인증이 아니다.

같은 상태 y에서 Geometry만 바뀌면 raw flux 계수 차이를 위 식들로 제한할 수 있다. 반면 두 trajectory의 상태와 Geometry가 모두 달라지면, 다음 두 항을 분리해야 한다.

1. 한 Geometry에서 x와 y의 상태 차이를 전파하는 항.
2. 같은 y에서 G(x)와 G(y)의 차이가 만드는 flux defect.

예를 들어 limiter 비활성인 허용 영역에서 두 번째 항의 edge flux 합을 Γ||x−y|| 이하로 증명했다면 공동 상태의 전파 상수로 L_frozen+2Γ를 쓸 수 있다. **현재 전체 모델의 유용한 Γ는 아직 확보하지 않았다.** State 저장량은 cap이 없으므로 saturation의 상한을 Geometry cap으로 대신할 수도 없다.

#### 7. [H] 저비용 판정 방법의 구체적 초안

다음은 구현되지 않은 설계다. fine 상태를 유지하면서 연산을 선택적으로 생략하는 방식으로 먼저 좁힌다. 이것만으로 coarse 메모리 절감은 주장하지 않는다.

1. **정적 준비:** topology·profile·면적·transform 조건, v_ij와 그 norm, 가능한 계수 상한을 준비한다. mesh/profile/transform 변경 시 유효성을 다시 판정한다.
2. **공통 범위:** reference와 근사 양쪽을 포함하는 State·높이·거리·중력 margin 범위를 유지한다. 현재 근사 상태의 min/max만으로 reference까지 포함한다고 가정하지 않는다.
3. **계산 전 flux 상한:** 예를 들어 saturation 항의 raw flux는 k_ij^max(s_i^upper−s_j^lower)_+ 이하이다. gravity 항은 γ_ij^max q_i^upper로 더한다. block 내부는 느슨한 공통 범위·미리 합친 계수로 상한을 줄 수 있고, 경계는 더 세밀하게 유지한다.
4. **삭제 제안:** 삭제할 edge의 raw 상한 합을 R_bound로 두어 같은 입력의 defect를 2R_bound 이하로 제한한다. source가 한쪽에서는 edge를 보내고 다른 쪽에서는 받지 않는 독립 삭제는 금지한다. §2의 장부 계약을 유지해야 한다.
5. **오차 전파:** 이전 오차와 Geometry 계수 차이, 새 삭제 defect, 저장 양자화 항을 합쳐 관측별 허용오차를 확인한다. 같은 오차 원인을 중복 계상하거나 누락하지 않도록 map을 먼저 고정한다.
6. **정확 갱신으로 복귀:** bound가 너무 크거나 branch 조건을 확인하지 못하면 해당 영역의 생략을 허용하지 않는다. 이전 오차는 유지하며, 장기 회복이 필요하면 신뢰된 checkpoint부터의 replay 등 별도 방법/비용이 필요하다.

저비용화의 핵심은 3을 실제 fine flux보다 싸게 수행하는 것이다. 매 Step 모든 edge의 높이·Normal·거리·flux를 먼저 계산하고 생략 여부만 결정하면 연구 목표를 놓친다. 대신 정적 계수와 block 범위를 쓰면 계산은 줄일 수 있지만 상한이 넓어져 생략이 거의 사라질 수 있다.

대략적인 필요조건은

\[
C_{audit}/H+C_{bound}+C_{boundary}+C_{wake}<C_{saved}.
\]

H는 감사/범위 재구축 사이의 Step 수다. 각 항은 **미측정**이며 이 부등식을 만족한다는 주장은 없다. 현재 활성 영역 Sparse가 이미 제거하는 빈 영역을 다시 제거한 이득으로 셈하지 않는다. 남은 표적은 **State가 존재하되 변화가 작거나 목표 관측에 영향이 작은 내부 영역**이다.

#### 8. 선행연구에 비추어 무엇이 남는가

문헌 확인 수준은 [[90_Research-Archive/Research-Notes-2026-10/04_Literature/01_Related-Work#source-reference-07-related-work|R36–R38]]에 기록했다.

- 단조·총량 보존으로 L1 비팽창을 얻는 논리는 기존이다.
- conservation-law adaptive multiresolution은 이미 flux 계산/표현을 줄이면서 근사 오차를 다룬다. 관련 원문 전체 접근은 제한돼 세부 적용 가능성 판단은 남는다.
- 반응–확산에서 threshold와 local time stepping을 함께 써서 계산과 오차를 관리하는 연구도 존재한다. [Bendahmane et al.의 저자 preprint](https://arxiv.org/abs/0807.1359)의 초록에서 확인했다. ‘Reaction이 있어서 기존 적응법은 못 쓴다’는 주장은 할 수 없다.

따라서 아래는 **새 Contribution으로 주장하지 않는다**: 2V 삼각부등식, Lipschitz recurrence, 정적 LS 민감도, interval bound, 느린 곳을 잠재우는 기본 아이디어.

그 이후에 남는 후보는 다음의 결합을 **표준 대조보다 실질적으로 잘 푸는 구체적인 방법**이다.

> 유입 경로에 따른 반응과 공유 형상 변화가 있는 표면 모델에서, 물·잔류·후속 수송에 중요한 오차를 추적하는 저비용 적응 갱신.

가장 강한 반론은 ‘기존 error-controlled adaptive finite volume에 geometry cache bound를 붙인 것이다’이다. 현 단계에서는 그 반론을 이겼다고 말할 수 없다. 기존 방법에 동일한 사전 계수·오차 정보·반응 장부를 허용한 대조가 필요하다.

#### 9. 세 방향의 현재 유력도와 중단 조건

| 방향 | 이번 연구가 추가한 것 | 현재 평가 |
|---|---|---|
| 3: 내부 반응·수송을 보존하는 축소/생략 | 물+잔류 분포 defect, 반응 비수축 반례, Normal 사전 민감도 | **알고리즘 탐색으로 가장 구체적. 신규성·실용성은 미입증.** 현 단계는 압축보다 연산 생략 문제에 가까움 |
| 2: 다른 현상을 보호하는 편집 | 형상 보존 제약/오차 평가에 쓸 수 있는 정적 민감도 구조 | 제작 목적과 가장 가까움. 이를 쓴 표준 optimizer보다 나은 해결법은 아직 없음 |
| 1: 미래 진행 역설계 | 목표 시점의 오차 장부를 방향2와 공유할 가능성 | 단독 기여 우선순위는 낮고, 방향2의 목표·평가로 유지 |

가장 작은 다음 이론 과제는 **cavity/Decay OFF, 단일 Incoming 전환, 제한된 transform·거리·중력 margin**에서, 위 구성의 Γ와 block 범위 갱신을 끝까지 연결하는 것이다. 여기서도 매 Step fine 계산과 같은 일이 필요하거나 bound가 대부분의 영역을 깨운다면 이 방법을 중단/수정한다.

이후 실제 실행이 허용될 때에만 품질–비용 대조를 수행한다. reference는 매 Step 전체 fine 갱신, 대조는 현 exact Sparse, 고정 주기/높이 threshold, 합리적인 적응형 flux/잔차 기준이다. 허용오차·관측·시간을 동일하게 하고 모든 준비/판정/재활성화 비용을 포함한다. 신규성 확인은 그와 별개의 문헌/방법 비교 과제다.

**현재 판정은 변하지 않는다:** 현 구현의 새로운 Contribution은 미확보다. 이번에는 ‘왜 오차 제어가 필요한가’의 반례와 ‘무엇을 사전 계산할 수 있는가’의 코드 근거가 추가됐고, 저비용 해법의 구체적 입증 과제가 줄었다. 이는 연구의 진전이지만 채택 가능성이나 실제 속도 향상의 증거는 아니다.

---

## source-analysis-16-low-cost-certificates-and-sleep-horizons

원본: `30_Deep-Analysis/16_Low-Cost-Certificates-and-Sleep-Horizons.md`

### 저비용 판정과 계산 생략 기간 — 불변 영역·이동률 기반 후속 분석

작성: **2026-10-28 / Asia/Seoul**. 사용자 ‘더 연구’ 요청에 따라 [[90_Research-Archive/Research-Notes-2026-10/03_Solver/03_Error-Control-and-Sparse#source-analysis-15-joint-state-error-and-geometry-bounds|15]]의 판정 비용 문제를 검토했다. **문헌·코드 읽기와 기호 유도만 수행했다. 앱·GPU·빌드·수치 실험·학습 미실행, 제품 코드 변경 없음.** 코드 기준은 기존 HEAD `880af1ebcffe601f0bac27413cad94055ce92812` 및 작업 트리다.

아래 [D]는 이상 산술에서 명시된 부분 모델의 결과, [C]는 코드 확인, [H]는 미구현 제안, [P]는 문헌 확인이다. 수식의 신규성·전체 MDSSP의 정확성·GPU 속도를 입증한 문서가 아니다.

#### 1. 이번 판단의 변화

15에서는 매 Step의 오차를 L·E+δ로 추적했다. 이번에는 **두 실행이 앞으로 머물 수 있는 공통 집합의 크기**로 오차를 제한한다. 이 방식은 매 Step 계수의 미분/민감도를 갱신하지 않아도 된다는 장점이 있지만, 집합이 넓으면 쓸모없는 상한만 준다.

이번에 확보한 것은 다음이다.

| 결과 | 조건부 진전 | 주요 제한 |
|---|---|---|
| 닫힌 saturation 수송의 공동 오차 ≤2B | 시간과 함께 커지지 않는 물·잔류 분포 상한 | 중력/다른 반응/외부 경계를 그대로 추가할 수 없음 |
| 이동률 기반 공동 오차 ≤2Σq₀[1−(1−α)^H] | 중력·동적 Geometry·outgoing limiter 허용, 미분 상수 불필요 | 단일 1:1 Incoming 전환, 닫힌 영역, 큰 α에서 느슨함 |
| 이동률 구간별 질량 요약 | 초기 감사 후 O(K) 크기의 기간 판정 | fine 상태는 유지, K 선택과 실제 비용은 미검증 |
| 기존 cap 판정 재확인 | 이미 구현된 생략을 후보에서 제거 | 새 스케줄러 필요성을 약화하는 근거 |
| 열린 경계 반례 | 어떤 사건이 기존 인증을 깨는지 특정 | 일반적인 GPU workgroup에 닫힌 영역 상한을 적용할 수 없음 |

이것은 ‘저비용 판정이 원리적으로 불가능하다’는 걱정을 제한 모델에서 줄인다. **유용한 기간이 실제로 얼마나 길며 기존 Sparse보다 이득인지**는 아직 모른다.

#### 2. [D] 공통 하한과 수지만으로 공동 오차를 제한

물 q_i≥0, 움직이지 않는 잔류 a_i≥0, 단일 1:1 Incoming 전환만 고려한다. 직접 입력·Decay·다른 반응·외부 유출입은 우선 없다. 초기 상태 q⁰,a⁰에서 출발한 reference와 근사 모두

\[
\sum_i(q_i(t)+a_i(t)-a_i^0)=M_0,\quad
a_i(t)\ge a_i^0,\quad q_i(t)\ge b_i\ge0
\]

를 만족한다고 하자. **같은 b_i가 두 실행 모두에 유효해야 한다.**

증분 벡터 \(z(t)=(q(t)-b,\ a(t)-a^0)\)는 비음수이고 총합은 \(B=M_0-\sum_i b_i\)이다. 두 비음수 벡터 사이의 L1 거리는 각 총합의 합 이하이므로

\[
\boxed{\|q(t)-\widehat q(t)\|_1+\|a(t)-\widehat a(t)\|_1\le2B.}
\]

공통 하한이 초기 q⁰ 이하이면 q̂=q⁰,â=a⁰로 동결하는 근사도 포함된다. 잔류의 **공간 분포**까지 포함하는 bound이며, 잔류 총량만의 상한이 아니다.

이 결과는 simplex의 지름과 수지에서 나온다. 새로운 수학적 원리가 아니다. 이전 L1 증폭 반례와도 모순되지 않는다. 가까운 두 초기 상태의 거리가 한 Step에서 늘어날 수 있어도, 같은 초기 상태에서 생긴 두 허용 trajectory가 들어갈 수 있는 집합의 크기는 제한될 수 있다.

#### 3. [D] saturation-only에서는 기간에 무관한 판정이 가능

[[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#source-analysis-12-exact-submodels-and-limits|12 §2.5]]의 조건을 따른다. 닫힌 graph, 고정 C_i>0, F_ij=k_ij(s_i−s_j)_+, s_i=q_i/C_i, k≥0이며 매 Step Σ_j max(k_ij,k_ji)≤C_i이다. k는 State/Geometry에 따라 변해도 된다. 단일 Incoming 전환·immobile 잔류 외의 반응/Decay/중력은 없다.

초기 최저 saturation l=min_i(q_i⁰/C_i)은 이후에도 하한이다. 따라서 b_i=lC_i를 §2에 대입하면

\[
\boxed{E_{joint}(t)\le2\left(M_0-l\sum_iC_i\right)=2B\qquad \text{모든 }t\ge0.}
\]

12의 물 bound와 잔류 bound를 따로 더하는 것보다 강할 수 있다. 잔류가 증가하면 남은 물의 초과량은 같은 만큼 감소하므로, 두 최대치를 독립적으로 계상할 필요가 없기 때문이다.

##### 계산 비용과 의미

초기에 M₀와 l을 한 번 집계하고 C 합을 준비하면, 조건이 유지되는 동안 E_joint≤ε인지 확인하는 데 새로운 fine flux가 필요하지 않다. 2B≤ε이면 해당 두 State의 계산을 동결해도 이 norm에 대한 판정은 시간에 따라 악화하지 않는다. 동적 형상이 k를 바꾸더라도 k의 비음수/상한 조건 안에 있으면 된다. **Geometry 미분 상수 Γ가 이 증명에는 필요 없다.**

물량이 작지 않아도 saturation이 거의 일정하면 B가 작을 수 있다. 반면 빈 texel 하나로 l=0이면 B=M₀가 되어 거의 쓸모없는 상한이 된다. 물이 퍼지는 전체 UV 영역에는 빈 texel이 많을 수 있으므로 이 한계는 중요하다.

이 방식은 q,a의 결과만 보호한다. 다른 State의 수송이 이 형상에 영향을 받아 달라지는 경우까지 보호하려면 그 State를 별도로 포함해야 한다. Surface 전체를 무조건 멈추는 인증이 아니다.

##### 두 texel의 정확한 sanity 식

같은 C, 고정 β∈(0,1/2], 같은 f, 중력/외부 입력이 없는 두 texel에서 실제 flux를 F₁₂=β(q₁−q₂)₊와 반대 방향 식으로 정의한다. d=q₁⁰−q₂⁰, ρ=1−(2−f)β일 때 H Step의 한 방향 누적 이동은

\[
S_H={|d|\over2-f}(1-\rho^H).
\]

초기 상태를 동결한 실행과의 공동 공간 L1 차이는 정확히 2S_H이다. source의 물 감소 S_H, recipient의 물 증가 (1−f)S_H, 잔류 증가 fS_H를 합한 것이다. B=|d|이므로 일반 상한 2B와 일치하는 방향의 sanity check가 된다. **기호 유도이며 실행 결과가 아니다.**

#### 4. [D] 중력까지 허용하는 유한 기간 하한

이번에는 saturation maximum principle을 사용하지 않는다. 같은 단일 Incoming 전환 모델에서 source의 실제 outgoing O_i가 모든 허용 상태에서

\[
0\le O_i^n\le\alpha_i q_i^n,\qquad0\le\alpha_i\le1
\]

을 만족한다고 하자. 남는 Incoming은 비음수이므로 q_i^(n+1)≥(1−α_i)q_i^n이다. 따라서 0≤t≤H 동안

\[
q_i(t)\ge q_i^0(1-\alpha_i)^H=:b_i(H).
\]

reference와 근사 모두 같은 α 범위 안에 있고 같은 초기 상태에서 시작하면 §2에서

\[
\boxed{E_{joint}(t)\le2\sum_iq_i^0\{1-(1-\alpha_i)^H\},\quad0\le t\le H.}
\]

이 식은 중력·비대칭 flux·State-Driven Geometry Coupling·활성 outgoing limiter를 허용한다. 단, 두 실행이 모두 비음수/수지/1:1 유입 전환을 지켜야 하며 α는 **현재 관측 flux가 아니라 모든 허용 미래의 상한**이어야 한다. 임의 다중 Reaction의 소비는 포함하지 않는다.

이 상한은 f에 의존하지 않는다. 잔류 생성만 보호할 때보다 더 강한 물 분포 요구를 함께 다루기 때문이다. H가 크거나 α가 1이면 2M₀의 자명한 상한으로 가까워진다. 반면 αH가 작은 짧은 기간에는 15의 반복 L·E+δ보다 유용할 가능성이 있다. 어느 것이 실제로 더 좁은지는 미검증이다.

#### 5. [C/D] α는 실제 fine flux를 계산하지 않고 구할 수 있는가

##### 5.1 저장량 cap에서 Geometry 범위를 먼저 얻는다

[accumulationHeight](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Solver/StateMath.glsl:48)는 저장 State를 Capacity까지만 형상에 반영한다. profile·mesh·transform·meso가 고정되고 모든 값이 유한하다고 하자. 채널 k에 대해 p_ik=ProfileCapacity_ik, F_ik=AccumulationFactor_ik, T_ik=ThicknessPerAmount_ik라 하면

\[
0\le h_i^{acc}\le H_i^{max}:=FillDepth_i+
WorldToLocalHeight_i\sum_k p_{ik}F_{ik}T_{ik}.
\]

이 상한은 cavity contribution도 포함한다. cavity height≤FillDepth이고, cavity 초과량의 following 기여는 cavity 가중 두께 합 이하이다. 비cavity following과 이를 합치면 ΣAmount·Thickness 이하이며 Amount≤pF이다. 적용되지 않는 채널/비양수 Factor는 합에서 제외할 수 있다.

이는 정확한 최대치보다 느슨할 수 있지만 **저장량 자체에 cap이 없어도 형상의 범위는 유한하게 제한되는 코드 구조**를 이용한다. 새 물리 부피 모델을 도입하지 않는다.

##### 5.2 중력 방향의 민감도 대신 크기 상한 사용

정적 meso 위치를 P_i^m, macro normal을 N_i, ModelLinear를 M, SolverUp을 u라 하자. 높이의 직사각형 범위에서

\[
D_{ij}^{max}=\max_{\eta_i\in\{0,H_i^{max}\},\eta_j\in\{0,H_j^{max}\}}
|u\cdot M(P_j^m+N_j\eta_j-P_i^m-N_i\eta_i)|
\]

를 사전 계산한다. affine 함수 절댓값의 최대는 모서리에서 얻으므로 네 조합이면 충분하다. [geometryDriveToPosition](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Solver/GeometryDrive.glsl:74)의 DirectionDrive∈[0,1]이므로 **GeometryDrive≤D_ij^max**이다. 접선 중력이 거의 0인 branch도 반환값 0이므로 이 크기 상한에 들어간다.

따라서 15의 미분 기반 bound와 달리, 여기에는 접선 중력 g_min이나 dynamic Concavity의 미분을 요구하지 않는다. 그 대신 모든 가능한 방향·적층 높이를 허용해 상한이 넓어질 수 있다. transform/meso/profile이 바뀌면 다시 계산하거나, 처음부터 그 변화 범위를 포함해야 한다.

##### 5.3 raw 식에서 outgoing 비율 상한

[rawFlux](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Solver/Transport.glsl:1)에서 saturation 차이의 양수 부분은 q_i/C_i 이하, weight와 Retention은 [0,1]이다. saturation rate S_i, geometry rate G_i에 대해

\[
rawF_{ij}\le {\Delta t\over C_i}(S_i+G_iD_{ij}^{max})q_i.
\]

실제 flux는 outgoing limiter로 더 줄어드므로

\[
\boxed{\alpha_i=\min\left(1,{\Delta t\over C_i}\sum_{j:i\to j}
(S_i+G_iD_{ij}^{max})\right)}
\]

는 충분한 상한이다. 고정 dt·정적 조건에서 State를 매 Step 읽지 않고 준비할 수 있다. rate 상수의 코드 정본은 [SurfaceSolverRates.h](C:/Users/yonai/Documents/MDSS_Engine/Source/SurfaceState/Types/SurfaceSolverRates.h:8)이다. 작은 texel footprint의 C_i와 큰 Geometry rate 때문에 α=1로 포화될 수 있다. 실제 장면에서 α가 얼마나 작은지는 **측정하거나 수치 실험하지 않았다**.

이 α는 무제약 다중 반응의 소모율을 포함하지 않는다. 현재 IceReactionLab에는 Heat 소비·Ice 융해가 있으므로 이 식을 그 전체 실험의 휴면 인증으로 사용하면 안 된다. FP raw 저장/산술의 과대 반올림도 별도 여유량이나 검증이 필요하다.

#### 6. [H/D] 이동률 구간별 질량으로 O(K) 기간 판정

α_B=max_i α_i 하나만 쓰면 작은 면적의 한 texel 때문에 전체 α_B=1이 될 수 있다. 이를 완화하는 간단한 방법은 **α의 구간별로 초기 질량을 모으는 것**이다.

각 구간 b에 상한 ᾱ_b를 두고, 그 구간에 포함된 위치의 초기 물량을 M_b=Σ_i∈b q_i⁰로 저장한다. 구간 membership은 정적 α로 정할 수 있다. 그러면

\[
E_{joint}(t)\le2\sum_{b=1}^K M_b\{1-(1-\bar\alpha_b)^H\},\qquad t\le H.
\]

K는 fine texel 수와 독립적으로 작게 고르는 제안이다. 실제 최적 K/분할은 미검증이다. 높은 α 구간에 물이 거의 없으면 그 구간의 큰 α가 다른 질량 전체에 적용되는 것을 피한다.

초기 질량을 그대로 두는 이유는 이것이 **초기 물량 중 각 위치에 최소한 남는 양**의 하한이기 때문이다. 이후 유입은 하한을 더 크게 만들 뿐이다. 실제 물이 이동하는 동안 질량 bin을 매 Step 재분류하는 알고리즘이 아니다. 새 입력은 아래 §8처럼 별도 처리해야 한다.

거듭제곱 없이 더 느슨하게 판정하려면 Bernoulli 부등식으로

\[
E_{joint}(t)\le2\sum_bM_b\min(1,H\bar\alpha_b)
\le2H\sum_bM_b\bar\alpha_b
\]

를 쓸 수 있다. W=Σ_b M_bᾱ_b>0일 때

\[
H_{safe}=\left\lfloor{\varepsilon\over2W}\right\rfloor
\]

Step까지는 조건부 충분 판정이 된다. H_safe=0이면 생략하지 않는다. W=0이면 초기 물은 빠져나가지 않고 새 유입도 없는 현재 가정 아래 상태가 정지한다. 비선형 식을 사용하는 경우 O(K) 평가/단조 검색으로 더 긴 기간을 찾을 수 있다.

**판정 비용은 줄었지만 효과는 아직 모른다.** 시작 시 O(N) 집계·K 저장·만료/입력 사건 관리가 필요하다. coarse State만 남기는 방법은 아니며 fine 상태와 A/B 일관성도 유지해야 한다. 매 Step GPU workgroup 목록을 실제로 건너뛰는 설계는 구현하지 않았다.

##### 해상도를 높이면 이 상한이 나빠질 수 있다

같은 물리적 표면을 균일하게 세분화하고 선형 간격을 ℓ이라 하자. 고정 Profile에서 C_i는 texel 면적에 비례하므로 O(ℓ²)이다. 이웃 수가 고정이면 saturation 항의 α 상한은 O(Δt/ℓ²)로 커진다. 서로 독립적인 전체 적층 범위로 D_max를 잡으면 이웃끼리 최대 높이 차이가 O(1)로 남을 수 있어 gravity 상한도 같은 악화를 보인다. 국소 기울기가 제한돼 D_max=O(ℓ)라고 추가 보장하면 gravity 항은 O(Δt/ℓ)로 완화된다.

이는 조건부 스케일 분석이다. 실제 UV 왜곡/경계 면적은 균일하지 않으며 측정값도 없다. α가 1이 된다는 것은 **이 판정이 무력해진다는 뜻**이지, outgoing limiter가 있는 원래 Solver가 음수가 되거나 불안정하다는 증명이 아니다.

정적 전체 범위 α만으로는 정밀한 Mesh에서 유용한 H를 얻기 어려울 수 있다. 돌파하려면 전체 cap 범위보다 좁은 **미래 높이 차이/State 범위**를 싸게 확보해야 한다. 현재 순간의 범위를 이후에도 유지된다고 가정해서는 안 된다. 제안한 범위가 앞으로 H Step의 갱신을 포함하는지 검증하는 enclosure가 필요하며, 그 원리도 기존 reachability 연구 R29와 겹친다.

#### 7. [D] 열린 경계에서 깨지는 최소 반례

block 내부 두 texel의 q₁=q₂=1, C=1, 초기 잔류 0이면 l=1, B=0이다. 하지만 q₂가 block 밖 q₃=0과 연결되고 k₂₃=β>0이면 첫 Step에 β가 유출된다. 닫힌 block이라고 잘못 가정해 모두 멈춘 실행과의 전체 물·잔류 L1 차이는 2β이다. **B=0인데 실제 오차는 0이 아니다.**

경계 수송을 계속 계산하고 내부만 멈춰도 첫 유출 뒤 q₁>q₂가 되어 다음 Step에 내부 수송이 생긴다. ‘외부 Incoming이 오면 깨운다’만으로는 부족하다. **Outgoing·직접 제거·경계 상태 변화**도 기존 판정을 무효화할 수 있다.

또 같은 q₁=q₂=1인 닫힌 두 texel이라도 중력 방향 flux γq₁가 있으면 첫 공동 변화량이 2γ이다. saturation 기반 B=0 상한은 이 경우 사용할 수 없다. 반면 §4의 α 상한은 중력을 포함하므로 모순이 없다.

실제 GPU의 64-thread workgroup은 transport graph에서 닫힌 영역이 아니다. 이웃 halo를 붙였다고 닫힌 집합이 되는 것도 아니다. §3–6을 일반 workgroup마다 독립 적용하고 오차를 합하는 방식은 아직 정당화되지 않는다.

##### 제한적으로 가능한 경계 확장

공통 하한 b가 두 실행에서 계속 유지되고, 잔류가 감소하지 않는다는 조건을 **별도로 보장**하자. 각 실행의 누적 외부 유입/유출을 I,O라 하면 내부 벡터 (q−b,a−a⁰)의 질량은 B+I−O이다. 누적 port별 outgoing을 비음수 장부로 붙이면 총질량은 B+I가 된다. 따라서 두 실행의 내부 상태와 누적 outgoing 장부의 공동 차이는

\[
E_{aug}\le2B+I^{ref}+I^{approx}
\]

로 제한할 수 있다. 이것은 외부 이웃의 상태/이후 반응까지 제한하는 식이 아니다. 외부로 나갔다 돌아오는 양은 gross 유입으로 다시 세야 한다. 순유량 0만으로 작은 budget이라 판단할 수 없다. α로 구한 초기물 하한은 boundary outgoing까지 α에 포함하면 유지할 수 있으나, reference의 실제 미래 Incoming을 공짜로 아는 것은 아니다.

일단 경계가 열리면 이 조건과 외부 영향 관리가 큰 비용/연구 과제가 된다. 따라서 현재의 O(K) 판정은 **닫힌 transport component 또는 명시적으로 경계 budget이 주어진 부분 문제**에 대한 방법이다.

#### 8. [H] 인증의 생성·만료·재사용 계약

| 사건 | 필요한 처리 |
|---|---|
| 알려진 초기 상태에서 최초 감사 | 질량/floor 또는 α-bin 질량 집계. 준비 비용 계상 |
| 조건 안의 State-Driven Geometry 변화 | §4–6의 α가 전체 가능한 형상을 포함하면 재감사 불필요. 다른 State의 결과까지 보호되는 것은 아님 |
| 직접 양수 입력 | 사건 전에 깨우거나 유입 budget에 추가. 닫힌 전체 모델에서 양쪽 총 입력≤I_max이면 공통 초기물 하한을 유지한 지름 bound는 2B_H+2I_max로 완화 가능 |
| 제거 입력·다른 소비/생성 반응·잔류 Decay | 현재 증명의 가정을 깨므로 그대로 사용 불가 |
| profile·mesh·transform·dt 변경 | α/형상 범위의 유효성 재판정. 기존 범위 밖이면 만료 |
| 예정 H 도달 | 추가 생략을 허용하지 않음. 정확 갱신으로 돌아와도 이미 생긴 오차는 없어지지 않음 |
| 다음 휴면 기간 시작 | 근사 상태를 새 ‘정답 초기 상태’로 보고 budget을 0으로 초기화하면 안 됨. 공통 reference enclosure/이전 오차 또는 신뢰 checkpoint 필요 |

상태를 매번 확인하는 대신 다음 감사 시점을 미리 정하는 기본 발상은 기존 self-triggered 방법과 겹친다. 이번 제안은 그런 스케줄링 원리를 새로 발명한 것이 아니다.

#### 9. [C] cap 위의 높이 생략은 이미 구현돼 있다

[StateFinalize.glsl::accumulationContributionChanged](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Solver/StateFinalize.glsl:1)는 Old/New State를 [0,Capacity]로 clamp한 뒤 비교한다. 둘 다 cap 위이면 값이 달라도 높이 재계산을 예약하지 않는다. [Pass 2](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Solver/SurfaceSolverPass2.comp:172)와 [Reaction](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Solver/SurfaceReaction.comp:175)가 이 함수를 사용한다.

[SurfaceAccumulationHeight.comp](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Geometry/SurfaceAccumulationHeight.comp:66)는 예약된 위치에서 실제 높이를 구하고 BuiltHeight와 비교해 Geometry 갱신을 예약한다. 반면 Solver의 다음 활성화는 Current>0 또는 NewState>0으로 유지된다. 따라서 **높이 불변 영역의 생략과 비영 물량의 수송 계산 생략은 이미 서로 다른 단계**다.

α 기반 하한으로 q_i⁰(1−α_i)^H≥C_i를 예측하면 cap 유지 기간을 구할 수는 있다. 그러나 이것은 기존 함수가 매 Step 수행하는 작은 비교를 예측으로 대체하는 수준일 수 있다. 다른 채널/이웃 형상 변화도 있어야 하고, 전체 갱신 이득을 새로 얻는다고 가정할 수 없다. **‘cap을 이용한 Geometry 생략’을 새 기여나 주력 알고리즘으로 제안하지 않는다.**

#### 10. 기존 연구와의 경계·현실적인 선택

[P] 불변 영역·convex limiting은 기존 수치해석 분야다. [Guermond, Popov & Tomas의 연구](https://arxiv.org/abs/1807.02563)는 이 개념의 선례이며 이번 식과 동일한 알고리즘이라고 주장하지 않는다. 현재 상태에서 다음 계산 시점을 예측하는 [Anta & Tabuada의 self-triggered control](https://arxiv.org/abs/0806.0709), 낮은 복잡도의 상태 범위로 전체 갱신을 줄이는 [Silvestre 등의 set-valued observer 연구](https://doi.org/10.1016/j.ins.2017.10.029)도 관련 선례다. 실제 열람 수준은 [[90_Research-Archive/Research-Notes-2026-10/04_Literature/01_Related-Work#source-reference-07-related-work|R39–R41]]에 기록했다.

이번 simplex 지름·하한 곱·rate bin 합산·만료 예약은 모두 표준 도구의 적용이다. 독립적인 SIGGRAPH Contribution으로 보기 어렵다. 그래도 다음 두 선택을 더 정확히 비교할 수 있게 됐다.

| 선택 | 장점 | 중단/방향 수정 기준 |
|---|---|---|
| 닫힌 saturation component의 near-equilibrium 생략 | 2B가 작으면 추가 fine 판정 없이 장기 제한 가능 | 실제 장면 대부분 l=0/중력 ON/다중 반응이면 적용 범위가 너무 좁음 |
| α-bin 기반 짧은 기간 생략 | 중력·동적 형상·limiter를 포함하면서 O(K) 기간 판정 가능 | H_safe가 대부분 0–1이거나 준비/목록 관리 비용이 이득을 소멸 |
| 임의 열린 block의 생략 | 실용성이 높을 수 있는 최종 대상 | boundary/다른 State 영향 관리가 fine 계산만큼 비싸거나 기존 적응법으로 충분 |

**현재 권고:** 새 코드를 먼저 만들지 않는다. 다음 비실행 분석은 (i) 알려진 Profile/geometry 계약으로 α가 어느 조건에서 1로 퇴화하는지, (ii) 열린 경계에서 원래 충분조건을 보존하는 최소 외부 정보가 무엇인지, (iii) 이 범위가 실제 제작 목표에서 중요한지를 좁힌다. 이미 가진 PERF 자료가 이 새로운 방법의 품질–비용을 입증하는 것처럼 재사용하지 않는다.

방향3은 **저비용 판정의 조건부 구성까지 진전**했지만 아직 주력 Contribution 확정 단계가 아니다. 방향2는 여전히 제작 목적과 가장 가깝고, 위 bound는 제한적 보호 평가의 도구로 쓸 가능성이 있다. 방향1은 방향2의 목표/평가로 유지한다. 이번 분석으로 현 구현의 시스템 통합 판정이나 신규성 평가를 상향하지 않는다.

---

## source-analysis-17-open-boundary-contracts-and-composition

원본: `30_Deep-Analysis/17_Open-Boundary-Contracts-and-Composition.md`

### 열린 경계의 정보 계약과 오차 합성 — 조건부 확장과 실패 지점

작성: **2026-10-28 / Asia/Seoul**. [[90_Research-Archive/Research-Notes-2026-10/03_Solver/03_Error-Control-and-Sparse#source-analysis-16-low-cost-certificates-and-sleep-horizons|16]]의 다음 질문인 열린 영역의 최소 외부 정보를 검토했다. **코드/문헌 읽기·기호 유도·문서 작성만 수행했다. 앱·GPU·빌드·수치 실험·학습 미실행, 제품 코드 변경 없음.** 코드 기준은 HEAD `880af1ebcffe601f0bac27413cad94055ce92812`와 기존 작업 트리다.

아래 [D]는 제한된 모델의 이상 산술 분석, [C]는 코드 근거, [H]는 미구현 제안, [P]는 확인한 선행연구다. 새로운 Contribution이나 실제 실행 성능을 입증하지 않는다.

#### 1. 이번 결론

열린 경계를 포함하는 오차 식은 만들 수 있다. 그러나 **식을 여러 block에 합칠 수 있다는 것과, 그 결과가 실제로 쓸 만한 정확도/비용을 갖는다는 것은 다르다.**

이번에 구체화한 결과는 다음이다.

1. 같은 시점·위치로 들어온 물은 모두 새 오차로 셀 필요가 없다. 두 실행에 공통으로 남는 물/잔류를 하한으로 빼면, 유입 이후의 이동 가능량과 서로 다른 유입만 계상할 수 있다.
2. 이 하한은 이동률 구간별 recurrence로 유지할 수 있고, 제한 모델에서는 경계의 오차 항을 상쇄하여 전체 영역의 오차 상한을 만들 수 있다.
3. 하지만 그 단순 합성은 **16의 전체 영역 α 상한보다 더 느슨할 수밖에 없는 구조**가 있다. 이 자체를 주력 방법으로 구현할 근거는 부족하다.
4. 실제 코드의 경계 영향은 물량만으로 결정되지 않는다. **source의 내부 raw outgoing 합**과 **공유 Geometry의 stencil 영향**도 필요하다.
5. 따라서 다음 후보는 무조건 많은 경계 통계를 저장하는 방법보다, **내부가 경계에 영향을 주지 않는 조건 또는 제한된 영향만 주는 조건을 판정하는 계산 계약**으로 좁혀야 한다.

#### 2. [D] 유입은 위치·시간·방향별로 구분해야 한다

여기서 port는 block을 가로지르는 **방향 있는 실제 edge**다. u_e^n은 Step n에서 외부→내부로 들어온 실제 물량, o_e^n은 내부→외부로 나간 실제 물량이다. source에서 뺀 값과 target Incoming에 더한 값은 같아야 한다. q는 물, a는 움직이거나 줄어들지 않는 잔류, f_i∈[0,1]은 recipient i의 고정 1:1 Incoming 전환 비율이다.

반례는 간단하다.

- 유입 총량이 같아도 f가 다른 두 위치로 들어오면 즉시 생성되는 잔류량이 다르다.
- f가 같아도 들어온 위치가 다르면 잔류의 공간 분포가 다르다.
- 위치·총량이 같아도 관측 직전에 들어온 물과 여러 Step 전에 들어온 물은 내부 이동에 사용할 수 있었던 시간이 다르다.

따라서 ΣIncoming−ΣOutgoing 또는 관측 끝의 누적 총량만으로 일반적인 후속 결과를 보존할 수 없다. 다만 **상한을 계산하는 용도**라면 이후처럼 일부 정보를 보수적으로 묶을 수 있다. 정확한 상태 복원과 오차 상한용 요약은 구분한다.

#### 3. [D] 공통 유입을 이용한 열린 block의 공동 하한

##### 3.1 모델과 하한

reference와 근사는 같은 q⁰,a⁰에서 시작한다. 내부/경계 outgoing 전체가 O_i^n≤α_i q_i^n을 만족하고, r_i=1−α_i∈[0,1]라 하자. 중력·동적 형상·limiter는 이 상한 안에서 허용한다. 단일 Incoming 전환 이외의 소비/생성·Decay·직접 제거는 없다.

두 실행의 경계 유입 u_e^n, û_e^n에서 c_e^n=min(u_e^n,û_e^n)를 공통 부분으로 둔다. recipient별 c_i^n=Σ_{e→i}c_e^n이다. 그러면 두 실행 모두

\[
q_i^H\ge r_i^Hq_i^0+
\sum_{n=0}^{H-1}r_i^{H-1-n}(1-f_i)c_i^n=:b_i^q(H),
\]

\[
a_i^H\ge a_i^0+\sum_{n=0}^{H-1}f_i c_i^n=:b_i^a(H).
\]

이유는 q_i^(n+1)≥r_iq_i^n+(1−f_i)c_i^n이며, 새 유입은 같은 Step에 다시 outgoing으로 사용되지 않기 때문이다. 내부 Incoming은 하한에서 버려도 비음수다. a의 내부 전환도 하한을 늘릴 뿐이다.

##### 3.2 출력 이력까지 포함한 상한

내부 최종 상태뿐 아니라 **port×Step별 outgoing 이력 전체**를 벡터에 붙인다. outgoing을 최종 누적값 하나로 먼저 합치지 않는다. 이렇게 해야 앞뒤 시점의 상반된 오차가 취소되어 외부의 시간 의존 반응을 놓치는 일을 피한다.

각 실행의 (q−b^q, a−b^a, outgoing history)는 비음수다. 수지에서 두 벡터 총합을 더하면

\[
\boxed{E_B(H)+D_B^{out}(H)\le2B_B(H;c)+D_B^{in}(H),}
\]

\[
E_B=\|q^H-\widehat q^H\|_1+\|a^H-\widehat a^H\|_1,
\quad D_B^{in}=\sum_{n,e\to B}|u_e^n-\widehat u_e^n|,
\quad D_B^{out}=\sum_{n,e:B\to}|o_e^n-\widehat o_e^n|,
\]

\[
B_B(H;c)=\sum_i q_i^0(1-r_i^H)+
\sum_{n,i}(1-f_i)c_i^n(1-r_i^{H-1-n}).
\]

첫 항은 초기 물 중 그 위치를 떠날 가능성이 있는 양, 둘째 항은 **공통 유입 중 즉시 잔류로 바뀌지 않은 부분이 이후에 떠날 가능성이 있는 양**이다. 들어온 직후에는 둘째 항이 0이다. 직접 생긴 공통 잔류 f_i c_i^n은 움직이지 않으므로 차이 예산에 다시 넣지 않는다.

이 식은 앞선 2B+I_ref+I_approx보다 공통 유입을 더 세밀하게 사용한다. 하지만 c를 구하려면 보통 reference의 미래 유입이 필요하므로, 그대로 구현 가능한 공짜 입력이라고 볼 수 없다.

##### 3.3 검증된 구간만 아는 경우

c를 정확히 모르고 c_lower≤u,û인 공통 하한만 알면, 입력 불일치 대신 두 입력에서 c_lower를 뺀 **남은 총량**의 상한을 사용해야 한다. 낮은 c_lower를 위 B에 넣으면서 실제 |u−û|만 더하면 공통 유입의 일부를 누락한다. 이 둘을 혼동하지 않는다.

또 근사와 reference에 같은 경계 유입을 강제로 재생한 비교는 **주어진 입력에서의 block 응답 비교**다. 완전히 결합된 MDSSP에서도 그 유입이 같다는 증거가 아니다.

#### 4. [D/H] 이동률 구간으로 하한의 합을 유지

16처럼 α_i≤ᾱ_b인 구간 b를 잡고 r_b=1−ᾱ_b로 둔다. 각 구간에서 초기 Q_b^0=Σ_i∈b q_i⁰, B_B^0=0으로 시작한다. 공통 유입의 mobile 부분 U_b^n=Σ_i∈b(1−f_i)c_i^n에 대해

\[
Q_b^{n+1}=r_bQ_b^n+U_b^n,\qquad
B_B^{n+1}=B_B^n+\sum_b\bar\alpha_bQ_b^n.
\]

이 recurrence는 §3의 B를 구간 상한으로 계산한다. 원래 각 위치에 공통으로 남는 양의 하한은 존재하지만, 그 **합**만 요약해도 지름 상한 계산에는 충분하다. 유입 이력 전체나 모든 내부 texel을 매 Step 읽을 필요는 없다.

대신 경계 유입을 recipient의 구간과 f에 따라 집계하는 비용이 든다. 이미 계산된 Incoming을 활용할 수 있어도 집계/동기화 비용이 0은 아니다. K개 요약의 갱신은 O(K), 경계 집계는 실제 경계 크기에 의존한다. fine 상태와 경계에 필요한 상태는 여전히 보존한다.

#### 5. [D] block을 합칠 수 있지만, 이 단순 합성은 주력 해법이 되기 어렵다

##### 5.1 제한 모델의 합성

전체 닫힌 graph를 겹치지 않는 block으로 나누고 같은 fine Step을 사용한다고 하자. 각 실행 안에서 cut edge의 outgoing이 바로 이웃의 incoming이며, 같은 방향/시점/단위를 유지해야 한다. 그러면

\[
\sum_BD_B^{out}=\sum_BD_B^{in}.
\]

§3의 부등식을 합치면 경계 오차 항이 상쇄되어

\[
\boxed{E_{global}(H)\le2\sum_B B_B(H;c).}
\]

각 B_B는 c에 대해 단조 증가한다. c≤û이므로 **근사 실행에서 실제 관측한 유입 û**로 recurrence를 돌린 B̂_B를 쓰면 B_B(c)≤B̂_B이다. 따라서 reference를 실제 실행하지 않고도

\[
E_{global}(H)\le2\sum_B\widehat B_B(H)
\]

라는 더 느슨한 조건부 상한을 얻는다. 같은 초기 상태·단일 1:1 전환·유효 α·비음수/수지 계약이 모두 필요하다. 외부 입력이 다르면 해당 불일치 항은 상쇄되지 않는다. local time stepping을 허용하려면 공통 port-time 장부로 다시 정식화해야 한다.

Geometry가 달라져도 α가 그 모든 변화를 포함하면 위 두 State의 수지 증명은 유지된다. 이 때문에 아래 §7의 geometry-only 영향과 모순되지 않는다. 다만 다른 State의 결과를 자동으로 보장하지 않는다.

##### 5.2 더 중요한 반론: 전체 영역 상한보다 느슨하다

같은 α_i, 같은 초기 시점, 같은 기간을 사용한 16의 전체 영역 상한은 **초기 물 cohort**만 센다. 위 block별 B̂는 그 초기 항들에 더해, 내부 경계를 넘을 때마다 생긴 경계 유입 cohort의 이동 가능량을 비음수 항으로 더한다. 따라서 닫힌 전체 영역의 물·잔류 오차만 목표라면

\[
\boxed{\sum_B\widehat B_B(H)\ge
\sum_iq_i^0(1-r_i^H)}
\]

이다(같은 bin 상한을 사용하는 경우도 동일). **단순히 쪼개 합성한 것만으로 전역 판정이 좋아지지 않는다.**

또 이 bound는 실제로 두 실행이 완전히 같은 영역의 정상적인 이동도 예산에 넣는다. 정확히 계산하는 활성 영역까지 같은 방식으로 계상하면, 생략한 영역이 작아도 상한이 커질 수 있다. 순환 이동은 경계 cohort를 반복 생성해 특히 불리하다.

따라서 ‘α-bin을 모든 block에 넣고 경계 입력을 더하면 실용적인 열린 영역 알고리즘이 완성된다’는 제안은 **현재 주력 구현안에서 보류**한다. 합성 가능성은 확보했지만 품질–비용 우위를 설명하지 못한다. 같은 시점/α의 비교에서 불리하다는 결론이며, 모든 block 모델이 불가능하다는 정리는 아니다.

필요한 개선은 **양쪽에서 같은 이동을 계속 비용으로 세지 않고, 근사 때문에 새로 달라진 부분을 구분하는 것**이다. 15의 local defect/오차 전달이나 기존 goal-oriented·compositional 분석과 연결해야 한다. 블록별 시계를 임의로 초기화해 이 문제를 숨길 수는 없다.

#### 6. [C/D] 경계 source의 값만 같아도 경계 Flux는 달라진다

[SurfaceSolverPass1.comp](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Solver/SurfaceSolverPass1.comp:95)는 source의 모든 방향 raw flux를 합친 뒤, available amount M에 대해 λ=min(1,M/RawOutgoing)을 계산한다. [Pass 2](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Solver/SurfaceSolverPass2.comp:85)는 이 source 배율을 경계 incoming에도 곱한다.

source의 경계 raw flux 합을 S, 내부 raw flux 합을 T, 특정 경계 edge의 raw flux를 r_e라 하면

\[
F_e=r_e\min(1,M/(S+T)).
\]

M=1, r_e=S=1인 source에서 T=1이면 F_e=1/2, T=3이면 F_e=1/4이다. **경계 source 양과 경계 raw 조건은 같은데 내부 수송 요구량 때문에 경계 출력이 다르다.** 이는 정규화된 장부 예이며 실제 장면 실행 결과가 아니다. 내부 target saturation과 edge 가중치가 T를 바꿀 수 있고, source 전체 배율로 경계에도 영향이 전달된다.

##### 내부 전체 대신 필요한 요약을 제한할 수 있는 경우

M,S,r_e가 같고 내부 T의 검증된 범위 [L,U]만 알면, S+L>0일 때

\[
r_e\min(1,M/(S+U))\le F_e\le r_e\min(1,M/(S+L)).
\]

M≥S+U이면 limiter가 확실히 비활성이므로 **F_e=r_e이고 내부 T의 정확한 값이 불필요**하다. S+L>M인 활성 branch에서는 구간 폭이

\[
\Delta F_e\le {r_eM(U-L)\over(S+L)(S+U)}
\]

이다. branch를 가로지르면 위 endpoint 식으로 처리한다. 분모가 0인 경우는 해당 raw edge도 0인 별도 경우로 다룬다. M,S,r_e까지 불확실하면 그 상관관계를 포함한 추가 enclosure가 필요하다.

이는 **내부가 경계에 영향을 주는지 판정할 수 있는 구체적 조건**이다. 다만 U를 구하려고 모든 내부 raw flux를 매 Step 계산하면 절감 목적을 놓친다. 기존 outgoing limiter의 분석이며 그 자체의 신규성을 주장하지 않는다.

#### 7. [C/D] 물질이 경계를 넘지 않아도 Geometry로 영향이 전달된다

[StateMath.glsl::effectiveLocalNormal](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Solver/StateMath.glsl:111)은 이웃 높이에서 Normal을 복원할 때 해당 State 채널 지원 여부가 아니라 texel 유효성과 macro normal 등을 검사한다. 반면 [Transport.glsl](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Solver/Transport.glsl:32)은 target 채널 지원 여부로 물질 수송을 제한한다.

따라서 다음은 코드가 허용하는 **구조적 반례**다(실제 asset 구성/실행은 하지 않음).

- 안쪽 Profile은 Ice 같은 적층 State를 지원하고 WaterFilm은 지원하지 않는다.
- 바깥 Profile은 WaterFilm을 지원하며, 안쪽과의 geometry 이웃 연결은 유효하다.
- 안쪽 적층이 달라지면 바깥 texel의 복원 Normal이 달라질 수 있다.
- Normal 기반 weight 또는 gravity drive가 활성이고 민감한 배치라면 바깥 texel들 사이의 물 이동이 달라질 수 있다.

이때 cut을 넘는 WaterFilm flux가 0이어도 외부 WaterFilm 결과가 달라질 수 있다. **동일한 물질 port 이력만으로 전체 State-Driven Geometry Coupling의 경계 응답을 규정할 수 없다.** 영향이 항상 생긴다는 뜻은 아니며, 실제 크기/중요성은 미검증이다.

필요한 geometry 정보는 내부 전체 높이일 필요는 없다. 외부에서 읽는 연산 stencil에 닿는 높이/위치/Normal/Concavity의 값 또는 검증된 범위가 후보이다. 15의 정적 LS 민감도는 이 범위를 연결하는 도구가 될 수 있다.

단, 현재 Geometry scheduler의 2-hop halo가 임의 block의 **전체 한 Step 경계 계약 반경**도 정확히 2라는 뜻은 아니다. target Concavity, 이웃 source의 전체 outgoing 배율, reaction/geometry 갱신 순서를 포함한 의존성 닫힘을 별도로 확인해야 한다. 여기서는 최소 반경을 확정하지 않았다.

#### 8. [H] 다음 방법 후보 — 연산 의존성을 반영한 경계 계약

이것은 구현이 아니라 다음 분석의 명세다. 경계 source/관측마다 다음 정보가 충분한지 확인한다.

| 정보 | 필요한 이유 | 생략/축약할 수 있는 조건 |
|---|---|---|
| 방향·시점·채널별 실제 물질 이동 | Incoming 반응과 외부의 시간 의존 결과 | 고정 f·고정 관측에서 필요한 cohort만 상한으로 합칠 수 있음 |
| source available amount와 내부 raw 합의 범위 | limiter가 모든 outgoing을 함께 조절 | M≥S+U로 비활성 인증, 또는 output 구간 폭이 허용량 이하 |
| 외부 stencil에 닿는 형상 정보/범위 | 질량 이동 없는 Geometry 매개 영향 | 관련 height 기여 불변 또는 geometry→관측 영향이 검증된 허용량 이하 |
| 상태/형상/배율의 계산 시점 | 다른 Step의 cache를 섞으면 공통 flux 계약이 깨짐 | 동일 갱신 기준과 오차 장부를 명시적으로 유지 |

가능한 절차는 (i) 경계 응답에 영향을 주는 연산을 추출, (ii) branch별로 필요한 요약/범위를 결정, (iii) 충분조건을 만족하는 동안 내부 갱신 생략, (iv) 조건이 만료되면 필요한 정보만 갱신하는 것이다. 이것이 일반적인 dependency/interval 분석 이상의 새 방법이 되는지는 아직 모른다.

**중단 기준:** 필요한 요약을 갱신하는 데 내부 fine 계산과 같은 비용이 듦 / limiter·geometry 범위가 대부분 불확실 / 경계 band가 대부분의 영역을 차지함 / 기존 microgrid·port reduction·오차 제어 방법에 같은 정보를 주면 동일한 결과.

규칙적인 넓은 2D patch는 내부 면적에 비해 경계가 작을 수 있다. 하지만 현재 64-thread 그룹과 UV seam/비정규 graph가 그런 patch인지 확인한 것은 아니다. thread group 크기를 근거로 경계 비용이 작다고 주장하지 않는다.

#### 9. 선행연구와 후보 판정

[P] 경계 flux 일치로 보존을 유지하는 방법은 이미 AMR의 기본 구조다. [AMReX 공식 FluxRegister 설명](https://amrex-codes.github.io/amrex/docs_html/AmrCore.html?highlight=fluxregister)을 확인했다. 따라서 공유 장부 자체는 기여가 아니다. 보존을 맞췄다는 것과 근사 결과가 가깝다는 것은 구분한다.

[P] [Smetana의 port/interface reduction](https://arxiv.org/abs/1808.02946)은 geometry 변화와 재사용도 다룬다. ‘경계 정보만 작게 남긴다’ 또는 ‘geometry-aware port’라는 이름만으로 신규성을 주장할 수 없다. [Rungger & Zamani의 compositional abstraction](https://arxiv.org/abs/1507.05014)도 subsystem의 오차 관계를 연결하는 선례다. 상세 확인 수준은 [[90_Research-Archive/Research-Notes-2026-10/04_Literature/01_Related-Work#source-reference-07-related-work|R42–R44]]를 따른다.

현재 판정은 다음과 같다.

- **단순 α-bin block 합성:** 조건부로 올바른 식을 얻었지만, 동일 조건의 전체 영역 bound보다 느슨하므로 주력 구현안 보류.
- **Limiter·Geometry 의존성을 반영한 경계 계약:** 현재 코드에서 출발하는 더 구체적인 알고리즘 탐색 후보. 충분조건 일부를 유도했으나 저비용 갱신과 실제 필요성은 미입증.
- **방향2의 보호 편집:** geometry-only 영향은 ‘물질 흐름을 바꾸지 않으면 다른 현상이 보호된다’는 가정에 대한 반례로 활용 가능. 독립적인 편집 해법을 확보한 것은 아님.

이번 연구는 유망도를 일괄 상향한 결과가 아니다. **한 단순 확장안의 약점을 확인하고, 실제로 보존해야 할 경계 정보를 더 정확히 특정한 결과**다. 새로운 SIGGRAPH Contribution은 여전히 미확보이며, 현재 구현의 시스템 통합 판정을 바꾸지 않는다.

---
