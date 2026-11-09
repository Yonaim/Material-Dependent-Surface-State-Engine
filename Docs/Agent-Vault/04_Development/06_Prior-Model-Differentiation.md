# 공통 계산 모델의 선행연구 대비 차이 — H4 검토

작성: 2026-10-27 · 코드 기준: `880af1ebcffe601f0bac27413cad94055ce92812` 이후 현 작업 트리. PERF-041/042의 미커밋 연구·계측 변경을 포함한다.

상태: **지정된 7편의 관련 원문·현행 코드 대조 완료. 현재 주장에 필요한 학술적 차별성은 확보하지 못함.** 새로운 실험, 성능 측정, Solver 변경은 수행하지 않았다. 이것은 문헌·모델 분석이며 새로운 PERF 결과가 아니다.

## 1. 판정

현재 MDSSP는 **고정 표면 graph에서 여러 비음수 State의 국소 이동·반응·공유 형상 갱신을 수행하는, 제어 가능한 현상학적 시스템**으로 설명할 수 있다. 구현의 계산 계약은 구체적이며 PERF-041은 형상 변화가 후속 수송에 실제로 영향을 준다는 증거를 제공한다.

그러나 다음 세 주장은 구분해야 한다.

| 질문 | 판정 |
|---|---|
| MDSSP가 구현한 정확히 같은 이산 식을 이 7편에서 찾았는가? | 찾지 못했다. 동일 식 부재가 신규성의 증거는 아니다. |
| 다중 현상·재질 규칙·형상 변화·후속 이동의 결합이 새로운가? | 그 넓은 주장은 성립하기 어렵다. Chen 2005와 Günther 2012에 직접적인 선례가 있다. |
| MDSSP의 더 구체적인 차이가 기존 방법의 중요한 제약을 해결했는가? | 아직 입증되지 않았다. PERF-042도 동일 모델 내부의 조정 구조만 비교한다. |

따라서 현 구현·현재 증거의 최종 분류는 사용자가 제시한 **3번: 기존 연구의 시스템 통합에 가깝다**이다. 향후 연구 문제의 가능성은 남지만, 지금 ‘의미 있는 새 시스템 기여’나 ‘새 알고리즘이 반드시 필요한 문제’를 확인했다고 쓰지 않는다. 새로운 수치 알고리즘이 모든 시스템 논문에 필수라는 뜻도 아니다.

## 2. 근거의 종류와 확인 범위

- **[P] 원문 확인:** 아래 출처의 관련 절·수식·도표에서 확인한 내용.
- **[C] 코드 확인:** 현 작업 트리의 실제 함수와 데이터 흐름.
- **[E] 실행 증거:** 이미 기록한 PERF-041/042. 이번 작업에서 재실행하지 않았다.
- **[I] 분석·추론:** 대응 관계, 비용·표현 범위의 예상. 측정 결과가 아니다.
- **[U] 미확인:** 원문 미기재, 구현·정량 비교 부재. ‘지원하지 않음’과 다르다.

지정된 7편의 PDF를 확보해 비교에 필요한 절을 읽었다. 핵심인 Günther의 PDF p.3/6, Chen p.6, Lo p.8은 렌더링한 페이지도 확인했다. 전체 선행 구현을 실행하거나 모든 관련 분야의 신규성을 전수 조사하지 않았다. 공개 논문이 보여준 모델과 확장 가능한 인터페이스를 구분하며, 논문에 없는 기능을 불가능하다고 판정하지 않는다.

## 3. MDSSP의 실제 모델

### 3.1 한 Step의 계약

texel i, State k의 저장량을 S(i,k), 고정 macro footprint의 기준 면적 대비 비율을 A(i), Profile의 포화 기준을 c(i,k), 유효 형상을 G_n이라 두면 다음과 같다. 아래는 정상적인 비음수 입력과 유효 계수에서의 설명이다. 음수 외부 입력의 clamp 손실까지 보존한다고 주장하지 않는다.

```text
saturation(i,k) = S_n(i,k) / (c(i,k) A(i))
q(i→j,k)       = rawFlux(S_n, G_n, Profile, dt) >= 0
alpha(i,k)     = min(1, max(S_n(i,k)-decay(i,k),0) / sum_j q(i→j,k))
                 (분모가 0이면 1)
F(i→j,k)       = alpha(i,k) q(i→j,k)
T(i,k)         = max(0, S_n(i,k) + Input(i,k)
                       + sum_j F(j→i,k) - sum_j F(i→j,k) - decay(i,k))
S_(n+1)(i,:)   = R(T(i,:), positiveInput(i,:), incoming(i,:), dt)
G_(n+1)       = BuildGeometry(S_(n+1), macro geometry, meso geometry)
```

[C] `Transport.glsl::rawFlux`는 포화도 차이와 형상/중력 구동력을 더하고 거리·Normal·Profile 경계 가중치 및 cavity 이탈 저항을 곱한다. 수송 속도에 대한 독립적인 운동량 방정식은 없다. 새 Input·Incoming·Reaction 생성물은 같은 outer Step에서 다시 이웃으로 수송되지 않는다.

[C] `SurfaceReaction.comp::reactionRequest/main`은 선언된 연속 반응과 유입 비율 전환을 계산한다. 연속 반응은 공통 snapshot에서 요청을 계산하고 공유 소비 State의 공급량에 맞춰 extent를 제한한다. 내부 시간 분할은 구현돼 있으나 일반적인 강직 반응의 오차 보장 적분기는 아니다.

[C] `StateMath.glsl::accumulationHeight`는 State별 축적 기여를 합친다. **형상에 반영할 State는 Capacity로 제한하지만 실제 저장 State는 상한 없이 유지한다.** 따라서 저장량 보존과 형상 부피 보존은 다르다. macro footprint는 동적 적층 면적으로 다시 계산되지 않는다.

[C] `effectiveLocalPosition/effectiveLocalNormal`, Dynamic Geometry/Weights Pass가 위치·Normal·Concavity·간선 가중치를 갱신하고 후속 Step이 이를 읽는다. 공유 형상은 물막만의 입력이 아니다. 예를 들어 Heat는 축적 기여·중력 계수가 0이어도 활성화된 거리/Normal 가중치를 통한 영향을 받을 수 있다.

### 3.2 구현 위치

경로는 저장소 루트 기준이며 함수명과 함께 기록한다.

| 계산 계약 | 구현 근거 |
|---|---|
| 공통 State별 raw flux | `Shaders/Simulation/Solver/Transport.glsl:1` — `rawFlux` |
| Source 공급량 제한 | `Shaders/Simulation/Solver/SurfaceSolverPass1.comp:29` — `main`, `AvailableState`, `OutgoingFluxScale` |
| 동일 directed edge의 제한된 Incoming gather | `Shaders/Simulation/Solver/SurfaceSolverPass2.comp:16` — `main`, reverse direction/cache 및 NeighborScale 적용 |
| 선언 반응·공급량 경쟁·내부 시간 분할 | `Shaders/Simulation/Solver/SurfaceReaction.comp:18` — `reactionRequest`; `:64` — `main` |
| 유입과 기존 보유량 구분 | 위 Pass 2의 `ReactionScratch` positive Input/Incoming plane; Reaction의 Inflow 분기 |
| State→높이→후속 형상 | `Shaders/Simulation/Solver/StateMath.glsl:48` — `accumulationHeight`; `:98` — `effectiveLocalPosition`; `:111` — `effectiveLocalNormal` |
| 공유 형상·수송 가중치 | `Shaders/Simulation/Geometry/SurfaceDynamicGeometryUpdate.comp:15` — `main`; `SurfaceDynamicWeightsUpdate.comp:37/98` — `buildDynamicConcavityWeight/buildTransferWeight` |
| Step의 연산 순서 | `Source/SurfaceState/Solver/SurfaceStateSolver.cpp:306` — `TSurfaceStateSolver::RecordSteps` |
| Normal Map의 상대 높이 복원 | `Source/SurfaceState/Geometry/MesoGeometryBuilder.cpp` — `BuildMesoGeometry`, `EdgeHeightDelta`, 연결 성분 기준 고정·Laplacian 최소제곱·PCG |
| 공통 provider를 허용한 모듈 대조 | `Scripts/Research/ModularBaseline.h` — `TModularCoordinator::Record`, `MakeLocalFluxModules` |

### 3.3 보존성과 안정성으로 주장할 수 있는 한계

[I, 코드의 대수적 성질] 닫힌 graph에서 source의 동일한 F를 outgoing과 target incoming에 한 번씩 사용하면 모든 간선 항은 전체 합에서 상쇄된다. outgoing limiter는 decay 후 보유량 이상을 내보내지 않게 한다. 반응의 화학양론 벡터 ν_r에 대해 어떤 가중치 l이 모든 r에서 `lᵀν_r=0`을 만족하면, 같은 extent로 소비·생성을 적용하는 반응은 그 가중합을 보존한다.

이것은 통상적인 이산 flux/화학양론의 성질을 현재 코드에 적용한 설명이다. **새 보존 기법·새 안정성 정리로 제안하는 것이 아니다.** 전제에는 reciprocal graph, 같은 시점의 형상/edge flux, 유효 산술, 입력·소실 장부가 포함된다. 임의 Scene의 cache/Sparse 경로까지 이번 문헌 분석만으로 검증하지 않는다. 비음수 유지가 시간·공간 정확도, 물리적 에너지 소산, 수렴을 보장하지도 않는다.

[E] PERF-041의 Ice+Waterfilm 수지와 PERF-042의 채널 수지는 해당 실험 조건의 수치 검증이다. 형상 부피·열역학 에너지 보존의 검증은 아니다.

## 4. 지정 선행연구와 계산 모델 대응

PDF 페이지는 내려받은 원문에서 1부터 센다. 출판 페이지와 다른 경우 병기했다. [P1]–[P7]의 실제 링크는 §10을 따른다.

| 선행연구·확인 위치 | [P] 직접 확인한 모델 | [C] MDSSP의 구체적인 차이 | [I] 차이의 의미·대가 | 현재 기여 판정 |
|---|---|---|---|---|
| Günther 2012 [P1], §3–4.5, PDF pp.2–6 | material atlas, 입자-표면 재료 교환, 재사용 계수·규칙, 재료량→높이→Normal, 그 Normal을 사용한 후속 입자 응답 | 입자 운반 대신 고정 표면 graph의 채널별 국소 flux. 임의 shader rule 인터페이스 대신 제한된 선언 반응과 공통 공급량 제한 | 운반 입자 설정 없이 표면 내 전파를 정의한다. 대신 공중 이동·분리된 물체 사이 이동은 현재 local graph만으로 표현할 수 없다 | 수송 모델 차이는 사실. 더 일반적이거나 구성이 본질적으로 쉽다는 결론은 미입증 |
| Chen 2005 [P2], §3.4·§4, PDF pp.4–6 / pp.1130–1132 | 다중 풍화 상호작용, 속성 갱신, 질량 증감에 따른 변위와 retiling, 변형된 표면에서 다음 γ-ton tracing | MDSSP는 texel graph와 macro footprint를 유지하며 유효 위치·Normal·가중치만 갱신 | 재표본화 없는 간단한 갱신이지만 큰 형상 변형·topology 변화는 처리하지 않는다 | ‘형상 변화가 후속 수송을 바꿈’의 선행 사례. 그 인과관계의 최초성 주장 불가 |
| Lo 2007 [P3], §3–4·§5.5–5.6, PDF pp.3–5/7–8 | UV 타일의 국소 Reaction–Diffusion, metric 보정, active tile 확장, 변형 후 Jacobian/metric 갱신, 이웃 위치·중력 방향을 이용한 물 이동과 흡수 | MDSSP는 State량 기반 outgoing 제한·별도 반응 공급량 경쟁·State 축적에서 유도한 형상 갱신을 동일 Step 계약에 포함 | 실제 변형된 mesh의 metric을 갱신하는 Lo와 달리 MDSSP는 macro 면적 고정. 두 모델의 정확도·표현 범위를 동일시할 수 없다 | 공통 표면 전파·국소 GPU 갱신·geometry-aware 계산 자체는 차별점이 아님. Lo에 MDSSP식 축적 feedback이 없다는 불가능성 증명은 하지 않음 |
| Azencot 2015 [P4], §2–5, PDF pp.3–7 | 곡면 thin film의 4차 모델, 곡률·advection 연산자, sparse 선형 해법과 fluid volume 보존 | MDSSP는 운동량·표면 장력/4차 curvature pressure를 풀지 않는 현상학적 국소 수송이며 다양한 scalar와 반응을 같은 형태로 처리 | 목적·보장 수준이 다르다. 공통 채널에 맞췄다는 이유로 물막 물리 표현력이 더 높아지지 않는다 | 다중 예술적 State는 응용 범위 차이. thin film 수치법 개선으로 주장 불가 |
| Vantzos 2018 [P5], §2–3.7, PDF pp.2–7 | GPU 국소 stencil로 mass·비음수·이산 energy를 다루는 viscous thin film scheme. 외부 potential/relief로 홈 축적 가능. Cartesian domain 한계를 명시 | MDSSP는 UV surface graph·재질별 계수·다중 반응을 사용하지만 같은 thin-film energy를 풀지 않음 | 곡면 적용 영역은 다르지만 곡면 해법 자체는 [P3]/[P4]에도 있다. MDSSP의 비음수 limiter를 동등한 energy 보장으로 취급할 수 없다 | ‘국소 GPU·음수 방지·홈 축적’ 단독 신규성 불가 |
| Su 2023 [P6], §3–4, PDF pp.3–6 | pure water/mixed water/sand/terrain의 층별 높이, 서로 다른 속도, 탄소성·마찰·포화·운동량 교환, splitting | MDSSP는 현상별 momentum/constitutive equation 없이 scalar flux와 공유 형상을 사용 | 서로 다른 현상을 쉽게 근사할 수 있으나 sand/water의 층별 속도·응력·운동량 상호작용을 대체하지 못함 | 다중 재료+변하는 높이의 결합은 기여가 아님. 특정 물리 모델보다 넓은 이름의 State를 지원하는 것이 물리적 일반화는 아님 |
| Jain 2024 [P7], §3–6, PDF pp.3–6 | bedrock/debris/sediment 변화, upstream discharge 기반 erosion/deposition 공통식, depression routing | MDSSP는 고정 graph의 직접 국소 State 교환. 새로운 침식 법칙이나 basin routing을 구현한 것은 아님 | 임의 mesh의 표면 효과와 장기 지형 진화는 다른 요구. 상호 대체 Baseline으로 바로 사용할 수 없다 | coupling 시스템만으로 충분하지 않음을 보여주는 비교. Jain은 구체적인 모델·routing 난제를 별도 해결함 |

## 5. 가장 가까운 Günther·Chen·Lo에 대한 주장 감사

### 5.1 Günther를 ‘독립 효과만 합성하는 시스템’으로 두면 안 된다

[P] Günther §4.3은 주어진 재료 구성값을 변경하는 rule 인터페이스를 허용한다. 제시 예제가 rust/decay/evaporation 세 가지라는 이유로 가능한 규칙도 세 가지라고 제한할 수 없다. §4.2는 계수를 다른 장면에서 재사용한다고 명시한다. §4.5는 재료량으로 높이를 갱신하고 Normal을 만들며 §4.1은 그 material-dependent Normal을 입자 응답에 사용한다.

다만 [U] **새 displacement를 OptiX 충돌 형상에 재구축하는 절차까지 명시적으로 확인한 것은 아니다.** ‘높이가 ray intersection geometry 전체에 매 Step 반영된다’고 확대 해석하지 않는다. 반대로 이를 렌더링 전용 효과라고 축소해서도 안 된다. 확인한 Normal 피드백만으로도 넓은 coupling 최초성 주장은 약해진다. 실제 displaced geometry에서 재추적하는 사례는 Chen에 명시돼 있다.

### 5.2 Ice–Waterfilm–Heat 연쇄는 어디까지 대응되는가?

| 현 실험의 인과관계 | 선행 모델과의 대응 | 남는 차이·불확실성 |
|---|---|---|
| Ice 축적→공유 형상 | Günther의 재료량→높이/Normal, Chen의 양 변화→변위와 개념 대응 [P] | Ice 전용 계수·규칙은 이 논문들의 실증 사례가 아님 [U] |
| Heat가 Ice→Waterfilm 전환을 조절 | 재료 속성에 따른 국소 갱신/rule 인터페이스에 구현할 수 있을 것으로 판단 [I] | 같은 제한·시간 분할·열량 해석을 제공한다는 뜻은 아님. 실제 선행 구현에서 실행하지 않음 [U] |
| 형상→후속 Waterfilm 이동 | Günther의 Normal→입자 응답, Chen의 변형 표면 재추적이 선례 [P] | MDSSP는 local flux로 다른 이산 결과를 만든다. 동일 결과 재현/우위는 미측정 [U] |
| 물의 존재→Heat 감소 | 사용자 정의 속성/rule로 대응 가능하다는 추론 [I] | 원문에 Heat 냉각 데모가 있는 것으로 인용하지 않음 [U] |
| 이동 경로에 Wetness를 남김 | Lo §5.6에 물 이동·stickiness와 흔적, Dorsey의 흡수 모델 [P8]에 선례 [P] | MDSSP `inflowFraction`은 신규 유입 장부를 사용하는 정확히 다른 의미. 이 의미의 편의성·중요성은 미입증 |

이 대응은 같은 수치해가 난다는 증명이 아니다. **해당 인과관계가 기존 풍화 프레임워크로는 원리상 불가능하다는 반례로 쓸 수 없다는 판단**이다.

### 5.3 Lo는 특히 Sparse 주장의 가까운 비교 대상이다

[P] §4는 영향받은 타일 집합과 이웃을 활성화하고 점진적으로 확장한다. §5.5는 변형 영역의 Jacobian/metric을 다시 계산한다. 따라서 Dirty Flag·halo·active tile 확장 자체를 새 기여로 쓸 수 없다.

[U] Lo가 MDSSP의 Reaction 공급량 경쟁·State 유도 형상·모든 Sparse 의존성을 해결했다고 확인한 것은 아니다. 새 연구를 하려면 이 특정 결합에서 기존 의존성 확장으로 부족한 조건을 먼저 제시해야 한다. PERF-041의 Dirty 비교는 작은 차이를 보였을 뿐, 그 필요성을 확인하지 못했다.

## 6. 실제 차이와 Contribution 가능성

아래 순서는 현재 확인된 차이의 구체성과 후속 검증 가능성 기준이다. 강한 신규성 순위가 아니다.

| 후보 | 코드에서 있는 것 | 해결했다고 말할 수 있는 문제 | 강한 심사자 반론 | 현재 평가 |
|---|---|---|---|---|
| 1. 표면 내 전파를 위한 공통 scalar 모델과 공유 형상 결합 | flux/반응/형상 계약 및 선언 확장 [C]; plate에서 coupling 영향 [E] | 여러 현상을 한 계산 형식으로 구성·재사용할 수 있음 | Günther/Chen의 범용 규칙과 Lo의 표면 연산을 조합한 특수화 아닌가? 무엇을 잃고 무엇을 얻었는가? | **시스템 구현의 정체성은 분명, 학술적 진전 미확보.** PERF-042에서 조정 구조에 따른 확장 우위도 지지 안 됨 |
| 2. Input·Incoming·기존 보유량을 구별하는 반응 계약 | positive Input/limited Incoming 장부, `inflowFraction`, 연속 반응의 공급량 경쟁 [C] | 새로 들어온 물만 일부 전환하고 정지한 보유량을 반복 흡수하지 않는 과제를 선언으로 표현 | 익숙한 source/flux 기반 반응과 양 제한의 조합 아닌가? 일반 rule backend도 같은 정보만 주면 가능한가? | **유용한 계약 차이.** 새 반응 알고리즘·표현력 우위를 주장할 근거는 없음 |
| 3. Normal Map 유래 meso 형상에서 다중 축적·후속 수송 | 상대 높이 복원·cavity/적층·동적 간선 갱신 [C] | 높이 asset 없이 meso 상세가 계산 경로에 참여 | 표준 적분+height-field 갱신이다. 적분 오차·scale·chart·mesh sampling에 얼마나 민감한가? | **현재 검증 공백이 큼.** PERF-041/042는 meso=0, cavity 비활성 조건이라 증거로 사용할 수 없음 |

Reaction의 limiter, 두 Pass의 flux 재사용, PCG 복원, Sparse schedule 각각을 별도 신규 알고리즘으로 세지 않는다. 정확한 조합의 식이 다르다는 것만으로 연구적으로 중요한 문제를 해결했다고 판단하지 않는다.

## 7. 모델 일반화가 실제로 포기하는 것

1. **물질량과 물리적 부피의 분리.** Capacity 이상 양은 이동에 남아도 높이 기여는 잘린다. 현상학적 점유 모델로는 의도할 수 있지만 volume-consistent layer라고 부를 수 없다.
2. **하나의 공유 표면.** 여러 State가 같은 유효 형상/간선 구조를 참조한다. 지지면·물 자유면·고체 내부 열전달면이 서로 달라야 하는 현상까지 일반적으로 맞는 모델은 아니다. 어떤 계수를 0으로 두면 영향의 일부는 끌 수 있지만 별도 층별 geometry/metric과 같지는 않다.
3. **층 순서의 부재.** 같은 texel의 State 벡터가 같으면 과거 적층 순서도 같은 데이터로 합쳐진다. ice 위 mud와 mud 위 ice의 다른 노출을 그 벡터만으로 구별할 수 없다. Chen/Günther도 single-layer의 한계를 논의한다. MDSSP가 그 제약을 해결했다고 주장하지 않는다.
4. **고정 graph와 면적.** 큰 변형에서 graph 재구성·재면적화·self-contact/분리 물체 사이 이동을 자동 처리하지 않는다. Bunny의 seam 링크 손실은 실제 재사용 결과의 한계다.
5. **물리적 현상 범위.** Heat는 열 방정식·잠열 모델로 검증한 물리량이 아니고, Waterfilm은 4차 thin-film/운동량 모델의 대체물로 검증되지 않았다.

이 선택들이 프로젝트 목적에 틀렸다는 뜻은 아니다. 오히려 **예술적 제어와 계산 단순성의 대가를 명시하는 것**이 필요하다. 물리 모델로 전환한다고 이 문헌 중복이나 신규성 문제가 자동 해결되지는 않는다.

## 8. 기존 실험이 뒷받침하는 범위

| 증거 | 뒷받침하는 것 | 뒷받침하지 않는 것 |
|---|---|---|
| PERF-041 plate D/E 물막 분포 차이 2.95223%, 경계 Flux·반응 수지로 차이 추적 | 현재 모델의 지속적 형상 갱신이 최종 State에 유의미한 영향을 줌 | 선행연구로 불가능한 효과, 신규성, 외관 품질·제작 생산성 |
| 물 이동이 오히려 줄어든 관측 | 미리 정한 ‘녹으면 물길이 열린다’ 설명 대신 실제 계산을 해석해야 함 | 현상학적 모델의 물리적 타당성 |
| PERF-042: 14개 본 진단의 7 U/M 쌍 수치 배열·Step 장부 일치; Mud/Wetness/합성 선언 확장 | 같은 수치 backend를 허용한 모듈 조정자도 과제를 처리함 | Günther 구현과 동등하다는 증명, 모델 자체의 비신규성 증명 |
| 26회 성공, 감사 오류·pair gate 실패 0 | 지정 범위의 대조 실행 유효성 | 공유 shader의 독립 검증, 사람이 느끼는 편의성, 미실행 44조건의 비용 우위 |

PERF-042의 M은 U와 같은 production flux shader 및 공통 Reaction/Geometry 서비스를 사용한다. 이는 허용한 재사용의 결과이고, 같은 식에 대한 수치 일치는 설계상 예상된다. **‘공정한 비교에서 차이가 없었으므로 모델도 기존 연구와 같다’는 추론은 하지 않는다.** 반대로 선언 확장을 U만의 독점적인 편의성이라고 주장할 수도 없다.

## 9. 연구를 계속할 때의 판정 조건

### 현재 보류할 주장

- 최초의 다중 재료·반응·State-Driven Geometry Coupling 시스템.
- 단일 Solver이기 때문에 합리적인 모듈식 설계보다 확장이 본질적으로 쉬움.
- 물리적 부피/열량을 보존하는 범용 다중 물질 Solver.
- 현재 실험으로 입증된 meso cavity의 일반성, 임의 mesh 재사용성, 성능 우위.

### 현재 사용할 수 있는 기술 서술

> MDSSP는 UV 표면 graph에서 비음수 scalar State의 이동·국소 반응·State 유도 형상 갱신을 공통 계산 계약으로 구성하는 현상학적 시스템이다. 제한된 실험에서 형상 갱신이 후속 수송과 반응의 분포를 바꾸는 것을 확인했다. 이 구조의 선행 프레임워크 대비 학술적 이점은 아직 확인되지 않았다.

이 문장은 구현 설명과 실험 범위를 말하며, 논문의 확정 Contribution 문장이 아니다.

### 후속 작업을 재개할 가치가 있는 조건

| 연구 질문 | 먼저 필요한 증거 | 단순 방법으로 해결되면 |
|---|---|---|
| 공통 모델이 실제 제작 과제에서 유용한가? | 목표 외관/인과 동작을 사전 고정하고, 기존 재사용 rule/표면 연산도 허용한 대안과 조정 실패·수동 작업·품질을 비교. 강한 생산성 주장은 통제된 제작 과제·사용자 증거 필요 | 편의성 우위를 Contribution으로 밀지 않음. 같은-model 아키텍처 대조의 반복으로 대신하지 않음 |
| meso 복원+축적+수송에서 일반적으로 해결할 문제가 있는가? | 실제 normal asset으로 입력 scale·해상도·동일 표면의 UV 변경에 대한 민감도와 후속 Flux 변화 확인. 알려진 height 입력 및 표준 복원/국소 갱신을 Baseline으로 사용 | 표준 적분·모델 정리로 해결되면 기술 개선으로 기록하고 새 알고리즘 주장은 중단 |
| Reaction과 형상 변화가 기존 Sparse 갱신을 어렵게 만드는가? | Dense/기존 dirty+충분한 halo 대비 누락과 오차–비용 문제가 실제로 중요하다는 반례. 같은 dependency를 공유한 단순 방법 포함 | 일반적인 dirty/halo로 충분하면 새로운 Sparse 기여를 가정하지 않음 |

새 구현을 바로 추가할 근거는 아직 없다. 지금의 결과로는 **시스템 Contribution을 확정하는 작업을 중단하고, 프로젝트 목적에 맞는 제작 가치 검증 또는 실제 실패에서 출발한 한 가지 문제 검증을 별도 질문으로 선택**하는 것이 타당하다. 현재 진행해 온 단일 Solver 확장 부담 가설은 v1 과제에서 지지되지 않았음을 유지한다.

## 10. 원문 출처·접근 수준

| ID | 출처 | 이번에 확인한 범위 |
|---|---|---|
| P1 | Günther, Rohmer, Grosch. 2012. [GPU-accelerated Interactive Material Aging](https://www.vc.tf.uni-erlangen.de/publications/Guenther12VMV/Guenther12VMV.pdf). [DOI](https://doi.org/10.2312/PE/VMV/VMV12/063-070) | 8p author preprint, §3–4.5·§6–8. PDF p.3/6 시각 확인. 후속 ray collision geometry 재구축은 미확인 |
| P2 | Chen et al. 2005. [Visual Simulation of Weathering by γ-ton Tracing](https://courses.cs.umbc.edu/graduate/CMSC691B/spring06/papers/p1127-chen.pdf). [DOI](https://doi.org/10.1145/1073204.1073321) | 7p 원문, §3.4·§4 및 소개. PDF p.6 ‘Geometry Changes’와 retiling/다음 추적 문단 시각 확인 |
| P3 | Lo et al. 2007. [Interactive Reaction-Diffusion on Surface Tiles](https://www.cse.cuhk.edu.hk/~cwfu/papers/gpurd/paper/gpurd.pdf). [저자 프로젝트](https://www.cse.cuhk.edu.hk/~cwfu/papers/gpurd/index.html) | 10p 원문, §3–4·§5.5–5.6·§6. PDF p.8 시각 확인 |
| P4 | Azencot et al. 2015. [Functional Thin Films on Surfaces](https://mirelabc.github.io/publications/func_thin_films.pdf). [출판 기록](https://diglib.eg.org/items/3915edcb-f780-4a57-95b1-bdac2916f77c) | 10p SCA 원문, 소개·§2–5의 관련 모델/보장. 동명 2017 TVCG 확장판과 혼동하지 않음 |
| P5 | Vantzos, Raz, Ben-Chen. 2018. [Real-time Viscous Thin Films](https://mirelabc.github.io/publications/rtvtf.pdf). [DOI](https://doi.org/10.1145/3272127.3275086) | 10p author PDF, §2–3.7. §3.3 relief potential 및 §3.7 Cartesian/local propagation 한계 |
| P6 | Su et al. 2023. [Real-time Height-field Simulation of Sand and Water Mixtures](https://kuiwuchn.github.io/RTWaterAndSand.pdf). [저자 프로젝트](https://kuiwuchn.github.io/rtsw.html) | 13p author PDF, §3–4, 층·속도·상호작용·splitting |
| P7 | Jain, Benes, Cordonnier. 2024. [Efficient Debris-flow Simulation for Steep Terrain Erosion](https://aryamaanjain.github.io/pdf/2024/debrisflow.pdf). [저자 기관 페이지](https://www-sop.inria.fr/reves/Basilic/2024/JBC24/) | 11p 원문, §3–6, layer 수지·통합 erosion 식·depression routing |
| P8 | Dorsey, Pedersen, Hanrahan. 1996. [Flow and Changes in Appearance](https://graphics.cs.yale.edu/publications/flow-and-changes-appearance) | **저자 기관의 초록 수준**. 현상학적 surface flow, 흡수·용해·침전 결합의 선례로만 사용. 세부 이산 수치법은 비교하지 않음 |

추가 검색에서 Mei et al. 2007 *Fast Hydraulic Erosion Simulation and Visualization on GPU*의 virtual-pipe/outflow limiter가 매우 가까운 수치 비교 후보로 확인됐다. 그러나 저자 PDF 직접 접근은 timeout, HAL은 bot 확인 HTML을 반환해 관련 식의 완전한 페이지 검토를 이번 범위에서 완료하지 못했다. 세부 식 동일성의 증거로 사용하지 않았다. [저자 PDF 위치](https://www-evasion.imag.fr/Publications/2007/MDH07/FastErosion_PG07.pdf), [DOI](https://doi.org/10.1109/PG.2007.15). Iber 2024도 publisher 검색 내용까지만 확인했으며 이 보고서의 상세 비교 근거로 삼지 않았다. 추가 검색 결과를 모두 원문 검증했다고 부르지 않는다.

확보한 P1–P7의 로컬 열람 사본·추출 텍스트·URL/SHA-256 목록은 Git 제외 `Build/research/prior-model-review/`에 있다. 논문 원문을 저장소 산출물로 재배포하지 않는다. 아래 hash는 이번에 사용한 PDF 버전을 식별한다.

| ID | SHA-256 |
|---|---|
| P1 | `a0a2a7afe1833332f7c5ced1f4a452a5fee3bd4d894dd90f1b0359820b6d2865` |
| P2 | `af7daa76cd8a4b989fd437eaf7a6bf74dcd46a1c6b92f0f2acd6f763dd642b96` |
| P3 | `d65474fab426444d2b2d5ecea965f261c6af3f0a61c6adb35a19d722045c144f` |
| P4 | `3a3e07e11e006e9eb57e9b92100f64dbeab0aae7762565b9227e740551bfd027` |
| P5 | `3eb8ed1a003c8d3ab58c446d60cec849fffddd69b9adb24c13ce266f0e73b152` |
| P6 | `7165b5d986c65923b99bfce409d19e4827947243c964ee49b951437c734f88b6` |
| P7 | `1d5c9057cc7d6912a114e60b9d994082ec700eb9ebeb24bed1248b4afd23737c` |

관련: [[04_Development/04_System-Contribution-Protocol|H1–H4 명세]], [[04_Development/05_Modular-Baseline-Spec|Baseline v1]], [[05_Experiments/02_Performance-Measurements|PERF-041/042]].

## 11. 후속 실험 연결 — 2026-10-27

위 본문의 ‘새 실험 없음’은 문헌 대조 작성 시점의 범위다. 이후 사용자 후속 승인으로 [[05_Experiments/02_Performance-Measurements|PERF-043]]의 Meso 실험을 별도로 수행했다. 35회·GPU 5,580 Step에서 합성 높이 복원 오차는 같은 제약의 표준 LS로 감소했지만, brick 128의 물막 차이는 공급량 대비 0.00062779%로 같은 조건의 60/120 Hz 민감도 0.00243000%보다 작았다. 한 평면·한 실제 map·4초 조건에서 새로운 복원/수송 알고리즘 필요성은 확인하지 못했다. 원문의 H4 판정을 강화된 Contribution으로 수정할 근거가 아니다.

연구 실행 감사 통과와 별도로 현행 render-mesh unit test 실패 1개, 최초 33회의 production SPIR-V 사전 보관 누락 및 후속 대표 2조건의 재현을 명시했다. 정확한 범위·도표·원자료는 PERF-043을 따른다.

## 12. 제작 연구의 실행 가능성 후속 점검 — 2026-10-27

[[04_Development/07_Authoring-Study-Protocol|제작 과제 명세]]를 준비하고 실제 Profile loader/registry/기본 외관 binding 5건을 검사했다. `wetness`의 수치 등록과 `absorbedwetness`에 연결된 기본 외관은 구분된다. 현 M은 U와 같은 Profile workflow를 쓰는 headless 조정자여서 backend 선택만으로 인간 제작 시간의 차이를 식별할 수 없다. 현재 공정한 비교 연구는 No-Go이며 단일 시스템 pilot 과제 3개만 준비했다.

앱 화면 확인은 첫 조건이 Windows Application Control 오류 4551로 실행 전에 차단돼 완료하지 못했다. 인간 session·화면·제작 시간 결과는 없다. 이 readiness 결과를 시스템 기여의 새 증거로 해석하거나 현재 판정을 상향하지 않는다. 소스 근거·과제·차단 기록과 재개 조건은 위 문서를 따른다.

이후 사용자의 Smart App Control 해제·재시도 지시에 따라 같은 exe로 Geometry 테스트가 통과했고 화면·State 6조건을 관측했다. 그러나 모든 앱 조건에서 descriptor layout 수명 관련 Vulkan 검증 오류가 있어 정상 reference 결과로 채택하지 않는다. 이전 차단 기록은 역사적 관측으로 유지하고 현재 판정·소스상 원인·잠정 결과는 위 제작 명세 §7과 검증 기록을 따른다. 기여 판정은 상향하지 않는다.

추가 후속 승인으로 위 수명 오류를 수정했다. 기존 descriptor set/layout을 유지하는 재연결 방식으로 12회 전환 회귀·기존 GPU 테스트·실제 6조건/2,172 Step 검증이 통과했고 State 배열 18개는 앞선 관측과 비트 일치했다. 최신 정상 실행은 제작 명세 **§8**과 검증 기록을 따른다. 일반적인 Vulkan 소유권 오류를 바로잡은 것이며 기존 모델 대비 연구 기여나 인간 제작 생산성의 새 증거는 아니다.
