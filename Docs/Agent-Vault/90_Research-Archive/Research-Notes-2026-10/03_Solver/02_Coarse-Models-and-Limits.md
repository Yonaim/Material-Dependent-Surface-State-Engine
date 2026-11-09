# Coarse 반응 모델과 표현 한계

coarse 평균의 정보 손실, 내부 유입 반응, 닫힌 최소 모델, 방법 초안과 기각 조건을 묶었다. 수식 반례와 범용 GPU 해법은 별개다.

## 먼저 볼 결론

- 단순 평균·히스토그램만으로는 내부 이동에 따른 유입 반응과 후속 출력을 일반적으로 정할 수 없다. 최소 반례는 이 정보 손실을 보여주며 새 방법의 성공을 뜻하지 않는다.
- 닫힌 두 texel에는 제한된 정확한 잠재량 표현이 가능하다. 열린 port·다중 반응·동적 형상으로의 저비용 확장은 미해결이다.
- 표준 closure, 양수 quadrature, microgrid, fine sparse가 강한 대조다. 정보 폐기 후 refinement만으로 원래 fine 상태가 복원되지는 않는다.

## 이 문서의 원본 절

- [[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#source-analysis-06-coarse-reactive-transport|30_Deep-Analysis/06_Coarse-Reactive-Transport.md]]
- [[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#source-analysis-11-contribution-proof-obligations|30_Deep-Analysis/11_Contribution-Proof-Obligations.md]]
- [[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#source-analysis-12-exact-submodels-and-limits|30_Deep-Analysis/12_Exact-Submodels-and-Limits.md]]
- [[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#source-analysis-13-method-blueprints-and-decision|30_Deep-Analysis/13_Method-Blueprints-and-Decision.md]]

원본 25개 파일의 수정 전 사본과 SHA-256 목록은 `Original-25-Notes.zip`에 있다. 아래 절은 원문의 수식과 당시 판단을 보존하며, 링크와 제목 수준만 새 위치에 맞췄다.

---

## source-analysis-06-coarse-reactive-transport

원본: `30_Deep-Analysis/06_Coarse-Reactive-Transport.md`

### 방향 3 — 혼합·경로 정보를 유지하는 coarse 표현

마지막 갱신: 2026-10-27. [[04_Development/08_Research-Directions-123|최초 통합 검토]]에서 분리한 후속 연구 정본이다. 이 파일의 내용은 제안·수식 분석이며 새 구현/실험 결과가 아니다. 로컬 앱·GPU·빌드 실행 중지를 유지한다.

공통 근거는 [[90_Research-Archive/Research-Notes-2026-10/03_Solver/01_Forward-and-Reaction-Semantics#source-solver-10-forward-model-and-controls|forward 모델]], 문헌 출처는 [[90_Research-Archive/Research-Notes-2026-10/04_Literature/01_Related-Work#source-reference-07-related-work|문헌 확인 수준]], 이후 연구는 [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-reference-09-analytical-findings|분석 노트]]를 따른다.


#### 1. 현재 코드에서 직접 유도되는 최소 반례

[C] `IceReactionLab.SRProfile`의 `water_cooling`은 Heat에 비례하는 rate coefficient 0.5와 Waterfilm의 linear modifier(scale=1)를 사용한다. `SurfaceReaction.comp::reactionRequest`를 대입하면 local request는 \(0.5 H_i W_i/a_i\)다. \(a_i\)는 기준 면적 대비 texel 면적이고, 반응이 사용하는 정규화는 **State/면적**이다. 표시용 State/Capacity와 혼동하지 않는다.

[D] 동일 면적 셀에서 \(h_i=H_i/a_i, w_i=W_i/a_i\)라 하면 단위 면적당 초기 냉각률은 \(0.5h_iw_i\)이고

\[
\overline r=0.5(\overline h\,\overline w+\operatorname{Cov}(h,w)).
\]

두 미세 셀을 coarse 한 셀로 합친다고 하자.

| 미세 분포 | 셀 1의 (h,w) | 셀 2의 (h,w) | 평균 (h,w) | 평균 초기 냉각률 |
|---|---|---|---|---|
| 분리 | (2,0) | (0,2) | (1,1) | 0 |
| 혼합 | (1,1) | (1,1) | (1,1) | 0.5 |

동일 Profile·평면에서 Transport/Decay/Geometry 변화를 끄고 이 local reaction을 분리하면, **같은 coarse 총량만으로 두 초기 반응률을 동시에 재현할 수 없다.** 이 계산은 실제 구현 식의 대입이며 새로운 GPU 실험 결과가 아니다. 시간 적분·공급량 제한이 활성화된 장시간 결과가 위 단일률과 같다고 주장하지 않는다. 분리된 재료가 실제로 접촉/혼합되기 전 반응하는 coarse 오차의 단서다.

이것은 1·2보다 문제 존재의 근거가 구체적이다. 그러나 평균의 곱과 곱의 평균 차이, covariance closure 자체는 오래된 문제다. [R9]는 segregated initial conditions의 coarse diffusion–reaction에서 단순 rate 보정 이상이 필요함을 다룬다. **이 반례를 발견한 것만으로 신규성은 없다.**

[D] 후속 분석은 두 가지를 더 분리했다. (1) Heat 평균·분산과 모든 State 이차 모멘트가 같은 여섯 셀의 두 분포에서도 현재 ramp 융해의 초기 평균 request는 1/16과 1/24로 다르다. (2) Capacity clamp 때문에 평균 저장량에서 만든 높이와 fine 높이의 평균도 다르다. 각각 threshold 분포와 capped occupancy 정보의 필요성을 보이는 조건부 반례이며, 새 정리나 GPU 결과가 아니다. [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#5. 방향 3A: 모든 이차 State 모멘트도 ramp 반응에는 부족한 반례|후속 분석 §5–6]]에 식과 제한을 기록했다.

#### 2. ‘해상도 독립’ 대신 분명한 목표

고해상도 MDSSP의 discrete forward \(\Phi_h\)를 기준 모델로, q 총량을 합치는 restriction R을 둔다. 물리적 정답이나 아직 유도하지 않은 연속 PDE를 기준으로 가정하지 않는다. coarse 예측기 \(\widehat\Phi_H\)의 목표는 \(R\Phi_h^m(q)\)를 관측량/보존식/비용 범위에서 근사하는 것이다.

[D] 어떤 x,y에서 Rx=Ry이지만 \(R\Phi_h^m(x)\ne R\Phi_h^m(y)\)이면, 평균만을 입력으로 받는 결정론적 coarse 모델은 둘을 동시에 정확히 맞힐 수 없다. norm에서 두 fine 결과의 거리가 d이면 같은 coarse 출력의 두 오차 중 적어도 하나는 d/2 이상이다(삼각부등식). 이는 일반적인 정보 손실의 성질이며 새 정리로 주장하지 않는다. Rx에는 coarse 모델에 허용된 모든 정적 형상/재질 정보도 포함해야 한다.

따라서 연구할 것은 ‘계수 하나를 해상도에 맞게 보정’보다 **어떤 추가 상태 z를 저장하고 어떻게 진화시키면 중요한 fine 결과를 저비용으로 근사하는가**다.

#### 3. 가능한 표현과 명확한 한계

| 후보 표현 | 기대 역할 | 해결되지 않는 문제 |
|---|---|---|
| 필요한 반응 쌍의 covariance/mixing fraction | Heat–Waterfilm처럼 곱 형태의 초기 평균 반응률 보정 | covariance의 시간 진화도 closure 필요. threshold/ramp·다중 소비 경쟁은 같은 이차 모멘트만으로 일반적으로 닫히지 않음 |
| 소수의 양수 quadrature 상태와 면적 가중치 | 혼합/분리·임계값 일부를 평균보다 자세히 보존 | 같은 통계라도 공간 연결이 다르면 flux를 구별하지 못함. node 수를 늘리면 비용 증가 |
| coarse 경계의 소수 통과 특성/국소 그래프 | 좁은 홈·장벽의 통과와 State 분포 상관 반영 | 고정 지형의 사전 conductance만으로 State가 바꾸는 형상을 모두 처리할 수 없음 |
| 필요한 영역만 microcell 사용 | compact closure가 실패하는 곳을 정확하게 처리 | 기존 AMR/microgrid와 다를 수 있음. 결국 대부분을 세분화하면 compact 표현 이점 사라짐 |

이 표는 구현 제안이며 기존 MDSSP 기능이 아니다. 모든 State 쌍의 covariance와 모든 세부 연결을 저장하는 것은 피한다. 반응 graph가 실제로 참조하는 쌍만 보존하는 범위도 지표로 검증해야 한다. 기존 chemistry reduced closure와 hydrology subgrid 방식의 단순 적용이면 기술 개선으로 한정한다.

[P] [R10–R11]에는 subgrid 지형에서 수위에 따른 storage/통과 특성 또는 cell/cell-face 평균화를 쓰는 선례가 있다. 따라서 ‘높이 평균 대신 경계 정보를 저장’도 독창성으로 가정하지 않는다. 동적 다중 State 축적에서 어떤 새 갱신이 필요하고 비용 대비 무엇을 유지하는지가 후보다.

[P] 추가 확인한 DQMOM-IEM 원문 서론 [R12]과 QBMM 출판사 요약 [R13]은 다중 scalar의 비선형 반응을 가중 대표 상태로 다루는 강한 선례다. 따라서 quadrature를 새 아이디어로 취급하지 않는다. 반응 request별 feature와 capped occupancy를 보존하는 표현은 탐색 후보지만, 그 feature의 시간 진화는 다시 closure를 요구한다. fine oracle 없이 갱신하는 방법과 표준 quadrature/microgrid 대비 이점은 아직 없다.

#### 4. 단계와 Baseline

**3A: 반응부터 분리.** 알려진 분리/혼합, Heat threshold 근처의 분포, 공급량 경쟁을 고정 평면에서 비교한다. 기준은 fine operator를 면적 합한 결과다. scalar coarse/단위·dt 보정, best-fit scalar rate, 표준 covariance 보정, 2/4-node positive quadrature, 실제 microgrid를 대조한다. fine에서 매 Step 읽은 covariance를 쓰는 oracle은 표현 상한 진단이며 실용 방법으로 계산하지 않는다.

**3B: 수송만.** Reaction OFF에서 같은 coarse 정보/높이 통계를 갖지만 연결이 다른 홈/장벽을 구성한다. subgrid conductance, cell-face 자료, microgrid/AMR를 대조한다. 정적 지형 사전 계산만으로 충분하면 새 dynamic closure 주장을 하지 않는다.

**3C: 결합.** 3A/3B의 어려움이 확인된 경우에만 State 축적이 통과 특성과 혼합을 바꾸는 문제로 확장한다. Reaction ON/OFF × Geometry 동적/고정의 factorial 대조로 결합 항의 필요성을 확인한다. 세 요소가 있다는 이유로 각각의 baseline을 생략하지 않는다.

초기 범위는 한 평면/한 Profile·Ice/Waterfilm/Heat(+잔류)와 단일 coarsening factor다. 실제 문제 확인 뒤 다른 입력 패턴·기울기·시간 간격/해상도와 한 미사용 형상으로 넓힌다. Bunny seam 누락이나 고해상도 미수렴을 coarse 모델의 오차로 잘못 흡수하지 않는다. UV 재분할·물체 scale·normal 적분 오차는 별도 요인으로 분리한다.

**평가:** 제한한 State량·분포의 trajectory error, 누적 반응 extent, 경계 통과량/도달 시점, 목표 면적·모양, 비음수·알려진 물질 불변량, 메모리·전처리/학습/추론 총 비용. 채널 숫자를 모두 더해 물질 보존이라고 하지 않는다. fine 모델이 물리적 정답이라는 주장도 하지 않는다.

**중단 기준:** 단위/거리 보정 또는 표준 closure로 충분 / 효과가 fine 해상도를 올리면 사라지는 기준 오류 / mixing 정보가 실제 외관·진행에 중요하지 않음 / compact 모델이 microgrid보다 손해 / 매 Step fine oracle이 필요 / 계수·mesh별 대규모 재학습으로 재사용 이점 상실.

#### 5. `inflowFraction`은 별도로 다룰 문제

현재 유입 비율 f는 실제 Incoming에 적용된다. 완전한 일방향 transfer를 m번 겪는 단순 예에서 남는 양은 \((1-f)^m\)이다. 이 예를 일반 MDSSP의 전체 흡수율 공식으로 쓰지 않는다. 부분 flux·되돌아오는 흐름·시간 간격·지형에 따라 관계가 다르다.

‘유입 1회당 비율’이 의도한 의미라면 그 자체가 버그는 아니다. 단위 거리/체류 시간당 효과를 의도했다면 rate/distance 기반 재정의가 필요할 수 있다. 이 **규칙의 단위 문제**와 위 **fine 혼합 정보의 손실**은 서로 다르다. 표준적인 지수 감쇠/면적·거리 보정으로 전자가 해결되었다고 후자의 closure 문제까지 해결했다고 하지 않는다.

#### 6. 추가 추론: histogram과 경계 순유량만으로도 부족하다

[[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#2. 가장 직접적인 새 단서: 순유량이 사라져도 유입 반응은 남는다|심화 추론 §2]]에서 네 texel chain의 (1,1,0,0)/(1,0,1,0)을 비교했다. 같은 histogram·고정 graph·닫힌 coarse 영역이지만 내부 이동량은 λ/3λ, 단일 inflowFraction의 잔류 생성량은 fλ/3fλ다. 현재 연산식의 제한 조건에서 유도한 discrete 한 Step 반례이며 실제 Scene/GPU 재현은 미실행이다.

따라서 소수 대표 State만으로 충분하다고 가정하지 않는다. edge 공동 정보·내부 총 이동 장부와 그 독립 갱신이 후보이며, 단순한 graph/microgrid 또는 의미상 적절한 규칙 변경으로 충분하면 새 coarse 기여를 추진하지 않는다. hinge feature·양수 cubature의 존재, 보존성 조건, 실제 새 갱신법이 없는 부분도 심화 추론에 구분했다.

---

## source-analysis-11-contribution-proof-obligations

원본: `30_Deep-Analysis/11_Contribution-Proof-Obligations.md`

### Contribution을 위한 심화 추론과 입증 조건

작성: **2026-10-27 / Asia/Seoul**. 사용자 요청: “새 Contribution이 입증을 위해 가능한 모든 추론을 해봐”. 현재 선택한 방향 1·2·3에서 주요 논리 경로를 확장했다. 모든 가능한 아이디어를 소진했다거나 신규성을 증명했다는 뜻은 아니다.

범위: 코드 읽기·수학적 유도·온라인 1차 문헌 확인·문서 작성. **앱/GPU/빌드/수치 실험/학습은 미실행**, 제품 코드 변경 없음. HEAD `880af1ebcffe601f0bac27413cad94055ce92812`와 기존 작업 트리를 기준으로 한다. 아래 [D]의 증명은 명시한 이상적 산술·연산 조건에 대한 것이며 GPU 측정/기계 검증이 아니다. 기존 [U/C/E/P/D/H] 구분은 [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-00-overview|Overview]]를 따른다.

#### 1. 결론부터: 입증 가능한 범위가 세 층이다

1. **문제의 존재:** 특정 관측/압축 정보가 부족함은 조건부 반례와 식으로 보일 수 있다. 이번에 이 범위를 확장했다.
2. **방법의 성질:** 양수성·저장량 수지·특정 feature 적합도·보호 오차의 조건부 bound를 증명할 수 있다. 기존 정리의 적용과 새 정리를 구별해야 한다.
3. **연구 기여:** 기존 강한 방법보다 중요한 문제를 더 잘 해결한다는 주장은 신규성 검토와 실제 비교 증거가 필요하다. 1·2만으로 SIGGRAPH 기여가 완성되지는 않는다.

현재 구현의 판정은 여전히 시스템 통합에 가깝다. 이번 추론은 후속 후보의 정의를 바꾼다. 특히 방향 3을 **반응률 보정만이 아니라, 내부 이동으로 생기는 전환까지 보존하는 coarse 표현**으로 좁힐 근거가 생겼다. 이것은 새 해법의 성능/신규성 입증이 아니다.

#### 2. 가장 직접적인 새 단서: 순유량이 사라져도 유입 반응은 남는다

##### 2.1 현재 구현이 요구하는 정보

[C] [SurfaceSolverPass2.comp](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Solver/SurfaceSolverPass2.comp:145)는 `Current + Input + Incoming - Outgoing - Decay`와 별도로 **Incoming 자체**를 ReactionScratch에 저장한다. [SurfaceReaction.comp::reactionRequest](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Solver/SurfaceReaction.comp:18)의 `inflowFraction`은 이 실제 유입 장부를 사용한다. [Modular baseline의 동결 규칙](C:/Users/yonai/Documents/MDSS_Engine/Scripts/Research/Plans/modular-baseline-v1.json:141)과 [제작 pilot의 준비 코드](C:/Users/yonai/Documents/MDSS_Engine/Scripts/Research/authoring_pilot.py:32)에 Waterfilm→잔류 State의 해당 규칙이 있다. `IceReactionLab.SRProfile` 원본에 이 규칙이 있다고 혼동하지 않는다.

[D] 한 coarse 영역 B 안의 fine 이동량을 \(F_{ij}\ge0\), 외부 입력·Decay·다른 반응은 0, 유일한 전환은 Waterfilm의 Incoming 중 비율 \(0<f\le1\)을 immobile 잔류 State A로 보내는 규칙이라고 하자. Transport의 공급량 제한은 유효하고 반응 공급량 경쟁은 없다. 그러면 한 Step의 잔류 증가량은

\[
\Delta A_B=f\sum_{j\in B}\sum_i F_{ij}
=f\left(\sum_{i\notin B,j\in B}F_{ij}+\sum_{i,j\in B}F_{ij}\right).
\]

내부 이동은 B의 Transport 순수지에서는 상쇄되지만 **두 번째 항은 반응에서 상쇄되지 않는다.** 외부 입출력이 전혀 없어도 내부 물 이동은 흔적을 만든다. Incoming fraction은 1 이하이고 recipient의 post-transport 보유량은 Incoming 이상이므로 이 단일 규칙에는 추가 반응 limiter가 필요하지 않은 조건이다.

이 식은 단순 request의 연속시간 근사가 아니라, 위 제한 조건에서 **현재 Transport + inflowFraction의 한 discrete Step**에 대응한다. FP 오차·cache 불일치·생산 장면에서의 재현까지 증명한 것은 아니다.

##### 2.2 모든 한 점 분포가 같아도 다른 결과: 네 texel의 반례

동일 면적·동일 Profile의 네 texel을 `1—2—3—4`로 연결한 고정 graph를 생각한다. Geometry/Gravity/Decay는 꺼 두고, 양방향 edge weight는 같고 양수다. Capacity C가 같고 saturation \(s_i=W_i/C\)의 차이만으로 이동한다고 하자. [Transport.glsl::rawFlux](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Solver/Transport.glsl:1)에 따라, saturation 차이 1인 edge의 이동량을 \(\lambda>0\)라 쓸 수 있다. \(2\lambda<C\)로 택하면 outgoing limiter가 활성화되지 않는다.

| 초기 saturation 배치 | 총 Waterfilm | 값의 전체 histogram | 내부에서 양의 이동이 생기는 edge | 내부 총 이동량 | 잔류 증가 |
|---|---|---|---|---|---|
| X=(1,1,0,0) | 2C | 1 두 개, 0 두 개 | 2→3 | λ | fλ |
| Y=(1,0,1,0) | 2C | 1 두 개, 0 두 개 | 1→2, 3→2, 3→4 | 3λ | 3fλ |

하나의 닫힌 coarse 셀로 묶으면 두 경우 모두 외부 boundary flux=0이다. 모든 위치 비의존 State 통계와 histogram, 정적 graph 자체가 같다. 그러나 총 잔류량은 다르다. 즉 **공간 배열을 버린 정확한 한 점 분포조차 충분하지 않다.** 같은 압축 입력을 받은 결정론적 모델의 두 잔류 예측 오차 중 적어도 하나는 fλ 이상이다(삼각부등식).

이는 코드의 graph 연산에 대한 이상화된 최소 반례다. 현재 Scene/UV mesh에서 이 네 texel graph를 만들고 실행한 것은 아니다. 실제 mesh에서는 추가 edge·경계·geometry·채널을 계측해 분리해야 한다.

동일 가중치·gravity OFF·limiter 비활성 조건에서는 내부 총 이동이 graph total variation \(\lambda\sum_{\{i,j\}}|s_i-s_j|\)에 비례한다. 임의 directed weight/geometry/limiter에서는 이 간단한 TV 식 대신 실제 directed flux 합을 사용해야 한다.

##### 2.3 연구 질문을 어떻게 바꾸는가

단순한 `coarse mean + covariance`보다 **State와 edge 연결의 공동 정보, 내부 총 이동량, 실제 유입 전환**이 중요하다. coarse cell face의 순유량만 보존하는 수송 모델은 현재 규칙을 재현하는 데 정보가 부족하다.

이 반례는 **Geometry OFF에서도 성립**한다. 그러므로 이것만으로 State-Driven Geometry Coupling 특유의 새로운 문제가 입증됐다고 할 수 없다. 정적 edge 모델로 해결되는지 먼저 확인하고, 이후 State가 바꾸는 형상이 그 모델을 실제로 무너뜨리는 경우에만 동적 결합의 추가 해법을 주장한다.

하지만 반론도 강하다. ‘유입 1회당 f’라는 규칙은 fine hop 수에 의존한다. 실제 제작자는 이 의미를 유지하기보다 거리/시간 기반 규칙을 원할 수 있다. 그런 경우 표준적인 규칙 재정의가 더 적절하며, 원치 않는 격자 의존성을 보존하는 복잡한 알고리즘을 연구할 이유가 약하다. **고정 fine 모델의 기존 결과를 저비용으로 재현할 필요**와 **해상도에 독립적인 새 모델의 필요**를 먼저 구별해야 한다. 물리 부피 모델로 전환할 필요는 없다.

[P] 이동 중 누적량을 따로 다루는 개념 자체는 transition/impulse reward 모델에도 존재한다 [R22]. 이것이 MDSSP와 같은 모델이라는 뜻은 아니지만, ‘순변화와 경로상 누적 사건을 구분’하는 발상이 새롭다고 주장할 수 없다. 반응 수송의 미해결 fluctuation을 다루는 선행연구 [R23]도 추가 대조 대상이다.

#### 3. 임계값 반응과 capped geometry의 공통 구조

[[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-reference-09-analytical-findings|이전 반례]]의 평균/분산 부족을 더 일반적으로 설명한다. 한 block의 면적 가중 평균을 E, Ice/면적을 i, Heat/면적을 h라 두자.

\[
M(t)=E[i(h-t)_+],\qquad
\bar r_{\rm melt}=kM(\tau),\quad k=0.5.
\]

[D] i≥0이고 가중 첫 모멘트가 유한하면 M은 t에 대해 감소·convex하고, 오른쪽 미분은 \(M'_+(t)=-E[i\mathbf1_{h>t}]\)다. 분포 미분의 의미에서 두 번째 미분은 **Ice로 가중된 Heat 분포**다. 일반적인 stop-loss/hinge transform의 가중 형태이며 새 수학이 아니다 [R14]. 전체 joint 분포나 공간 배열을 복원하는 식도 아니다.

한편 한 State의 capped 높이는

\[
\bar g(c)=\kappa E[\min(s,c)]
=\kappa\{E[s]-E[(s-c)_+]\}.
\]

따라서 현재 융해와 높이 cap은 모두 **경계 초과량의 평균**을 참조한다. ‘임의 차수의 모멘트를 추가’하는 대신, 선언 rule의 threshold와 Capacity 위치에서 필요한 hinge feature를 선택하는 것은 합리적인 후보 설계다.

##### 3.1 제한적으로 얻을 수 있는 보장

Heat를 대표값 \(\hat h\)로 치환하되 같은 i를 유지하고 \(|h-\hat h|\le\delta\)이면, positive-part의 1-Lipschitz 성질에서

\[
|\bar r-\widehat{\bar r}|\le kE[i]\delta.
\]

같은 방식으로 \(|s-\hat s|\le\delta_s\)이면 capped 평균 높이 오차는 \(\kappa\delta_s\) 이하이다. i까지 비음수 값으로 근사하면 bound는 \(kE[|i-\hat i|(h-\tau)_+]+kE[\hat i]\delta\)로 쓸 수 있다. 이 bound는 초기 연산 관측에 대한 것이지 trajectory나 Flux 오차 보장이 아니다.

Heat bins가 τ에서 나뉘고 각 bin의 \(E[i\mathbf1_b]\), \(E[ih\mathbf1_b]\)를 정확히 보존하면 초기 융해 request는 정확히 복원된다. 그러나 시간 진행으로 bin 경계를 넘는 양과 mixed moments를 어떻게 갱신할지는 남는다. 조건부 통계와 closure 자체는 CMC 등의 선례 [R15]가 있다.

##### 3.2 남는 후보와 쉽게 기각되는 주장

[H] 여러 rule과 cap을 읽어 **필요한 feature·분할 경계·허용 오차를 선택**하고, 대표 상태와 edge 정보를 함께 갱신하는 제한된 모델을 검토할 수 있다. 모든 rule에 대해 exact한 범용 압축을 주장하지 않는다. generalized mass action·여러 modifier·공급량 경쟁·시간 세분화를 포함하면 초기 request feature만으로 실제 finite-step extent를 보존하지 못한다.

정해진 threshold histogram이나 2/4-node 표준 quadrature로 충분하다면 이것은 기존법의 적용이다. ‘반응식에서 필요한 feature를 자동으로 뽑았다’는 구현 편의만으로 강한 Contribution이 되지 않는다.

##### 3.3 현재 계수에서 융해의 형상 효과는 부호도 바뀐다

[C,D] `IceReactionLab`의 cavity=0 조건에서 world 요청 높이는 \(g(i,w)=0.035\min(i,4)+0.0063\min(w,2)\)다. Heat는 높이에 기여하지 않는다. 수송/입력/Decay를 분리하고 융해 extent/면적/시간을 ρ라 하면 cap 경계 밖에서

\[
\dot g=\rho\{-0.035\mathbf1_{i<4}+0.0063\mathbf1_{w<2}\}.
\]

- i>4, w<2에서는 **+0.0063ρ**: 숨은 초과 Ice가 줄어도 Ice 높이는 그대로이고 새 Waterfilm 높이가 더해진다.
- i<4, w<2에서는 **−0.0287ρ**: Ice 높이 손실이 Waterfilm 증가보다 크다.
- 두 채널 모두 cap 위이면 이 반응만의 순간 높이 변화는 0이다.

따라서 ‘얼음 융해량이 늘면 높이가 항상 감소한다’는 가정을 optimizer나 오차 추정에 넣을 수 없다. State-Driven Geometry Coupling의 민감도가 branch에 따라 바뀌는 구체적인 조건이다. 실제 수송을 켠 이후 높이/Flux 곡선은 별도 검증이 필요하다. artistic occupancy의 현 계산 결과이며 물리적 융해 법칙이나 새 현상으로 주장하지 않는다.

#### 4. 양수 대표 상태의 존재는 보일 수 있으나 새 정리는 아니다

현재 fine samples \(x_j\)와 면적 가중치 \(\alpha_j\ge0,\sum\alpha_j=1\), 보존하려는 m개의 유한 feature \(\phi(x)\in\mathbb R^m\)를 둔다. 예를 들면 State 평균, 각 request, capped height다.

[D,P] \(v=\sum_j\alpha_j\phi(x_j)\)는 feature vectors의 convex hull에 있다. Carathéodory로 **기존 samples 중 최대 m+1개와 비음수 가중치**를 택해 같은 v를 표현할 수 있다. 상수 feature를 포함한 공간의 rank로 더 날카로운 bound를 쓸 수 있으며, finite-function cubature는 Tchakaloff 확장에 이미 포함된다 [R16].

따라서 ‘비다항 반응·형상 feature를 양수 가중치로 동시에 맞추는 작은 snapshot 표현’의 존재는 설명할 수 있다. 하지만 다음을 보장하지 않는다.

- 한 번 선택한 samples가 이후에도 같은 오차를 유지함.
- 실제로 m이 충분히 작음, 선택/갱신 비용이 fine보다 작음.
- 노드별 양수성만으로 공간 수송 수지와 incoming 장부까지 맞음.
- coarse 상태만으로 다음 대표 상태를 선택할 수 있음. fine 재조회가 매 Step 필요하면 실용적인 압축이 아님.

[P] Graphics의 An–Kim–James 2008은 특정 재질/형상에 맞춘 cubature와 runtime error estimation을 이미 제시했다 [R17]. Empirical Cubature [R18] 및 2026 adaptive-weight preprint [R19]도 존재한다. **‘feature 맞춤 양수 quadrature + 적응 가중치’만으로 최초성 주장은 불가**하다.

#### 5. 실제 연구 방법으로 남는 방향 3의 최소 설계

[H, 미구현] 후보는 **반응과 실제 유입 전환을 보존하는 적응형 표면 coarse graph**다. 명칭은 임시이며 Contribution 문장이 아니다.

| 저장/계산 요소 | 필요한 이유 | 미해결 핵심 |
|---|---|---|
| coarse 총량과 소수의 양수 대표 State | 저장량·threshold/cap feature를 보존 | 독립 시간 갱신, 대표 상태 분할/합침 |
| 대표 State 사이의 edge 공동 정보 | §2의 같은 histogram/다른 배열을 구별 | 면적 가중치와 edge incidence 가중치는 다름. 임의로 같은 marginal로 놓을 수 없음 |
| 내부 이동 및 양방향 경계 이동 장부 | inflowFraction의 실제 Incoming 재현 | 현재값만 저장해도 다음 이동량의 예측은 닫히지 않음 |
| 공유 축적 형상에 따른 edge 갱신 | State-Driven Geometry Coupling 유지 | 평균 높이로 edge 기울기/연결을 복원할 수 없음 |
| 오차가 큰 부분의 microcell fallback | 임계값·경로 변화 때 표현 실패 처리 | 대부분 fine으로 돌아가면 비용 이점 소멸 |

첫 알고리즘 초안은 node feature를 근사하는 양수 압축, 내부/경계 flux의 계산, source별 **모든 outgoing을 합친** 공급량 제한, Incoming 기반 전환, 반응/형상 갱신, 필요한 영역의 분할 순서다. 동일 inventory를 여러 edge에서 중복 사용하지 않아야 한다. 기존 limiter·finite-volume 수지·AMR를 재사용한 부분은 신규성으로 세지 않는다.

제한적으로 확보할 수 있는 성질도 있다. 대표 node의 저장 총량을 Q로 두고 같은 F를 source에서 빼고 target에 더하면 전체 수송 수지는 상쇄된다. 각 node에서 \(\ell^TS=0\)인 reaction invariant를 지키고, 재압축 때 해당 선형 불변량과 총량을 feature로 보존하면 split/merge에서도 이를 유지할 수 있다. 공급량을 넘지 않는 flux/extent와 비음수 대표 상태는 양수성의 충분조건이 된다. 모두 기존 수지/convex 표현 원리이며, **수지를 맞춘다는 사실은 fine 분포·수송·외관의 정확성을 보장하지 않는다.** 실제 floating point와 전체 연산 순서 검증도 남는다.

중요한 한계: pair 정보를 추가해도 다음 edge 상태는 주변 이웃의 영향을 받으므로 고차 공간 의존성이 생긴다. 따라서 **임의 장면·모든 미래의 exact compact closure**를 목표로 삼지 않는다. 제한된 Profile/입력/시간 범위에서 품질–비용의 유용한 차이가 목표다.

비교군: 평균+단위 보정, 조건부 histogram/QMOM, feature-matching empirical cubature, edge-aware graph aggregation, 같은 예산의 microgrid/AMR, **기존 fine solver+Sparse를 유지하고 렌더 LOD만 바꾸는 방법**. 마지막 대안이 충분하면 시뮬레이션 coarse화 자체의 필요가 약하다.

가장 강한 Reject: “경로 정보를 가진 microgrid를 일부 남긴 것 아닌가?”, “기존 inflowFraction의 격자 의존성을 재현하는 데 왜 새 방법이 필요한가?”, “압축 관리가 fine Sparse보다 싸다는 근거는?”. 이 세 질문에 실측/정량으로 답하지 못하면 주력 기여로 채택하지 않는다.

#### 6. 방향 2: 최적화 실패 전에 불가능을 판정할 수 있는 범위

##### 6.1 보존량으로 얻는 보호 오차의 하한

[D] 닫힌 영역, 외부 물질 입력·sink·Decay 없음, Ice→Waterfilm→AbsorbedWetness의 화학량론에서 \(I+W+A=M\)가 고정이다. 두 rollout의 차이는

\[
\delta I+\delta W+\delta A=0.
\]

흔적 변화 \(\delta A=d\)를 요청하고 \(|\delta I|\le\epsilon_I,|\delta W|\le\epsilon_W\)를 보호하면 **필요조건은 \(|d|\le\epsilon_I+\epsilon_W\)**다. 가중 L2 보호 손실을 \(P=\sqrt{(\delta I/\sigma_I)^2+(\delta W/\sigma_W)^2}\)라 하면 Cauchy–Schwarz로

\[
P\ge |d|/\sqrt{\sigma_I^2+\sigma_W^2}.
\]

예를 들어 흔적 총량을 1 줄이면서 Ice와 물의 총량 변화를 각각 0.1 이내로 제한하는 요구는 이 조건 아래 불가능하다. 공간 분포나 Geometry를 바꿔도 이 총량 제약은 피할 수 없다. 외부 입력/sink가 허용되면 그 budget을 포함해 다시 계산해야 한다. **기존 U-input은 Waterfilm 입력을 허용하므로 이 닫힌 예를 모든 U-input 과제에 적용하면 안 된다.**

##### 6.2 선언 rule로 만드는 도달 가능성의 바깥쪽 근사

reaction stoichiometry S, graph incidence D, 허용 입력 B를 이용해 시간별 장부를

\[
q_{n+1}=q_n+D f_n+S\xi_n+B u_n-d_n,
\qquad f_n,\xi_n,q_n\ge0
\]

로 놓을 수 있다. 실제 단계별 Input/Transport/Reaction 순서와 overflow/Decay를 표현하거나 충분히 느슨하게 포함하고, protection·input budgets를 더한 선형 제약으로 actual trajectory 집합의 **superset**을 만든다. rate/geometry 조건을 생략하면 실제보다 자유로운 수송·반응을 허용한다.

그 superset도 infeasible이면 actual 과제는 불가능하다. feasible이면 실제 kinetics·threshold·geometry·시간 순서 때문에 여전히 불가능할 수 있다. 변수 f/ξ는 실제 flux/extent의 누적량이지 자유 조작이 아니다. 수치 solver의 infeasible 표시를 인증으로 쓰려면 dual residual/허용오차 또는 검증 가능한 certificate가 필요하다. float roundoff·clamp 보정량까지 포괄하지 않은 장부는 제품의 모든 실행에 대한 인증이 아니다.

[P] 이는 Petri-net state equation/CEGAR [R20] 및 기존 수지 기반 최적화와 가까운 접근이다. 선형 장부, Farkas certificate, 반례에 의한 refinement 자체를 새 기여라고 하지 않는다.

##### 6.3 연구 후보로 남기는 범위

[H] **보호 요구의 충돌을 설명하고, 가능한 보상 입력의 최소 범위를 찾은 뒤 실제 coupled solver로 검증하는 편집 방법**을 검토한다. 결과는 (i) 검증된 편집 해, (ii) 제한된 가정에서의 불가능 certificate, (iii) 미해결/예산 소진을 구별한다.

공간–시간 의존 graph의 경로 밖은 보존될 수 있다는 사실도 표준 프로그램 slicing이다. threshold 아래 gradient=0만으로 경로를 삭제하면 유한 편집을 놓친다. Geometry·smoothing·전체 시간 간격의 의존성을 포함해야 하며, 실제 coupling의 halo 반경을 임의로 1-hop이라 두지 않는다.

강한 Baseline은 전역 bounded SQP/multi-start/derivative-free **+ 같은 수지 사전 검사**다. 그 사전 검사를 우리 방법에만 주고 우위를 주장하지 않는다. 실제 새 기여 후보는 비교적 작은 조작 집합으로 보호를 지키며 유효한 event 전환을 찾는 계산 방법과 비용 이점이다. 아직 그 방법과 이점은 미확보다.

#### 7. 방향 1: 후속 probe가 줄 수 있는 것과 줄 수 없는 것

##### 7.1 한 texel에서는 모호성을 단순한 probe로 풀 수 있다

[D] 같은 snapshot을 복제해, 각 복제본에 서로 다른 Heat/면적 입력 p₁,p₂를 즉시 더한다고 하자. i>0, 두 경우 모두 threshold 위, 아직 수송/냉각/공급량 제한이 작동하기 전 초기 request/면적/시간을 관측한다.

\[
r(p)=\tfrac12 i(h+p-\tau),\qquad
i=\frac{2(r(p_2)-r(p_1))}{p_2-p_1},\quad
h=\tau-p_1+\frac{2r(p_1)}i.
\]

이것은 계산상의 단일 texel 식별성이다. 영상으로 r을 정확히 측정할 수 있다는 뜻이 아니며, 같은 상태 복제 없이 연속으로 두 pulse를 주면 i,h가 바뀌므로 식이 성립하지 않는다. finite-step reaction과 노이즈, 공간 평균에서는 추가 조건이 필요하다.

또한 MDSSP 내부에는 q가 이미 저장돼 있다. 이 연구를 ‘시뮬레이터도 모르는 State를 발견’하는 것으로 포장할 이유가 없다. 문제는 **사용자의 외관 목표가 향후 거동을 충분히 지정하지 않는 것**이다. probe는 제작 의도를 추가로 기술하는 수단이다.

##### 7.2 유한 probe로 모든 미래를 보장할 수 있는가

일반적으로 불가하다. 관측 y가 같아도 다른 미래가 가능하며, 모든 미래 입력에 대해 관측이 같은 상태만 합칠 수 있다는 조건은 behavioral equivalence/lumpability와 관련된 기존 개념이다 [R21/R24].

제한된 probe parameter 공간에 δ-net이 있고 response 차이 함수가 L-Lipschitz이며 모든 표본의 오차가 ε 이하이면 sup 오차≤ε+Lδ라는 표준 bound를 쓸 수 있다. 하지만 현재 stepAbove/stepBelow modifier, Geometry fallback, adaptive substep 경계에 대해 그러한 전역 L을 확보한 것은 아니다. 단지 몇 pulse에서 맞았다는 이유로 모든 미래 반응을 보장하지 않는다.

전체 augmented state와 입력이 같으면 deterministic forward의 미래도 같은 것은 자명하다. 따라서 ‘모든 State를 보존’보다 적은 제약으로 제작자가 중요한 미래만 보존하는 것이 유용한지 비교해야 한다. **참조 State 거리 penalty/constraint**도 주 Baseline에 넣는다.

방향 1의 단독 가능성은 여전히 낮다. adaptive probe selection을 추가해도 기존 experimental design·multiobjective control과 겹친다. 방향 2의 목표·평가로 쓰는 것이 우선이다.

#### 8. 한 Step의 적합도에서 장시간 기여로 넘어가는 데 필요한 것

압축/재구성한 상태를 같은 공간에서 비교할 수 있고, 해당 영역에서 full Step F가 L-Lipschitz, coarse 근사/재구성의 한 Step defect가 ηₙ 이하라면

\[
e_{n+1}\le Le_n+\eta_n,\qquad
e_N\le L^Ne_0+\sum_{j=0}^{N-1}L^{N-1-j}\eta_j.
\]

이는 표준 오차 누적식이다. **현재 MDSSP 전체에 이 조건을 증명한 것은 아니다.** 작은 height 오차·request 오차가 곧 작은 상태 오차라는 주장을 대신할 수 없다. Geometry 정규화의 비퇴화, 입력/State bounds, 시간 분할, discrete event, 내부 이동 장부의 영향까지 필요하다. L이 크거나 전역 bound가 지나치게 느슨하면 실용적 보장은 못 된다.

새 이론을 주장하려면 기존식의 재서술을 넘어, 실제 MDSSP 연산에 대해 계산 가능한 충분조건과 쓸 만한 bound를 제시해야 한다. 그렇지 않으면 이론 보장 대신 명확한 제한·검증 범위·실패 감지를 갖춘 empirical 방법으로 보고한다.

#### 9. 후보의 우선순위와 입증 계획

| 후보 | 현재 가능성 평가 | 논문으로 만들기 위한 실질적 새 부분 | 아직 빠진 결정적 증거 |
|---|---|---|---|
| 방향3: 반응·내부 이동·공유 형상을 함께 보존하는 coarse 계산 | **알고리즘 탐색 우선순위 1. 문제 근거는 가장 구체적, 성공 가능성은 미확정** | 표준 node-distribution/graph/AMR보다 작은 독립 갱신 표현으로 같은 오차를 유지 | 새 갱신법, 실제 제작상 영향, fine Sparse/표준 closure 대비 전체 비용 |
| 방향2: 도달 가능성과 보호 조건을 반영한 편집 | **목적 적합성 높음, 방법 기여는 중간 이하의 미입증 후보** | 유한 event 전환과 보상 범위를 효율적으로 선택하고 full forward에서 보호 유지 | nontrivial feasible witness, 강한 일반 optimizer 대비 성공률/비용, 유용한 인증 범위 |
| 방향1: 진행+후속 반응 목표 역설계 | **독립 기여 낮음** | 같은 intent를 더 적은 제약/probe로 안정적으로 지정하는 특별한 방법이 있을 경우 | 단순 state/probe loss 최적화보다 중요한 차이 |

여기서 유력도는 논문 채택 확률이 아니다. 방향3의 문제 근거가 강화됐다는 이유로 신규성이 확보됐다고 올리지 않는다. 1+2의 제품 목적 적합성과 3의 계산 문제 명확성을 구별한다.

##### 최초로 반증해야 할 세 가설

- **H3-E:** 실제 fine 결과를 유지할 제작 필요가 있고, 내부 이동 장부를 버린 coarse 모델의 잔류/형상 차이가 중요하다. 규칙 재정의·표준 microgrid·fine Sparse로 충분하면 기각.
- **H3-C:** 선언 반응/형상 feature와 소수 edge 정보가 독립적으로 갱신되며 메모리·총 비용 이점을 갖는다. 매 Step fine oracle·과도한 node 수·자주 전체 refinement가 필요하면 기각.
- **H2-P:** 실제 보호 편집에서 일반 optimizer가 중요한 구조적 어려움을 겪고, 구조를 이용한 방법이 같은 조작/오차/예산에서 이를 줄인다. 수지 사전 검사만 더한 표준법으로 충분하면 기각.

아직 어느 가설도 실측으로 검증하지 않았다. 자동으로 구현을 시작하거나 중지된 실험을 재개하지 않는다. 그동안은 표준 closure의 정확한 갱신식, coarse graph의 정보/메모리 요구, 보호 과제의 feasible witness를 종이 위에서 더 좁힐 수 있다.

#### 10. 지금 논문에 쓸 수 없는 문장과 조건부 문장

| 피해야 할 주장 | 지금 가능한 정확한 표현 |
|---|---|
| 다중 State와 Geometry Coupling을 처음 제안했다 | 구현·선례 대조가 있으며 최초성은 지지되지 않음 |
| 모든 모멘트를 보존하면 수송/반응을 재현한다 | 공간 배열을 버린 한 점 분포는 §2의 모델에서 충분하지 않음 |
| 양수 대표 상태를 사용한 새 수학을 제시했다 | 기존 convex/cubature 정리를 feature 선택에 적용할 수 있음 |
| 보호 편집의 불가능성을 일반적으로 판정한다 | sound relaxation이 infeasible인 경우만 제한적으로 배제 가능 |
| future-aware 역설계로 모든 후속 거동을 보장한다 | 지정된 probe/시간/조작 범위에서만 검증할 계획이며 아직 미실행 |
| SIGGRAPH Contribution이 입증됐다 | 코드 기반 반례와 구체적인 후보·입증 조건을 확보했지만 새 해법/우위는 미입증 |

문헌 원문 접근 수준·실패는 [[90_Research-Archive/Research-Notes-2026-10/04_Literature/01_Related-Work#source-reference-07-related-work|R14–R24]]에 기록했다. 이 문서는 새로운 실험 결과가 아니므로 PERF 기록을 생성하지 않는다.

---

## source-analysis-12-exact-submodels-and-limits

원본: `30_Deep-Analysis/12_Exact-Submodels-and-Limits.md`

### 실행 없는 연구 — 정확한 최소 모델과 불가능 조건

작성: **2026-10-27 / Asia/Seoul**. 사용자 요청: “직접적인 실험없이 할 수 있는거 전부 해봐. 추론, 새로운 아이디어, 돌파할 지점 뭐든”.

[[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#source-analysis-11-contribution-proof-obligations|직전 분석]]을 이어 코드 읽기·기호 유도·문헌 대조만 수행했다. **앱·GPU·빌드·수치 시뮬레이션·학습·새 benchmark는 실행하지 않았다.** 아래 숫자는 식에 대입한 예이며 측정치가 아니다. 제품 코드 수정 없음. HEAD `880af1ebcffe601f0bac27413cad94055ce92812`와 기존 작업 트리를 읽었다. [C/D/P/H]는 코드/조건부 유도/문헌/가설이다. 전체 탐색 공간을 소진했다는 뜻은 아니다.

#### 1. 이전 분석에서 실제로 전진한 부분

- [D] 내부 이동에 따른 잔류 반응을 **fine 값을 매 Step 다시 읽지 않고** 갱신하는 제한된 두 texel 모델을 유도했다. 미래 잔류 오차의 상한도 얻는다.
- [D] 더 일반적인 닫힌 graph에서도 saturation 차이 수송·단일 유입 전환·시간 간격 조건 아래, **형상이 바뀌어도 유효한 미래 총 잔류량 상한**을 유도했다. 전역 형상/이미지 오차 보장은 아니다.
- [D] 모든 미래를 정확히 보존하는 고정 크기 압축은 일반적으로 불가능하다. MDSSP의 제한된 확산 경로에서 필요한 차원이 늘어나는 예를 구성했다.
- [D] 현재 형상이 같거나 현재 물막이 같다는 조건은 후속 수송 보호를 뜻하지 않는다. 보호 조건을 유한 시간·허용 입력으로 한정해야 한다.
- [D] 현 Ice 규칙에서 융해를 바꾸면서 한 substep 끝의 Heat와 capped 높이를 보존하는 **반응 단계의 대수적 witness**를 만들었다. 실제 U-input rollout의 witness는 아니다.
- [P] 기억을 가진 축소 모델, 목적별 축소, 적응형 오차 제어, 보상/누적량 보존은 기존 분야다. 아래 유도를 새 수학의 발견으로 주장하지 않는다.

실제 알고리즘 초안·중단 조건은 [[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#source-analysis-13-method-blueprints-and-decision|후속 방법 명세]]를 따른다.

#### 2. 두 texel에서 정확히 닫히는 ‘남은 잔류 생성량’

##### 2.1 모델과 코드 대응

동일 면적·Capacity·Profile의 두 texel만 연결한다. 이동하는 Waterfilm을 x,y, 두 texel에 쌓인 immobile 잔류 State의 합을 a라 한다. 잔류는 높이·다른 반응에 기여하지 않는다. 외부 Input/flux, Gravity, Decay, 다른 Reaction, Geometry 갱신은 없다. Waterfilm의 saturation spread만 켠다. 유일한 Reaction은 **Incoming의 비율 f를 물에서 잔류로 전환**, 0<f≤1이다.

[C] [Transport.glsl::rawFlux](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Solver/Transport.glsl:1), [Pass 1의 outgoing 제한](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Solver/SurfaceSolverPass1.comp:127), [Pass 2의 Incoming 저장](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Solver/SurfaceSolverPass2.comp:153), [reactionRequest](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Solver/SurfaceReaction.comp:18)에 대응한다. 이 inflow 규칙은 기존 연구용 계획/준비 코드에 있으며, `IceReactionLab` 원본의 두 continuous rule과 구별한다.

한 Step의 양방향 raw flux를

\[
F_{12}=\beta(x-y)_+,\quad F_{21}=\beta(y-x)_+,
\qquad 0\le\beta\le\tfrac12
\]

로 쓴다. β는 공통 saturation 계수·weight·dt/Capacity를 묶은 무차원 값이다. β≤1/2는 충분조건으로 선택했다. source의 outgoing은 보유량 이하이며, recipient의 post-transport 보유량은 Incoming 이상이어서 f≤1의 반응 limiter도 비활성이다. 이상적 산술에서 FP16 flux/cache 양자화는 제외한다.

##### 2.2 독립 갱신식과 증명

\(m=x+y\), \(d=x-y\), \(\rho=1-(2-f)\beta\)라 하자. d≥0이면

\[
x'=x-\beta d,\qquad y'=y+(1-f)\beta d.
\]

d<0이면 x와 y의 역할을 바꾼다. 두 경우 모두

\[
m'=m-f\beta|d|,\qquad d'=\rho d,\qquad
a'=a+f\beta|d|.
\]

0≤ρ≤1이므로 d의 부호는 뒤집히지 않는다. 이제

\[
\boxed{p=\frac{f}{2-f}|d|}
\]

를 **현재의 비균일한 물 분포가 이후 내부 이동으로 만들 수 있는 잔류량의 상한**으로 정의하면,

\[
\boxed{p'=\rho p,\quad a'=a+(1-\rho)p,\quad
m'=m-(1-\rho)p.}
\]

따라서 m,p,a만으로 block 총량의 다음 상태를 계산한다. 매 Step fine snapshot을 조회할 필요가 없다. m+a 및 a+p는 불변이고, 0≤p≤m가 유지된다. p는 별도 물질 저장량이 아니라 **m 안의 불균일성이 남길 수 있는 결과를 나타내는 변수**다. 질량 합에 m+p+a를 쓰면 이중 계상이다.

β가 일정하면 N Step 후

\[
a_N-a_0=p_0(1-\rho^N),\qquad m_N=m_0-p_0(1-\rho^N).
\]

β가 매 Step 달라져도 각 βₙ이 위 조건을 만족하고 독립적으로 알려져 있으면 ρᴺ을 \(\prod_{n=0}^{N-1}\rho_n\)로 바꾼 식이 성립한다. 곱이 0으로 수렴할 때에만 최종 잔류 증가가 p₀와 같고, 이동이 중간에 멈추면 더 작다.

**얻은 성질:** 현재 시점에서 내부 모드를 없애고 m,a를 그대로 유지할 때, 같은 닫힌 조건이 계속되는 미래의 block 총 잔류·물 총량 오차는 각각 p 이하이다. p≤ε인 경우에 한해 이 관측에 대한 생략 기준을 만들 수 있다. 공간 분포·최대 texel 오차·이미지 오차의 보장은 아니다.

##### 2.3 의미와 한계

평균값만 쓰면 두 texel을 즉시 균일하게 만들어 이후 Incoming 반응을 전혀 만들지 못한다. p는 이미 생긴 잔류량이 아니라 **아직 발생하지 않은 내부 이동의 효과**를 보관한다. 이것이 이전의 ‘통계가 부족하다’는 반례보다 진전한 부분이다.

그러나 두 물 변수 x,y를 m,p로 바꾸는 것만으로 차원을 줄인 것은 아니다. a를 aggregate할 수 있는 관측에서만 원래 두 잔류 값의 위치 정보를 생략한다. 중요한 후속 가능성은 **p가 작아진 block에서 내부 변수를 제거하는 기준**이지, 두 셀 예제 자체의 속도 향상이 아니다. 원래 fine Sparse도 정지/작은 활동을 잘 처리한다면 유용성이 없다.

이 유도는 기존 compartment/mode elimination/transition reward의 특수 사례에 가깝다 [R22, R25, R31]. 논문 신규성으로 확정하지 않는다.

##### 2.4 어디에서 즉시 깨지는가

| 추가 요소 | 필요한 정보 / 실패 이유 |
|---|---|
| 어느 한쪽으로 들어오는 외부 입력 | d와 −d는 같은 m,p이지만 같은 위치의 입력 후 \(|d+u|\)가 달라진다. p만으로 충분하지 않음 |
| 좌우 boundary flux | 각 port의 물량을 알려면 signed d 또는 동등한 방향 정보가 필요 |
| 다른 f를 가진 재질·direction별 계수 | 위의 하나의 ρ와 p 정의를 그대로 재사용할 수 없음 |
| 잔류가 Geometry에 기여 | 잔류가 어느 texel에 생겼는지에 따라 다음 edge weight가 달라짐. a의 합만으로 부족 |
| Ice/Heat의 국소 반응·공급량 경쟁 | 지역별 joint State와 반응 branch가 필요. 물의 contrast만으로 닫히지 않음 |
| 3개 이상 texel의 일반 graph | 각 edge contrast는 이웃 edge와 공유 inventory를 사용. edge마다 독립 p를 합하면 동일 물/미래 반응을 중복 계산할 수 있음 |
| 새로운 입력 뒤의 생략 보장 | 새 입력이 contrast를 다시 만들 수 있다. 이전 p≤ε는 새 과제의 보장이 아님 |

따라서 이 식으로 ‘일반적인 MDSSP coarse solver를 해결했다’고 쓰지 않는다. **외부 port와 state-driven edge를 가진 block에서도 작은 내부 상태와 쓸 만한 잔여 오차 한계가 존재하는가**가 새 연구의 실질적 질문이다.

##### 2.5 일반 graph: Geometry가 변해도 남는 제한적 상한

두 texel의 exact closure와 별도로, 닫힌 graph 전체에 적용하는 더 느슨한 bound를 유도할 수 있다. 각 texel의 고정 Capacity 총량을 Cᵢ>0, 물을 qᵢ, saturation을 sᵢ=qᵢ/Cᵢ라 하자. 단일 Incoming 전환 비율은 texel별 0≤fᵢ≤1이고, 잔류는 이동/소멸하지 않는다. 다른 입력·Decay·물 생성/소비 Reaction·Gravity drive는 없다. **물/잔류가 Geometry를 바꾸는 것은 허용**하되, flux가

\[
F_{ij}=k_{ij}(s_i-s_j)_+,\qquad k_{ij}\ge0
\]

형태이며 모든 미래 Step에서

\[
\sum_j\max(k_{ij},k_{ji})\le C_i
\]

를 만족한다고 요구한다. k에는 dt가 포함된다. 상대 방향 edge가 없으면 그 k는 0으로 둔다. 이 충분조건은 공급량 limiter를 비활성으로 만들고 다음 convex-combination 성질을 보장한다. 실제 기본 dt가 언제나 이 조건을 만족한다고 확인한 것은 아니다.

수송 후 유입 전환을 합친 갱신은

\[
s_i'=s_i-\sum_{s_j<s_i}\frac{k_{ij}}{C_i}(s_i-s_j)
 +(1-f_i)\sum_{s_j>s_i}\frac{k_{ji}}{C_i}(s_j-s_i).
\]

위 조건 아래 sᵢ와 이웃 sⱼ의 비음수 가중 합이며 가중치 합은 1이다. 따라서 한 시점 n의 최소/최대 saturation lₙ,uₙ은 모든 후속 시점의 값들을 감싼다. k가 State/Geometry에 따라 변해도 매 Step 조건을 만족하면 귀납적으로 성립한다. kᵢⱼ=kⱼᵢ 대칭성은 필요하지 않다.

총 물 Mₙ=Σqᵢ, 총 잔류 Aₙ, CΣ=ΣCᵢ에 대해 M+A는 일정하고, 미래 물량은 lₙCΣ 이상이므로

\[
\boxed{0\le A_t-A_n\le M_n-l_n C_\Sigma=:B_n\quad(t\ge n).}
\]

또 각 texel의 물 상태는 같은 saturation interval에 남으므로, 시점 n의 물 상태를 고정한 근사와 비교한 미래 물 상태 L1 오차에는 \(\sum_i|q_i(t)-q_i(n)|\le C_\Sigma(u_n-l_n)\)라는 느슨한 bound도 있다. 잔류 변화는 비음수라 그 L1 오차는 위 Bₙ과 같다. 형상/normal/렌더 오차를 이 식에서 곧바로 결론내리지 않는다.

[C] [buildTransferWeight](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Geometry/SurfaceDynamicWeightsUpdate.comp:98)는 유효 경로의 distance/normal/profile weight를 [0,1]에 제한하고, `rawFlux`의 cavity Retention도 [0,1]이다. **Gravity OFF의 saturation 항**에서는 material rate·dt·최대 이웃 수로 k 상한을 정할 수 있다. 따라서 동적 형상 때문에 coefficient가 바뀐다는 사실 자체는 위 증명을 깨지 않는다. 고정 footprint Capacity도 이 분석에 부합한다. FP16 raw flux 양자화·산술/동일 Step cache 일관성은 이 이상적 식의 범위 밖이다.

이 bound가 Bₙ≤εA 및 CΣ(uₙ−lₙ)≤εW이면, 위 조건이 유지되는 닫힌 영역에서 내부 계산을 멈췄을 때 지정 State 총량/L1 오차가 제한됨을 뜻한다. 하지만 **그 영역을 다른 영역과 연결하거나 새 입력을 주면 기존 인증은 만료**된다. 빈 texel 때문에 lₙ=0이면 Bₙ=Mₙ으로 매우 느슨할 수 있다. 많은 State가 계속 반응하는 전체 MDSSP의 정지 기준은 아니다.

[P,D] 사용한 이론은 고전적인 discrete maximum principle/convex combination과 수지 [R33]다. 새 maximum principle을 발명한 것이 아니다. 남는 후보는 이런 bound를 실제 제작에 유용한 open block·다중 반응 범위까지 넓히고, 기존 fine Sparse보다 적은 비용으로 계산을 중단/재개하는 방법이다. 그 확장과 효과는 미확인이다.

#### 3. Fine 제거의 대가는 기억 또는 추가 상태다

[D] 고정된 선형 영역에서 resolved z, unresolved r를

\[
z_{n+1}=Az_n+Br_n+Eu_n,\qquad
r_{n+1}=Cz_n+Dr_n+Fu_n
\]

로 나눈다. 두 번째 식을 반복 대입하면

\[
z_{n+1}=Az_n+BD^n r_0+Eu_n+
\sum_{j=0}^{n-1}BD^{n-1-j}(Cz_j+Fu_j).
\]

fine 성분을 식에서 없애도 **초기 미세 상태와 과거 입력/거동**이 남는다. 현재 평균의 Markov 모델만으로 대체할 수 있다는 보장은 없다. \(\|D\|\le\gamma<1\)이면 오래된 memory tail의 기하급수 bound를 만들 수 있지만, 현재 MDSSP의 모든 block에 이 안정성/시간척도 분리를 증명한 것은 아니다. 느린 내부 모드가 많으면 기억 비용도 크다.

[P] Mori–Zwanzig와 기존 ROM의 핵심 논점이다 [R25]. 기억을 저장한다는 발상·RNN으로 근사한다는 발상은 기여가 아니다. 위 두 texel의 p는 해당 특수 모델에서 기억을 한 비음수 변수로 나타낸 경우로 해석할 수 있다.

##### 3.1 정확한 고정 선형 quotient의 판정

\(q'=Lq+Bu\), 관측 \(y=Cq\), 장부 증가 \(a'=a+Rq\), 압축 \(z=Pq\)가 선형이라고 하자. 모든 q,u에서 정확한 축소를 원하면

\[
PL=\widehat L P,\quad C=\widehat C P,\quad R=\widehat R P,
\quad \widehat B=PB
\]

가 충분하며, 해당 선형 형태에서는 필요한 조건이다. 동치인 kernel 불변성으로 검사할 수도 있다. state 출력뿐 아니라 장부 R도 보존 조건에 넣어야 한다. affine 항은 상수 좌표를 붙인다.

이것도 기존 observability/lumping의 적용이다. 일반 MDSSP는 반응 곱·limiter 비율·Geometry 정규화 때문에 branch를 고정해도 전체가 선형이 아니다. 이 검사를 전체 Solver의 exact compiler로 확대하지 않는다.

#### 4. 모든 미래를 정확히 유지하는 소수 변수의 보편 해법은 없다

동일 면적·Profile, Geometry/Gravity/Reaction/Decay OFF, outgoing limiter 비활성인 n texel path를 생각한다. 포화도 차이에 의한 양방향 positive-part는 순수지에서 차이로 합쳐져

\[
q_{t+1}=Aq_t,\qquad A=I-\alpha L_{\rm path},
\quad 0<\alpha<1/2
\]

가 된다. 모든 q가 양수인 작은 열린 영역에서도 이 모델을 쓸 수 있다. 첫 texel의 물량 \(y_t=e_1^Tq_t\)만 관측한다.

\[
\mathcal O_n=\begin{bmatrix}e_1^T\\e_1^TA\\\cdots\\e_1^TA^{n-1}\end{bmatrix}.
\]

k번째 거듭제곱은 첫 texel에서 k edge보다 멀리 닿지 않으며, 열 k+1의 계수는 유일한 최단 경로의 \(\alpha^k\ne0\)이다. 이 행렬은 위쪽 성분이 0인 삼각 형태로 대각이 1,α,…,αⁿ⁻¹이므로 rank n이다.

**결론:** 임의 초기 상태에서 첫 texel의 n개 시점 값을 정확히 재현하는 선형 축소 모델은 최소 n차원을 필요로 한다. differentiable encoder E:ℝⁿ→ℝᵐ와 differentiable reduced rollout H가 열린 집합에서 \(H(E(q))=\mathcal O_nq\)를 만족한다고 요구해도 chain rule의 rank로 m≥n이다. 비연속 실수 부호화나 제한된 초기 상태 집합까지 배제하는 정리는 아니다.

시간 범위를 H<n−1로 줄이면 같은 행렬의 H+1개 행 rank는 H+1이다. 이 예에서는 **얼마나 먼 미래까지 보호/예측할지**가 최소 정보량에 직접 영향을 준다. 시간은 discrete Step 수이며 dt를 바꾸면 비교 계약도 달라진다. 작은 α의 먼 영향은 아주 작을 수 있으므로 exact rank 하한을 실용적인 근사 차원 하한으로 오용하지 않는다.

[P] 이는 고전 observability [R26]를 이 graph에 적용한 증명이다. 새 Complexity 정리나 MDSSP만의 한계가 아니다. 다만 모든 mesh·모든 입력·모든 미래를 소수의 대표 State로 정확히 계산하겠다는 연구 목표는 버릴 근거가 된다.

#### 5. Geometry 보호의 강도와 수송 보호의 강도는 다르다

##### 5.1 현재 물이 균일하면 Geometry 차이가 숨을 수 있다

고정 형상의 symmetric diffusion 단계에서 \(w'=w-L(g)w\)라 하자. g→ĝ의 한 Step 영향이 0일 조건은

\[
[L(\hat g)-L(g)]w=0.
\]

균일 w는 어떤 해당 Laplacian에서도 kernel에 있으므로 형상이 달라도 현재 이동은 0이다. 모든 w에서 같은 결과를 요구하면 L(ĝ)=L(g)가 필요하다. 즉 한 장면의 균일/빈 물막에 대한 ‘보호 성공’은 대단히 약한 검사다.

현재 w에서 차이가 없어도 다음 \(L(g)w\) 방향 또는 새 input 뒤에는 차이가 생길 수 있다. 고정 선형 경로의 N Step 보호는 \(CA^kw=C\widehat A^kw\) (필요한 k 범위), 모든 입력까지 포함하면 \(CA^kB=C\widehat A^k\widehat B\) 같은 추가 조건을 요구한다. 일반 coupled nonlinear 모델에서 이 행렬 식을 그대로 사용하지 않는다.

##### 5.2 Height를 보호하는 방법과 후속 반응을 보호하는 방법

[C,D] cavity=0인 `IceReactionLab`의 world 요청 높이:

\[
g(i,w)=0.035\min(i,4)+0.0063\min(w,2).
\]

두 채널이 cap 아래이고 반응만으로 Ice→Water extent가 m만큼 발생하면 \(\Delta g=-0.0287m\). 같은 초기 상태에서 extent의 편집 차이를 δm라 하면, 다른 입력/효과가 없는 이 branch에서

\[
|\delta g|=0.0287|\delta m|.
\]

따라서 geometry tolerance εg는 \(|\delta m|\le\epsilon_g/0.0287\)를 강제한다. 반면 i>4,w>2가 전체 비교 구간 유지되면 해당 height의 변화는 0이다. **같은 목표라도 cap branch에 따라 보호 가능한 자유도가 달라진다.**

형상 동일은 Ice 상태·융해 속도 동일을 뜻하지 않는다. Saturation은 저장 State를 참조하고 reactionRequest도 unclamped Ice를 읽는다. geometry를 유지하는 숨은 초과량 편집은 미래를 바꿀 수 있다. 이 자유도는 의도적으로 쓸 수도, 보호 검사에서 막아야 할 수도 있다.

##### 5.3 현재 두 반응식의 대수적 feasible witness

이하 i,w,h는 **ReactionScratch 입력 plane의 면적당 값**이다. 전체 Input/Transport/Decay를 포함한 실행 가능한 contact timeline이라고 주장하지 않는다. 한 continuous substep dt=δ=1/240, limiter 비활성, h>τ=0.25라 하면 실제 두 rule은

\[
m=\tfrac12 i(h-\tau)\delta,\quad
c=\tfrac12 hw\delta,\qquad
i'=i-m,\ w'=w+m,\ h'=h-0.45m-c.
\]

i=5,w₀=3,h₀=0.5에서 baseline h′를 H*라 두면 H*=0.495703125이다. 원하는 h>h₀를 정한 뒤

\[
w(h)=\frac{2(h-H^*)}{\delta h}-0.45i\frac{h-\tau}{h}
\]

로 놓으면 **h′=H***가 정확히 유지된다. h=0.501이면 w≈3.94760479, 융해량은 baseline보다 1/96000 증가한다. 두 경우 모두 i′>4,w′>2이고 초기·최종 height는 0.1526이다. 반응 수요는 10% depletion 제한보다 충분히 작아 위 δ 선택과 limiter 비활성 가정에 모순이 없다.

이 결과가 보여주는 것은 ‘보호 편집이 언제나 불가능하다’가 아니라는 작은 대수적 사례다. 보호하는 것은 **그 한 substep 끝의 Heat와 capped height**뿐이다. 물 총량·Ice 총량·전체 Heat 곡선·이후 probe는 보호하지 않는다. 새 물 입력 비용도 공짜가 아니다. geometry coupling은 활성 변화가 없는 사례이므로 핵심 연구 데모로 쓰지 않는다. 실제 U-input의 bounds·접촉 형상·전 단계 수송/Decay·복수 substep을 통과하는 witness는 아직 없다.

이 식은 표준 optimizer가 쉽게 풀 수 있는 강한 sanity baseline이다. 이보다 못한 새 편집 방법은 버려야 한다.

#### 6. 규칙 재정의로 문제가 없어지는지 먼저 확인한다

고정 경로를 따라 같은 물질이 N번 이동하고 각 이동에서 f만큼 남긴다는 이상화에서는 생존 비율이 (1−f)ᴺ이다. hop마다 같은 f를 쓰면 경로 분할에 의존한다. 단방향 경로 구간 길이 ℓ에 대해 \(f(\ell)=1-\exp(-\kappa\ell)\)라 두면 두 구간의 생존 비율 곱은 합친 길이의 비율과 같다.

이는 표준 지수 감쇠의 semigroup 성질이고 새 알고리즘이 아니다. 또한 **현재 MDSSP의 확산 flux·분기·공급량 경쟁 전체에 대한 격자 불변성 증명이 아니다.** 확산성 random walk에서 이동 거리 총합은 refinement에 따라 발산할 수도 있어, 단순히 distance 계수를 붙이면 해결된다고 하지 않는다. 시간 노출·표면 접촉·순수송/총수송 중 어느 의미를 의도하는지부터 선택해야 한다.

따라서 방향3의 갈림길은 다음과 같다.

- 기존 fine preset의 결과를 보존해야 한다면: 누적 유입과 미해결 내부 이동을 재현하는 축소 문제.
- 해상도 변화에도 의미가 유지되는 제작 규칙이 목표라면: rule 의미와 discretization을 먼저 재정의. 그 뒤 기존 FV/반응 모델로 충분하면 새 closure 연구 중단.

두 목표를 혼용해 새로운 모델이 원래 결과와 다르다는 사실을 ‘coarse 오차 감소’로 보고하지 않는다.

#### 7. 수학적으로 확인한 것과 아직 확인하지 못한 것

| 주장 | 현재 수준 |
|---|---|
| 두 texel의 m,p,a 갱신식·보존/양수성·누적 오차 상한 | 명시한 discrete 이상 모델에서 대수적으로 유도 |
| 동적 coefficient를 허용하는 닫힌 graph의 미래 잔류/물 L1 bound | saturation-only·단일 전환·고정 Capacity·충분한 dt 조건에서 유도. open block/전체 MDSSP 적용 불가 |
| 같은 식이 실제 GPU 결과를 허용오차 안에서 재현 | 미실행 |
| 외부 port/다중 반응/동적 Geometry까지 작은 표현으로 확장 | 미해결 |
| 전체 미래를 exact하게 유지하는 범용 저차 압축 | path 예제에서 제한을 입증. 보편 주장 불가 |
| capped geometry 아래 보호 편집의 반응 단계 witness | 명시한 한 substep 식에서 유도 |
| 실제 U-input에서 유용한 편집 과제를 만족 | 미검증 |
| 위 방법이 기존 ROM/optimizer/AMR보다 우수 | 미검증 |
| 새로운 논문 Contribution | 미입증 |

이 문서의 진전은 ‘논문이 된다’는 결론이 아니라, **독립 갱신이 가능한 최소 출발점과 그 확장을 막는 정확한 경계**를 확보한 것이다.

---

## source-analysis-13-method-blueprints-and-decision

원본: `30_Deep-Analysis/13_Method-Blueprints-and-Decision.md`

### 실행 없는 연구 — 방법 초안, 돌파 지점, 중단 조건

작성: **2026-10-27 / Asia/Seoul**. [[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#source-analysis-12-exact-submodels-and-limits|최소 모델·한계 유도]]의 후속 방법 명세다. **아래 방법은 제안이며 제품에 구현하거나 실험하지 않았다.** 로컬 실행 중지 유지. 사용자가 제외한 축적 순서/층 표현은 다시 제안하지 않는다.

#### 1. 이번에 바꾸는 질문

‘MDSSP의 모든 기능을 더 일반적으로 만들자’는 목표는 기여를 흐린다. 이제 두 질문을 분리한다.

1. **방향3:** 이미 아는 coarse 평균이 아니라, 아직 일어나지 않은 반응·이동 결과를 만드는 내부 상태를 얼마나 작게 유지할 수 있는가?
2. **방향1+2:** 사용자가 보호하려는 진행·후속 응답을 명시했을 때, 어떤 변경은 허용되고 어떤 변경은 반드시 다른 결과를 깨뜨리는지 계산으로 설명할 수 있는가?

두 축은 ‘향후 결과에 필요한 정보’라는 공통 관점이 있지만, 이를 하나로 묶었다는 사실은 기여가 아니다. 한 방법에서 성과가 나기 전 두 논문 분량의 시스템을 동시에 구현하지 않는다.

#### 2. 후보 A — 내부 반응 잠재량을 유지하는 적응형 block 모델

##### 핵심 설계

[[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#2. 두 texel에서 정확히 닫히는 ‘남은 잔류 생성량’|정확한 두 texel 유도]]를 출발점으로 한다. 모든 histogram을 저장하는 대신 다음을 저장한다.

- 보존할 State 총량.
- 외부와 접촉하는 port 상태/양방향 flux.
- 내부의 아직 소멸하지 않은 mixing/transport modes.
- 각 mode가 남길 수 있는 잔류 반응량 및 오차 상한.
- threshold/cap/geometry 변화로 기존 mode 모델이 유효하지 않게 되는 조건.

두 texel에서는 p가 독립 갱신되며, 닫힌 미래에서 총 잔류 오차를 제한할 수 있다. 일반 block에서는 이런 비음수 잔여량/상한을 구성할 수 있는지가 핵심 미해결 부분이다. ‘edge마다 p를 넣고 더한다’는 방식은 shared inventory 이중 계상 때문에 정당화되지 않는다.

추가로 [[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#2.5 일반 graph: Geometry가 변해도 남는 제한적 상한|닫힌 graph bound]]는 saturation-only 수송·유입 전환·충분한 dt 아래 동적 Geometry가 계수를 바꿔도 미래 잔류량을 B=M−s_min·ΣCapacity로 제한한다. 이는 **약한 근거로 시작하는 새 압축기보다, 먼저 인증 가능한 near-equilibrium 구간의 내부 계산을 생략하는 방법**이라는 대안을 준다. 다만 열린 경계/다른 반응에서 인증이 깨지고 최소 saturation=0이면 매우 느슨하다. 알려진 maximum principle의 응용이며 새 Sparse 기여를 확보한 것은 아니다.

##### 가장 좁은 알고리즘 초안 [H]

1. 최초 fine 상태에서 disjoint block과 port를 정의하고, block 총량/내부 상태/보호 관측을 계산한다. 이 비용은 포함한다.
2. 정적 transport와 단일 inflow 전환부터 시작한다. 각 block의 내부 업데이트를 signed mode 또는 작은 positive compartment graph로 표현한다.
3. 외부 flux는 source inventory에서 한 번만 차감하고 target의 Incoming 장부에 한 번만 기록한다. 내부 flux의 전환과 boundary 유입 전환을 중복하지 않는다.
4. 내부 mode는 정해진 국소 식으로 갱신한다. fine 재조회를 상시 요구하는 표현은 후보에서 제외한다.
5. 남길 수 있는 관측 오차의 상한이 허용량보다 작을 때만 mode를 제거한다. 이후 port input이 mode를 재활성화할 수 있으므로 입력 분포 정보를 유지하거나 block을 분할한다.
6. 동적 Geometry를 추가할 때는 mode에 반영된 **잔류 위치·signed contrast·cap 상태**로 edge를 계산할 수 있는지 확인한다. 그렇지 않으면 이미 가진 coarse 정보에서 모르는 부분을 복원했다고 가정하지 않고 refinement/불확실성을 선언한다.

##### 이번에 드러난 중요한 위험: refinement는 잃은 정보를 복구하지 않는다

단지 m,p,a만 남긴 뒤 다시 fine으로 나눠도 잔류 위치와 전체 State 분포를 알 수 없다. 현재 축소값에서 임의로 fine을 재생성하고 이를 정답 fallback이라 부를 수 없다.

가능한 선택은 (i) fine checkpoint를 보관하고 필요 시 입력 이력으로 replay, (ii) coarse 정보와 일치하는 fine 집합을 보관하며 오차를 제한, (iii) detail을 남겨 메모리 절감 대신 연산만 절감, (iv) 근사 reconstruction의 오차를 별도 계상이다. 모두 비용/제약이 있다. checkpoint+replay도 fine 비용을 미룬 것이며 공짜가 아니다.

##### 기존법과 경쟁하는 정확한 지점

[P] memory ROM [R25], positive reduction [R32], reward-sensitive abstraction [R31], cubature [R17–R19]는 이미 존재한다. 남는 후보 차이는 **State가 edge geometry를 바꾸고 Incoming이 반응을 발생시키는 모델에서, 미래 누적 반응의 잔여 오차를 기준으로 내부 상태를 유지/삭제하는 구체적 방법**이다. 현재 이 전체 방법은 미완성이다.

Baseline: 같은 크기의 positive compartment graph, node/edge quadrature, microgrid/AMR, fine Sparse, geometry frozen vs updated, mode 없는 모델. 표준법에도 Incoming ledger와 같은 초기 정보/허용 관측을 준다. Reaction을 일부러 빼서 약한 비교군을 만들지 않는다.

**기각:** 중요한 block에서 내부 상태 수가 fine 수준 / 모든 port 입력 뒤 replay / geometry 복원 비용이 압축 이득을 소멸 / fine Sparse가 충분 / 고정 hop 반응의 결과를 유지할 제작 필요가 없음.

#### 3. 후보 B — 보호 조건으로 선택하는 축소 모델과 편집

##### 목적

State 전체를 작게 재현하는 대신, 사용자가 고정한 **편집 목표·보호량·허용 후속 입력·시간 구간**에 필요한 정보만 유지한다. 방향1은 여기서 미래 응답을 지정하는 인터페이스/평가 역할이다.

계약을 \(\mathcal C=(E,P,\mathcal U,H,\epsilon_E,\epsilon_P)\)로 쓴다. E는 바꿀 관측, P는 보호 관측, U는 허용 입력, H는 유한 시간이다. 모델 전체의 보편 보장 대신 해당 계약 안의 성공/실패를 주장한다.

##### 방법 초안 [H]

1. 보존량과 입력 budget으로 명백히 불가능한 요구를 제외한다. 동일 사전 검사를 Baseline에도 준다.
2. reaction·height cap·geometry·transport·Incoming ledger를 포함한 의존 graph에서 목표/보호 관측에 닿는 경로를 잡는다. 현재 gradient=0만으로 경로를 지우지 않는다.
3. 허용 조작의 범위 안에서 threshold/cap을 넘을 가능성과 margin을 계산한다. 알려진 불연속 경계는 별도 event 후보로 취급한다.
4. 경계에서 멀리 떨어진 영역은 기존 constrained optimizer/ROM으로 처리한다. 경계 근처만 유한 event 전환·추가 조작 support·더 세밀한 모델을 검토한다.
5. full forward 검증 또는 sound enclosure로 보호 허용오차를 확인한다. 검증 불가를 ‘보호됨’으로 바꾸지 않는다.

편집에 필요한 제어 영역과 정확히 재계산해야 할 영역은 다를 수 있다. 전체 adjoint/forward 비용, event 후보 수, 실패 replay를 포함해야 한다.

##### 이론적 바탕과 새로 해야 할 부분

[P] goal-oriented ROM은 최적 설계/제어/역문제에 이미 사용되고 [R27], DWR 기반 적응형 ROM도 있다 [R28]. spacetime editing과 재질 최적화/감축의 결합도 선례 [R30]가 있다. 따라서 ‘보호 지표를 loss에 넣고 중요 영역을 정밀 계산’만으로는 새 기여가 아니다.

실질적 새 해법이 될 여지는 **reaction의 threshold·형상 cap·유입 전환의 의존성을 이용해, 강한 일반 최적화가 놓치거나 비싸게 찾는 유한 편집을 더 작은 범위에서 찾는 방법**에 있다. 현재는 그러한 과제의 중요성도, 새 방법의 우위도 미확인이다.

Baseline: bounded SQP+다중 크기 FD/AD, multi-start, derivative-free, 같은 보존 사전 검사, 기존 goal-oriented ROM. 조작이 적으면 전역 grid/search도 강한 대조가 된다. 모든 후보가 같은 입력 bounds를 써야 한다.

**기각:** 작은 전역 optimizer가 충분 / 보호를 거의 하지 않아야 편집 가능 / event 분기가 폭발 / 실제 목표가 단순 State penalty로 충분 / geometry 경로를 포함해도 성능/성공률 차이가 없음.

#### 4. 후보 C — 편집 가능 범위를 설명하는 제한적 인증

이것은 B의 보조 기능이 될 가능성이 크고, 독립 주력 후보로 우선하지 않는다.

##### 세 가지 결과를 분리한다

1. **검증된 해:** 허용 입력으로 목표/보호를 만족하는 실제 rollout 또는 한정된 수학 모델의 witness.
2. **조건부 불가능:** sound outer approximation의 infeasibility, 또는 수지/형상 branch 하한에 의해 불가능.
3. **미해결:** 탐색 실패, 수치 예산 소진, enclosure가 너무 넓음. 불가능과 다르다.

현재 문서의 닫힌 물질 수지 하한, cap 아래 \(|\delta g|=0.0287|\delta m|\), 반응 단계의 보상 witness가 작은 예다. 이것들을 사용자의 요구를 자동 분해하는 데 쓸 수는 있으나 새 이론으로 세지 않는다.

##### 후보 방법 [H]

Profile에서 stoichiometry·modifier 경계·capacity·지원 State를 읽어, 제한된 조작 box에서 interval/affine bound를 전파한다. cap 아래/위가 확실한 부분은 단순 식을 쓰고, 경계를 넘는 부분만 분할한다. 동일 Step의 실제 순서를 반영한다. Geometry 정규화의 퇴화, clamp, FP rounding, 시간 세분화 분기는 별도 처리해야 한다.

결과가 보수적으로 너무 넓으면 certification을 포기하고 full forward의 경험적 검증만 보고한다. 같은 변수의 반복 사용으로 생기는 interval dependency와 수많은 texel의 branch가 큰 장애다. Flow* 등 기존 nonlinear/hybrid reachability [R29]를 못 하는 방법처럼 묘사하지 않는다.

**기각:** 결과가 늘 미해결 / 유용한 편집을 대부분 제외 / 인증이 forward보다 훨씬 비쌈 / 설명이 단순 보존량 이상의 정보를 주지 못함. 정확성 인증 자체가 프로젝트의 필요보다 커지면 연구 범위를 축소한다.

#### 5. 추가로 생각했지만 주력으로 추천하지 않는 아이디어

| 아이디어 | 매력 | 우선하지 않는 이유 / 재개 조건 |
|---|---|---|
| profile에서 자동 feature/branch compiler 생성 | 선언 모델을 재사용할 수 있음 | DSL/code generation만으로는 엔지니어링. A/B의 중요한 보장·비용 차이를 만들 때만 포함 |
| 학습된 memory closure / recurrent surrogate | 복잡한 미세 경로를 압축할 수 있음 | 데이터·일반화·양수성·분기 안정성 부담. analytic/표준 ROM보다 필요한 근거 없음 |
| coarse 실패를 자동으로 찾는 반례 생성기 | 잘못된 합치기를 진단 | 일반 differential/property testing과 CEGAR 선례. A의 검증 도구 역할이 우선 |
| 같은 외관에서 다른 후속 반응을 고르는 제작 도구 | 현상학적 모델의 목적과 잘 맞음 | hidden State 조작/다중 loss 자체는 약함. B의 제한 입력·보호와 결합될 때 의미 |
| incoming 규칙을 거리/시간 기반으로 재정의 | 해상도 의존 문제를 줄일 수 있음 | 표준 감쇠/반응 모델로 충분할 수 있음. 기존 preset 재현과 새 의미를 혼동하지 않음 |
| 논문을 ‘통합 시스템 분석’만으로 제출 | 반례와 재현 가능한 모델 계약을 정리 가능 | 작은 합성 반례만으로 SIGGRAPH급 중요성 부족. 실제 여러 시스템에서 반복되는 한계와 유용한 해결 지침이 필요 |

#### 6. 실행 없이 작성 가능한 알고리즘의 정확한 공백

현재 가장 중요한 미완성 부분은 A의 **외부 port+dynamic geometry를 가진 독립 갱신식**이다. 아래 정보를 추측으로 메우면 연구가 아니라 surrogate의 주장에 그친다.

- coarse 총량과 내부 modes에서 실제 recipient Incoming을 어떻게 나눌 것인가?
- 위치를 잃은 잔류량이 형상에 기여하면 새 normal/edge weight를 어떻게 복원하는가?
- cap/threshold가 넘어갈 때 basis/compartment를 어떤 기준으로 늘리는가?
- mode를 합치거나 제거할 때 수지·양수성과 미래 오차 budget을 함께 어떻게 보존하는가?
- 외부 입력을 받은 뒤에는 이전 residual bound를 어떻게 다시 유효하게 만드는가?
- fine oracle 없이 판단할 수 없는 경우 어떤 비용을 지불하고 복원하는가?

이 중 하나라도 ‘필요하면 fine을 매번 확인’으로 답한다면 그 비용을 포함해 기존 fine Sparse와 비교해야 한다. 문제를 숨긴 채 독립 closure가 완성됐다고 쓰지 않는다.

#### 7. 연구적 유력도와 현재 권고

| 후보 | 문제 근거 | 실행 없는 해법 진전 | 신규성/성공 전망의 정직한 판단 |
|---|---|---|---|
| A 내부 반응 잠재량·port 기반 적응형 block | 가장 구체적 | 제한된 exact update와 tail bound 확보 | **알고리즘 탐색 1순위**, 범용화 난도 높음. 새 기여는 미입증 |
| B 보호 계약에 맞춘 편집/축소 | 사용자 목적에 가장 가까움 | cap별 하한·sanity witness·의존 명세 | **실용 연구 1순위 / 알고리즘 탐색 2순위**. 강한 optimizer와 경쟁 필요 |
| C 보호 요구의 제한적 인증 | 수지/branch 예가 명확 | 조건부 certificate의 범위 정리 | 보조 후보. 기존 formal methods 대비 독자 이점은 약함 |
| 방향1 단독 future-aware authoring | 관측 모호성 확인 | 미래 시간/입력 범위의 필요 정리 | 단독 Contribution 유력도 낮음. B의 목표·평가로 유지 |

채택 확률을 수치로 줄 근거는 없다. ‘수식이 생겼다’는 이유로 A의 출판 가능성을 높게 확정하지 않는다. **문제만 있었던 방향3에서 제한된 독립 갱신 해법까지 한 단계 전진했다**는 평가가 적절하다.

#### 8. 실행 전에 고정할 판정표

| 판별 | 통과해야 할 내용 | 실패하면 |
|---|---|---|
| 의미 | 기존 hop 기반 반응 결과를 유지할 제작 요구가 실제 있음 | 규칙 재정의/표준 모델로 전환, A 보류 |
| 최소 정확성 | 두 texel 유도가 production 연산 순서/정밀도 아래 재현됨 | 수식-구현 차이를 먼저 해결. 새 모델 실험 금지 |
| 열린 block | 동일 예산 microgrid보다 적은 내부 정보로 port input을 처리 | 정적 toy model 이상 주장을 중단 |
| Geometry | state-driven geometry 변화에 필요한 추가 정보가 작고 유효함 | frozen/static 범위로 축소하거나 A 중단 |
| 총 비용 | 생성·복원·오차 평가·replay 포함 품질–비용 이점 | GPU 내부 몇 연산의 속도 개선으로 대체하지 않음 |
| 편집 | 허용 U-input의 비자명한 feasible witness가 있음 | optimizer 실패로 Contribution을 만들지 않음 |
| 강한 대조 | 같은 보호/입력 조건의 표준법보다 성공률 또는 비용 이점 | B/C를 제품 기능으로 취급, 방법 기여 축소 |

이 표는 후속 **실행을 허가하거나 예약하는 문서가 아니다.** 지금은 문헌·기호 분석·설계만 수행했다. 실증 없이 남길 수 있는 최종 결론은 유망한 질문과 제한적 해법, 반례, 기각 조건까지이며 실제 효과·속도·제작 유용성은 미판정이다.

---
