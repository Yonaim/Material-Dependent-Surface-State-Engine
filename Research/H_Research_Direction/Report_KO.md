# MDSSP 연구 방향 전환 보고서

2026-11-23 · 문헌, 수학적 통제 실험, 반증 계획

## 결론

**시간 적분기 트랙은 종료한다.** 검증한 1D 연결 모델에서 기존 DOP853이 실측 정확도·CPU 기준을 충족했다. 실장면 적용과 온라인 전역 상한 비용은 Hold이며, 입력 로그·기존 적분·보존 경계 장부는 엔진 기능 또는 검증 자산으로 재사용할 수 있다. [F/G 종료 기록](Temporal_Closeout_KO.md)

**첫 실험은 B의 반응형 재질에서 한다.** 한 번의 젖음→건조 영상으로 맞춘 모델들이 미관측 닦기→재젖음에서, 촬영 반복 오차를 넘는 다른 결과를 내는지 먼저 확인한다. 그 차이가 나타나면 같은 시간으로 캡처한 짧은 개입 하나가 예측 문제를 해소하는지 본다. 가장 강한 두 저장량 물리 모델이나 이력 LUT가 이를 이미 해결하면 신규 알고리즘 경로도 종료한다. **A는 예비 후보**로 남긴다. 저자가 명시한 속도·패턴 충돌이 구체적이지만, 기존 박막 수치법과의 공정 비교 비용이 더 크다. B의 GO는 최소 실험에 대한 승인 판단이며, 아래 어떤 후보에서도 신규 알고리즘 필요성이 입증되지는 않았다.

## A. 표면 액체의 시각 특성을 유지하는 비용 절감

**원문에서 확인한 사실.** Herson, Paris, Michel의 *Dripping Thin Films for Real-time Digital Painting*은 얇고 느린 층류와 매끄러운 평면 캔버스를 대상으로 2D 박막식을 Thickness·Fluidity·Hydrophoby로 재매개변수화한다. §4–5에서 dry cell로 흐를 수 있는 면 mobility와 보존 flux 제한을 사용한다. §5.2/Fig. 8은 공간 격자가 손가락보다 굵거나 시간 간격에 비해 유체가 너무 빠를 때 패턴 편향이 나타남을 보고한다. 저자 방식이 Vantzos 계열 규제보다 fingering 구간을 넓혔고, 작은 시간 간격과의 wet-area symmetric difference/union도 이미 사용했다. §7에서 일부 아티스트가 \(F_{\max}\)보다 더 빨리 손가락이 생기길 원했고, 저자는 채택한 명시적 방식의 안정화와 패턴 손실을 그 원인으로 지목했다. 또한 선형 pigment 혼합, 장애물 앞 축적 및 coffee ring을 각각 한계·후속으로 언급한다. 어느 한계도 정확한 implicit 또는 IMEX 방식 전반의 한계로 입증한 것은 아니다. [Herson 등 원문, §4–5·§7](https://eliemichel.github.io/dripping-thin-films/documents/herson26dripping_thin_films.pdf)

**후속 확인.** 2026-11-23 기준 OpenAlex DOI 인용 조회에서 찾은 Price의 *From art to physics*는 경사판 물감의 실험과 패턴 간격, 교육용 반응–확산 유비를 다룬다. 저자의 기관 기록과 초록을 확인했고, 게시된 교정본은 접근 제한이었다. 공개 범위에서 Herson의 속도 제한을 푸는 동일 PDE·예산 비교는 확인되지 않았다. 인용 조회와 제목·저자·fingering/implicit 검색은 누락될 수 있다. Herson의 저자 사이트에 논문과 공개 fluid demo는 있으나, 이 조사에서 별도의 재현용 소스 및 실행 절차는 확인하지 못했다. [Price 기관 기록](https://eprints.worc.ac.uk/16682/) · [Herson 등 프로젝트](https://eliemichel.github.io/dripping-thin-films/)

**강한 반론과 비교군.**

- Vantzos, Raz, Ben-Chen의 *Real-time Viscous Thin Films*는 실시간 박막 흐름의 직접 선례다. Vantzos 등의 *Functional Thin Films on Surfaces*는 곡면 메시에서 부피 보존 희소 선형 풀이, 물방울 상호작용·fingering을 보여 주고 양수성 보장이 한계라고 명시한다. 곡면으로 옮기는 것만으로 차별화하기 어렵다. Stomakhin 등의 FLIP/APIC 접촉각·부착·피부에서 떨어지는 물방울은 근접 촬영 기준으로 유용하나 게임 예산 검증과는 범위가 다르다. [실시간 박막](https://cris.technion.ac.il/en/publications/real-time-viscous-thin-films/) · [곡면 박막 원문](https://ddg.num.math.uni-goettingen.de/pdf/ftf_tvcg.pdf) · [Stomakhin 등](https://alexey.stomakhin.com/research/drips.html)
- 강직성이 핵심이면 기존 sparse implicit, 양수성 보존 박막 방식, 적응형 지수 적분과 IMEX가 강한 반론이다. Beltrame–Thiele는 Jacobian/Krylov 기반 2D lubrication과 적응형 간격을, Orizaga–Witelski는 가변 mobility 박막식의 상수계수 IMEX 분할 및 2차 정확도 사례를 다룬다. 각각 원문에서 확인했다. Herson의 기울어진 fingering 식·dry-cell 조건·지각적 출력·동일 장치 비용까지 직접 비교한 결과로 확대할 수 없다. [양수성 보존 박막식](https://doi.org/10.1137/S0036142998335698) · [적응 지수 전파](https://arxiv.org/html/0903.0014v3) · [박막 IMEX](https://arxiv.org/html/2405.19483)
- 목표가 순수한 속도 연출이면 타임스케일, 안정 substep, 사전 계산 재생, 사례 기반 POD 등이 먼저다. 모델 축소와 고해상도 유체 detail 합성에는 선례가 있다. 새 입력이 없는 재생과 연속 상호작용 모델의 비용·품질은 각각 측정해야 한다. [실시간 유체 모델 축소](https://grail.cs.washington.edu/projects/model-reduction/) · [고해상도 유체 표면](https://cs.uwaterloo.ca/~c2batty/papers/Goldade2016/)

**남는 가설 및 판정 — HOLD.** 동일 물리식과 허용 접촉선·질량 오차를 유지하면서, 가속과 신규 상호작용을 동시에 요구할 때 기존 방법의 속도–폭·간격·연결성 Pareto가 무너지는지 확인할 가치가 있다. 고해상도 측정 없이 보이는 디테일 차이는 표현상의 차이일 수 있다. 기준이 수렴하지 않거나 격자가 손가락을 해상하지 못하면 적분기 자체의 실패로 분류할 수 없다. 평면 한 장의 CPU 비교는 가능하나, 비교군 재현과 설득력 있는 GPU 게임 예산 검증은 B보다 자원 소요가 크다.

## B. 적은 예시로 만드는 입력 반응형 재질

**확인된 선행연구와 적용 범위.**

| 방법 | 원문에서 확인한 범위와 한계 |
|---|---|
| **모델 기반 젖음과 건조** | Dorsey 등의 물 입자·흡수·침착 연성은 표면의 지속 흔적 선례다. Jensen 등의 표면 수막과 내부 산란의 조합은 외관 변화를 두 성분으로 설명하는 직접 반론이다. Lu 등은 건조 영상의 이력을 두 파라미터로 줄여 합성 형상에 전이했다. Gu 등의 공개 CAVE 자료에는 물감·젖은 시멘트/석고/직물 등의 시간별 BRDF와 적합값이 있고, 논문의 장치는 각 시간 표본을 36초 안에 획득했다. 이는 dry-down 파라미터/LUT의 좋은 기준이며 닦기 플래그와 반복 순서가 주석된 데이터로 확인한 것은 아니다. [Dorsey 등](https://graphics.stanford.edu/papers/flow/) · [Jensen 등](https://doi.org/10.2312/EGWR/EGWR99/273-282) · [Lu 등](https://graphics.cs.yale.edu/sites/default/files/drying_materials.pdf) · [Gu 등 CAVE](https://www1.cs.columbia.edu/CAVE/databases/tvbrdf/about.php) |
| **Neural Differential Appearance Equations** | 예시 RGB 또는 flash-lit 영상에서 시간에 따라 변하는 공간 정상 통계와 재조명 가능한 svBRDF를 학습한다. §6.8은 국소적으로 진행되는 patina를 공간 균일한 통계로 바꾸거나 구조를 놓치는 실패를 다룬다. 본문과 보충자료에 시간 희소 표본(5프레임)과 관련 외관으로의 dynamics transfer가 이미 있다. 닦기·재젖음의 라벨 입력 순서 일반화는 확인한 실험 범위에 없다. 저자 JAX 코드·데이터·가중치 링크를 확인했고 실행은 하지 않았다. 인용으로 확인한 같은 저자의 *Generative Video Bi-flow*는 무조건부 스트리밍 영상 생성이며 이 제어 문제를 해결하지 않는다. [원문과 부록](https://arxiv.org/html/2410.07128v2) · [저자 코드](https://github.com/ryushinn/ode-appearance) · [Generative Video Bi-flow](https://arxiv.org/abs/2503.06364) |
| **MeshNCA 및 다른 NCA** | MeshNCA는 새 메시·분할 수준에 일반화하고, orientation·grafting·regeneration·motion을 조작하며, 여러 재질 맵을 생성한다. §7에서는 채널 간 어긋남, 고르고 조밀한 정점 요구, 렌더 해상도·카메라 민감도와 의미적 구조 실패를 명시한다. 따라서 인상적인 실시간 브러시 데모는 강한 비교군이며 물의 실제 이력 검증 범위는 별도다. DyNCA는 무한 길이와 크기의 실시간 동적 RGB 텍스처를 생성한다. 신호 반응형 NCA는 한 모델에서 학습 텍스처 간 보간·grafting까지 보여, 조건을 입력으로 넣는 발상도 선례가 있다. [MeshNCA §7과 코드](https://meshnca.github.io/) · [MeshNCA 원문](https://infoscience.epfl.ch/bitstreams/07fa1b30-9248-4b6b-88ce-2b2b099e625d/download) · [DyNCA](https://openaccess.thecvf.com/content/CVPR2023/papers/Pajouheshgar_DyNCA_Real-Time_Dynamic_Texture_Synthesis_Using_Neural_Cellular_Automata_CVPR_2023_paper.pdf) · [신호 반응형 NCA](https://doi.org/10.1038/s41598-025-23997-7) |
| **후속·독립 경쟁 방법** | SPH-NCA 포스터는 임의 표본점·곡면으로의 텍스처 일반화, *From Cells to Pixels*는 NCA 결과의 통합 렌더링까지 다룬다. 2026년 10월 *DynaMesh* 프리프린트는 영상으로부터 고정 메시의 시간별 PBR 속성을 생성하고 새 형상 전이를 보고했다. 저자는 영상마다 적응이 필요하고 형상이 고정되는 한계를 명시한다. 단일 시점에 보이지 않는 전역 패턴의 실패도 제시한다. 이 결과들은 신규 메시·시간별 PBR 그 자체의 신규성 주장을 약화한다. [SPH-NCA](https://doi.org/10.1145/3721250.3742998) · [From Cells to Pixels](https://cells2pixels.github.io/) · [DynaMesh §4–5](https://arxiv.org/html/2610.05529) |

한 시점의 공간 차이로 풍화의 시간 순서를 복원하는 문헌도 있다. 따라서 적은 예시로 시간에 따른 외관을 얻는 것 자체보다 관측하지 않은 행동 순서에 같은 재질이 어떻게 반응하는지를 별도로 검증해야 한다. [Appearance Manifolds](https://www.microsoft.com/en-us/research/publication/appearance-manifolds-for-modeling-time-variant-appearance-of-materials/)

**가장 강한 반론.** 독립적인 누적 건조와 표면 수막 제거를 가진 2상태 모델, action-conditioned LUT 또는 작은 state machine으로 목표를 만족할 수 있다. 기존 capture·fit 모델을 포함해 비교한다. 학습법에도 동일한 입력 라벨과 촬영 예산을 줘야 한다. 기존 NDE/NCA 데모의 그대로의 재현은 외관 비교로 보고, 같은 작업의 비교에는 각 방식에 시각·dose·wipe 마스크를 입력으로 갖는 조건부 버전 또는 사전에 공개된 액션 추정기를 쓴다. 원본 학습법이 제공받지 못한 입력을 실행할 수 없다는 사실만으로 성능 패배로 집계하지 않는다. 데이터가 아무리 적어도 일반화된다는 주장은 물리 상태와 카메라 관측의 비식별성에 막힐 수 있다.

**남는 가설 및 판정 — GO, 반증 실험 한정.** 동일한 passive 캡처에 부합하는 상태들이 실제 입력에서 눈에 띄게 갈라지는 경우, **한 번의 짧은 개입 또는 다른 조명 한 컷을 무엇으로 선택해야 같은 제작 비용에서 미관측 행동 순서의 예측 오차를 줄이는가**가 후보 문제다. 일반 시스템 식별의 input design과 data informativity 자체에는 선례가 있다. 그래픽스 기여를 주장하려면 조명에 따른 구분 가능성, 매체 응답, 제한된 아티스트 비용을 결합했을 때의 검증 가능한 이득이 있어야 한다. B는 시편 두 종류와 간단한 모델로 반증할 수 있어 우선순위가 높다. 공개 자료가 검증해 둔 것은 단일 경로의 외관과 relighting이다. 닦기 순서의 실물 ground truth는 별도로 캡처할 필요가 있다. [온라인 실험 설계 선례](https://arxiv.org/abs/2102.11193) · [Data informativity](https://arxiv.org/abs/1908.00468)

## C. 품질을 유지하는 해상도·갱신률 조정

**제품과 논문의 범위.** Unreal Niagara Fluids는 해상도, 고정 dt·시간 배율, 반복 횟수, 품질별 override를 제공한다. Effect Type은 인스턴스 culling, spawn·renderer 스케일링 및 설정 재사용을 지원한다. Unity VFX Graph의 명시된 버전에서는 Fixed Delta Time update가 있고, Unity의 고정 계산은 렌더 프레임과 독립적인 호출 주기를 갖는다. Unity용 상용 Fluid Frenzy는 2.5D surface fluid, wetness mask·감쇠, flow mapping, 별도 mask 해상도, 고정 60 Hz solver와 화면 밖 culling을 문서화한다. culling 상태에서도 이웃 동기화나 선택 시각 레이어가 갱신될 수 있다. 제품 기능은 공식 문서에서 확인했으며 장애물·사용자 입력을 바꾼 다중 예산 품질을 직접 실행하지 않았다. [Niagara Fluids](https://dev.epicgames.com/documentation/en-us/unreal-engine/niagara-fluids-reference-in-unreal-engine) · [Niagara 예산 관리](https://dev.epicgames.com/documentation/en-us/unreal-engine/performance-budgeting-using-effect-types-in-niagara-for-unreal-engine) · [Unity VFX Graph Update 10.8](https://docs.unity3d.com/Packages/com.unity.visualeffectgraph@10.8/manual/Context-Update.html) · [Fluid Frenzy 기능](https://frenzybyte.github.io/fluidfrenzy/docs/fluid_simulation_components/) · [Fluid Frenzy 시간 처리](https://frenzybyte.github.io/fluidfrenzy/docs/setup/)

자동 shader 단순화와 품질–성능 최적화, 저해상도 물리와 고해상도 표면을 결합하는 dual resolution 연구가 있다. Goldade–Batty–Wojtan은 해상도 불일치가 만드는 artifacts와 개선법을 직접 다뤘다. MeshNCA 및 SPH-NCA는 텍스처의 공간 표본 변화에 대한 비교군이다. 다른 조명·갱신률에서의 시간별 PBR 응답의 위치와 사용자 조작 결과까지 보장하는 제품 기능은 확인한 문서의 범위에 없다. [Shader 자동 단순화](https://web.eecs.umich.edu/~weimerw/p/sitthiamorn_siga11.pdf) · [이중 해상도 유체](https://cs.uwaterloo.ca/~c2batty/papers/Goldade2016/) · [시공간 지각 지표 FovVideoVDP](https://github.com/gfxdisp/FovVideoVDP)

**구체적 문제·반증·판정 — HOLD.** 동일 접촉 입력에서 coarse solver로 옮겼을 때, artist retuning과 단순 filtering, mesh의 projected footprint, 보존 remap, 출력 보간 및 고해상도 외관 detail의 조합으로 가시 반응과 접촉 시각을 지킬 수 있는지 먼저 본다. 평균 두께만으로는 서브셀 유량을 복원할 수 없는 최소 반례를 실제로 계산했다. 그러나 한정된 장면의 지각 임계값 안에서 기존 서브셀 통계 하나 또는 장면별 LUT가 충분할 가능성이 높다. 최소 실험은 한 개 좁은 물길과 dry 주변, 두 simulation 해상도 × 두 renderer 해상도 × 두 갱신률, 표면 이동 조작 하나다. 동일 입력으로 world-space 전면 도달 시각·흔적 support·재조명 오류, GPU·CPU p95, 제작자 활동 분을 잰다. 고품질 baseline의 저비용 설정이 별도 수작업 없이 동등하거나, 저해상도가 유실한 구조가 고해상도 보조 채널로 복구되면 연구를 멈춘다. F/G는 비교 프레임·입력 기록에 재사용 가능하다. 실성능과 지각 비용 측정에는 A/B보다 많은 기기·아티스트 검증이 필요하다.

## D. 별도 탐색 — 국소 흐름 편집과 다른 장면으로의 재사용

**문헌의 출발점.** *Detail-Preserving Fluid Control*은 디테일을 살리는 유체 제어의 선례다. *Hierarchical Differentiable Fluid Simulation*은 저해상도에서 큰 힘을 먼저 최적화한 후 제한된 서브도메인을 세분화하며 메모리와 실행 시간을 절충했다. Chen 등의 *Fluid Control with Localized Spacetime Windows*는 물 튀김과 경로의 국소 시공간 제어, 주변 흐름 유지, 힘 템플릿 재사용의 초기 데모를 포함한다. 원문의 미래 과제에는 겹치는 창, 순차 편집이 후속 창에 미치는 영향, 움직이는 물체, 국소 힘 템플릿의 광범위한 전이가 있다. OpenAlex에서 해당 DOI의 인용 후속이 조회되지 않았으나 누락 가능성이 있다. [Detail-Preserving Fluid Control](https://www.sciencedirect.com/science/article/abs/pii/S1524070309000022) · [계층적 역제어](https://doi.org/10.1111/cgf.70226) · [국소 시공간 창 원문 §6](https://arxiv.org/html/2511.15189)

**구체적 문제·반증·판정 — NO-GO, 이번 범위.** 흥미로운 좁은 가설은 같은 물길 목표를 경사가 다른 표면에 전이하면서 편집 대상 바깥의 기존 wet footprint와 배수량을 보호하는 것이다. 단순히 제어 가능한 액체 또는 국소 편집 UI를 만드는 기여는 이미 중복된다. 한 개의 Y자 물길과 다른 경사의 Y자 물길에서 hand-painted force/flow map, 좌표를 재정렬한 force template, 목표마다 다시 최적화하는 국소 역제어를 같은 허용 전송량으로 비교할 수 있다. 목표 구간의 도달 오차, 바깥 영역의 건조/보호 위반, 작가 수정 분, 반복 최적화 시간과 메모리를 함께 잰다. 힘 템플릿 또는 재최적화 비용이 가벼우면 멈춘다. 움직이는 장애물·곡면·보존 제약을 한꺼번에 넣으면 실패 원인을 분리하기 어렵다. 좁은 문제도 미분 가능한 물리와 편집 데이터가 먼저 필요하여 지금 B보다 투자 대비 근거가 약하다.

## 비교와 우선순위

| 후보 | Novelty 가능 범위 | Feasibility | 결정적 검증 / 가장 강한 반론 | Risk / 판정 |
|---|---|---|---|---|
| B | 제한된 개입·조명에서 미관측 행동의 시각 예측을 가르는 재질 관측 설계 | 높음. 시편 2개로 시작. CAVE·NDE는 passive 비교에 재사용, 엔진 통합 불필요 | 과거에 본 프레임을 맞추는 모델들이 held-out 반응에서도 다른가. 기존 2상태 모델과 단순 실험 설계로 이미 충분한가 | 획득 잡음, wipe 재현성, 식별 가능성의 과대 일반화. **GO — 실험** |
| A | 입력 이후 궤적의 시각 특징을 유지하는 가속의 Pareto에서 남는 한계 | 중간. 평면 참조부터 가능. 박막 capillary·접촉선은 F/G로 대체하기 어려움 | 단순 재생/시간 재매개화, adaptive explicit, sparse implicit·IMEX, 동일 격자 기준이 이미 해결하는가 | 원본 실행 재현, 공간 alias, 물리 범위와 속도 연출의 혼동. **HOLD** |
| C | 고정된 품질 기준과 제작 예산으로 저사양 계층의 입력·상태·외관을 함께 비교 | 중간. 기존 프리셋으로 벤치 가능하나 실제 하드웨어·시각 판정 필요 | 수동/자동 retuning과 decoupled rendering이 같은 화질과 반응을 낼 수 있는가 | 품질 지표 약함, 기존 기능 조합 성공 가능성이 높음. **HOLD** |
| D | 보호 영역과 담보 조건을 가진 국소 편집 전이 | 낮음. 기성 force edit 자료를 쓰더라도 비교군 구현·검증 비용 큼 | 단순 force map·template·per-scene 역제어가 비용 목표에 들어오는가 | 가까운 연구 중복과 스코프 확대 위험이 높음. **NO-GO** |

엔진의 공통 State/경계 장부는 입력을 고정하고 여러 출력을 비교하기에 유용하다. 그러나 PERF-042의 결과가 구조적 확장 부담 우위를 입증하지 못했고 기존 작업에서 제작자 비교 시간이 측정되지 않았다. “하나의 엔진 구조에 통합”하는 것만으로 새 contribution을 만들기보다, 먼저 B를 엔진 밖에서 검증하는 편이 물리·캡처·제작 비용을 분리할 수 있다. 결과가 좋아도 상태 종류나 재질 채널을 한 번에 확장할 근거가 되지는 않는다. [엔진 실험 기록](../../Docs/Agent-Vault/05_Experiments/Records/Performance-History-Through-2026-11-08.md)

## 수행한 최소 계산

- **B 합성 통제.** 표면막 \(f\)와 흡수량 \(p\)가 같은 감쇠율을 갖고 \(f\to p\)인 두 모델을 계산했다. 41개 passive 표본의 총량 차이는 최대 \(1.11\times10^{-16}\)였다. \(t=2\)에 표면막의 80%를 닦고 \(t=3\)에 질량 0.25를 다시 넣으면 \(t=4\)의 합은 **0.40453 또는 0.70457**이 된다. 수동 관측만 보고 한 값을 내는 예측기의 최악 절대오차 하한은 **0.15002**다. 합성 모형에서 wipe 효율이 알려져 있으면 전후 한 쌍으로 두 흡수율을 구분했다. 실제 카메라 RGB나 실제 재질의 오차 측정값으로 사용할 수 없다.
- **C 서브셀 통제.** 두 fine profile \((1,1)\)과 \((0,2)\)의 평균 두께는 모두 1이다. 단위 \(k\)의 cubic flux를 평균하면 각각 1과 4다. coarse 평균 하나만 입력으로 받는 공통 예측은 두 경우에 최악 절대오차 1.5 이상을 갖는다. 이론적 정보 손실의 예이며 지각 차이·실제 flux 오차 크기를 측정하지 않았다.

[계산 코드](Evidence/check_counterexamples.py) · [실행 결과](Evidence/Counterexamples.json)

## 다음에 실행할 한 가지

**B의 블라인드 wet–wipe–rewet 시편 실험 하나를 실행한다.** 매끈한 비흡수 시편과 무코팅 흡수 시편에서 건조 예시를 맞춘 후, 동일 비용의 추가 정보 세 조건을 비교한다. 수동 구간을 더 촬영하는 조건, 고정 시각의 짧은 wipe를 촬영하는 조건, 표면 반사를 분리하는 한 번의 grazing 관측이다. 순서를 보지 못한 테스트에서 같은 LUT와 두 저장량 모델부터 맞붙인다. 반복 오차를 넘는 차이와 문턱을 미리 정한다. 이 모형들로 충분하면 중단하며, 차이가 반복될 때만 조건부 NDE/NCA를 붙여 검증한다. 어떤 정보를 추가할지의 일반적인 선택 알고리즘은 이 시험에서 개발하지 않는다. [수학적 문제 정의와 실행 프로토콜](Falsification_Protocol_KO.md) · [원문 조회 기록](Source_Audit_KO.md)
