# 관련 문헌과 열람 수준

선행연구의 직접 확인 범위와 신규성에 대한 제한을 보존했다. 이 장부는 분야 전체를 망라했다는 보증이 아니다.

## 먼저 볼 결론

- 범용 표면 노화, 재료별 규칙, 반응 수송, 동적 형상, 적응 계산에는 폭넓은 선례가 있다. MDSSP의 기능 목록만으로 최초성이나 시스템적 우위를 주장할 수 없다.
- 이 장부에는 원문 직접 확인, 초록·출판사 요약 확인, 조건부 추론이 섞여 있다. 각 항목의 열람 수준을 확인하고 논문 주장에는 원문 확인 범위만 사용한다.

## 이 문서의 원본 절

- [[90_Research-Archive/Research-Notes-2026-10/04_Literature/01_Related-Work#source-reference-07-related-work|90_Reference/07_Related-Work.md]]

원본 25개 파일의 수정 전 사본과 SHA-256 목록은 `Original-25-Notes.zip`에 있다. 아래 절은 원문의 수식과 당시 판단을 보존하며, 링크와 제목 수준만 새 위치에 맞췄다.

---

## source-reference-07-related-work

원본: `90_Reference/07_Related-Work.md`

### 후속 연구 문헌과 확인 수준



마지막 갱신: 2026-10-28. [[04_Development/08_Research-Directions-123|최초 통합 검토]]에서 분리한 후속 연구 정본이다. 이 파일의 내용은 제안·수식 분석이며 새 구현/실험 결과가 아니다. 로컬 앱·GPU·빌드 실행 중지를 유지한다.



공통 근거는 [[90_Research-Archive/Research-Notes-2026-10/03_Solver/01_Forward-and-Reaction-Semantics#source-solver-10-forward-model-and-controls|forward 모델]], 이후 연구는 [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-reference-09-analytical-findings|분석 노트]]를 따른다. 아래 접근 수준은 각각 구분하며 원문 전체를 읽었다는 뜻이 아니다.





| ID | 1차 출처 | 이번에 확인한 내용·제한 | 연구 설계에 주는 영향 |

|---|---|---|---|

| R1 | Bosch et al. 2011, [Image-Guided Weathering](https://graphics.cs.yale.edu/publications/image-guided-weathering-new-approach-applied-flow-phenomena), [author PDF](https://graphics.cs.yale.edu/sites/default/files/a20-bosch.pdf) | 저자 기관 소개와 검색 색인에 노출된 원문 §3–4 일부. PDF 직접 열기는 크기 제한으로 실패 | 파라미터·detail map 추출과 새 target 적용은 선례. 다중 State 보호/후속 반응 보장은 확인 못 함 |

| R2 | McNamara et al. 2004, [Fluid Control Using the Adjoint Method](https://grail.cs.washington.edu/wp-content/uploads/2015/08/mcnamara-2004-fcu.pdf) | 저자 기관 PDF 검색 색인의 §6 adjoint 연산 설명. 직접 열기는 timeout | forward 연산을 역순 adjoint로 미분해 keyframe control하는 접근은 기존 |

| R3 | Mordvintsev et al. 2021, [Differentiable Programming of Reaction-Diffusion Patterns](https://selforglive.github.io/alife_rd_textures/ALIFE_Reaction_Diffusion.pdf) | 6p 원문 텍스트 pp.1–5: 모델·loss·학습·해상도/mesh 전이와 제한 | texture 역설계·RD 학습·mesh 일반화 최초성 배제. 비음수/질량 보장은 해당 모델의 전제가 아님 |

| R4 | Pan et al. 2013, [Interactive Localized Liquid Motion Editing](https://www.cs.columbia.edu/~cxz/publications/sketchy_fluids.pdf) | 10p 원문 관련 텍스트: 소개의 deformation/guidance 절차, 검색 색인의 마지막 limitation. 페이지 screenshot은 timeout | shape/keyframe 국소 수정·시공간 전파 선례. 다중 재질 반응 보호까지 비교한 것은 아님 |

| R5 | Barbič et al. 2012, [Interactive Editing of Deformable Simulations](https://www.cs.columbia.edu/cg/pdfs/1344873352-BarbicSinGrinspun-SIGGRAPH2012.pdf) | 8p 원문 텍스트 pp.1–4, 목적·fully actuated 설명·reduced constraints·경계 | 세부 보존·제약 편집·reduction은 기존. 원문의 ‘모든 force 허용’과 MDSSP 제한 입력을 구분 |

| R6 | Kong et al., [Hierarchical Differentiable Fluid Simulation](https://onlinelibrary.wiley.com/doi/abs/10.1111/cgf.70226), [저자 목록](https://www.cs.mcgill.ca/~kry/) | 출판사 원문 HTML 관련 §1/3.2–3.4/4.3과 토의. 2025-10-17 온라인 출판, 사이트의 자동 인용에는 2026 표기도 있음 | bulk/subgrid 역설계·메모리 절감이 이미 존재. 무조건 빠르다고 비교하지 않음 |

| R7 | Ishitobi et al. 2026, [Directable Weathering…Pseudo-Time Reversal](https://doi.org/10.1109/MCG.2026.3734161), [논문 초록](https://pubmed.ncbi.nlm.nih.gov/42747933/) | **논문 초록만**. 2023 학위논문과 동일 전체 알고리즘이라고 가정하지 않음 | 시공간 offset/되돌리기 선례. 세부 다중 State 보호 지원 여부 미확인 |

| R8 | Bangaru et al. 2021, [Systematically Differentiating Parametric Discontinuities](https://people.csail.mit.edu/sbangaru/projects/teg-2021/teg-2021.pdf) | MIT 저자 PDF의 검색 색인에 노출된 서론/결론. 직접 열기는 크기 제한 | 불연속 미분을 새로운 문제로 처음 제시한다고 쓰지 않음. MDSSP에서 적합성은 미검증 |

| R9 | Ostvar & Wood 2016, [A non-scale-invariant form for coarse-grained diffusion-reaction equations](https://doi.org/10.1063/1.4962421) | 출판사 검색 색인 원문 중 covariance/closure 관련 Eq.29–31·설명과 소개. 전체 직접 열기는 실패 | 평균화에 따른 초기 분리·추가 항·rate 유효화 문제는 기존. 현재 코드의 혼합 반례를 새 이론으로 주장하지 않음 |

| R10 | Milzow 등 2010, [Accounting for subgrid scale topographic variations in flood propagation modeling using MODFLOW](https://doi.org/10.1029/2009WR008088) | 출판사 검색 색인의 초록·§1/4 관련 원문. 직접 DOI 열기는 실패 | subgrid 지형의 수위 의존 통과/저장 함수와 fine 사전 계산은 기존 |

| R11 | [Use of upscaled elevation and surface roughness data in two-dimensional surface water models](https://www.sciencedirect.com/science/article/abs/pii/S0309170811000303) | 출판사 초록/소개·highlights 수준 | cell와 face 평균화·통로 정보의 활용도 비교 대상. 전체 세부 수치법 검토는 남음 |



새 세부 해법을 선택하기 전 R7/R9/R10/R11의 미확인 원문 절과 실제 사용할 기존 closure/optimizer의 정확한 조건을 추가 확인해야 한다. 검색 요약만으로 이들의 미지원 기능을 단정하지 않는다. 기존 Günther/Chen/Lo 등과의 비교는 [[04_Development/06_Prior-Model-Differentiation|이전 분석]]을 함께 적용한다.



#### 추가 확인 — 05_Research 정리 이후



| ID | 1차 출처 | 실제 확인 범위와 접근 제한 | 연구에 주는 제한 |

|---|---|---|---|

| R12 | Akroyd, Smith, McGlashan & Kraft, [Numerical investigation of DQMoM-IEM as a turbulent reaction closure](https://como.ceb.cam.ac.uk/preprints/76/), [Cambridge preprint 76](https://como.ceb.cam.ac.uk/media/preprints/c4e-Preprint-76.pdf), 2009; 후속 CES 2010 [DOI](https://doi.org/10.1016/j.ces.2009.11.010) | 36p 기관 PDF의 표지·초록·목차·서론(PDF pp.1–6) 텍스트. §2 이후 세부 추가 열기는 timeout, CES 직접 열기는 403. preprint와 출판본의 모든 식이 같다고 가정하지 않음 | joint PDF·저차 모멘트/가정 분포·가중 대표 상태의 다중 scalar 반응은 기존. singularity/boundedness도 기존 쟁점. 실제 사용할 갱신식·조건의 상세 검토는 남음 |

| R13 | Marchisio & Fox, 2013, [Quadrature-based moment methods, Chapter 3](https://doi.org/10.1017/CBO9781139016599.004) | Cambridge 출판사 장 요약·서지·목차만. Gaussian quadrature, moment inversion, 다변량 확장 설명. 장 본문은 미확인 | moment→node/weight와 다변량 대표 상태 표현 자체를 새 기여로 주장할 수 없음 |



추가 탐색 중 Iowa State의 `J21_DFPF_2010.pdf`는 기관 방화벽 거절, CQMOM 2011 DOI 직접 열기는 접근 실패했다. 검색에 나왔다는 이유로 그 논문의 세부 알고리즘/보장까지 확인한 것으로 기록하지 않는다. 위 두 문헌이 MDSSP의 capped geometry·State별 local flux까지 그대로 해결한다는 결론도 내리지 않는다.



#### 심화 추론의 신규성 대조 — 2026-10-27



아래 자료는 [[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#source-analysis-11-contribution-proof-obligations|심화 추론]]의 기존 수학/방법과 겹치는 부분을 확인하기 위한 것이다. 열람 범위 밖의 세부 성능·보장·미지원 기능은 단정하지 않는다.



| ID | 1차 출처 | 확인 범위 | 주장에 주는 제한 |

|---|---|---|---|

| R14 | Denuit, Dhaene, Goovaerts & Kaas, 2005, *Actuarial Theory for Dependent Risks*, [출판사 발췌 §1.7.1](https://catalogimages.wiley.com/images/db/pdf/9780470014929.excerpt.pdf) | 출판사 PDF의 서지 및 검색 색인의 stop-loss 정의·성질·분포 복원 식. 문서 전체 정독 아님 | positive-part 평균과 tail/분포의 관계는 기존 수학. MDSSP의 가중 hinge 대응은 대입/유도 |

| R15 | Klimenko & Bilger, 1999, [Conditional moment closure for turbulent combustion](https://doi.org/10.1016/S0360-1285(99)00006-4) | 출판사 초록 | 조건부 평균/고차 모멘트로 반응을 다루는 접근은 기존. 현재 threshold bin의 구체적 갱신식과 동일하다고 하지 않음 |

| R16 | Piazzon, Sommariva & Vianello, 2017, [Caratheodory–Tchakaloff subsampling](https://emis.muni.cz/journals/DRNA/pdf/piazzonsommarivavianello_2017_cts.pdf), [저자 페이지](https://www.math.unipd.it/~marcov/Tchakaloff.html) | 원문 검색 색인의 Theorem 1.1과 conic-combination 증명 도입. PDF/저자 페이지 직접 열기는 timeout | 유한 함수공간의 양수 정확 cubature는 알려진 결과. 비다항 feature 선택만으로 새 정리 아님 |

| R17 | An, Kim & James, 2008, [Optimizing cubature for efficient integration of subspace deformations](https://doi.org/10.1145/1409060.1409118), [저자 PDF](https://www.cs.cornell.edu/~djames/papers/cubature08.pdf) | ACM 초록·검색 색인, 저자 PDF 색인의 서론/Eq.1–5. 직접 PDF는 크기 제한 실패 | 특정 재질/형상용 cubature·runtime error estimation도 Graphics의 직접 선례 |

| R18 | Hernández, Caicedo & Ferrer, 2017, [Dimensional hyper-reduction…via empirical cubature](https://doi.org/10.1016/j.cma.2016.10.022) | 출판사 색인에 노출된 초록·서론·방법 설명 일부 | 양수 weights·적분 feature 적합·volume 조건의 선례. MDSSP의 edge event 갱신까지 해결했다는 뜻은 아님 |

| R19 | Hernández, Ares de Parga & Rossi, 2026-09-02, […empirical cubature with manifold-adaptive weights](https://arxiv.org/abs/2609.03068) | **arXiv preprint 초록만**, 심사 완료 여부 미확인 | adaptive positive weights 자체의 최초성도 배제. 보고된 속도/압축 수치를 MDSSP 기대 성능으로 쓰지 않음 |

| R20 | Wolf & Wimmel, 2012, [Applying CEGAR to the Petri Net State Equation](https://arxiv.org/abs/1208.2159) | 논문 초록·서지 | state-equation overapproximation과 반례 refinement·unreachability 진단은 기존. 연속 MDSSP의 sound relaxation은 별도로 유도해야 함 |

| R21 | Ovchinnikov et al., 2021, [CLUE: Exact maximal reduction…constrained lumping](https://pubmed.ncbi.nlm.nih.gov/34037712/) | 논문 초록 | 사용자 지정 선형 관측을 보존하는 polynomial ODE의 constrained linear lumping은 기존. 현재 piecewise GPU Step에 그대로 적용된다고 하지 않음 |

| R22 | Tan, Mahdaviani & Khisti, 2021, [Markov Rewards Processes with Impulse Rewards and Absorbing States](https://arxiv.org/abs/2105.00330) | 저자 preprint 초록 | transition에 누적량을 부여하는 생각은 기존. MDSSP 자체가 이 선형 Markov 모델이라는 주장은 아님 |

| R23 | Sole-Mari et al., 2020, [Lagrangian Modeling of Mixing-Limited Reactive Transport…](https://doi.org/10.1029/2019WR026993) | 출판사 검색 색인의 초록/서론 일부 | subgrid segregation·fluctuation의 생성/수송/감쇠와 반응 영향은 직접 비교 분야. 정확한 모델/세부 closure 검토는 남음 |

| R24 | Girard & Pappas, 2005, [Approximate Bisimulations for Nonlinear Dynamical Systems](https://www.georgejpappas.org/wp-content/uploads/2024/04/CDC05-Nonlinear.pdf) | 저자 PDF 검색 색인의 초록·서론·transition-system 정의 일부 | 미래 관측 차이를 제한하는 equivalence/축약의 원리는 기존. MDSSP에 대한 bisimulation function을 구한 것은 아님 |



#### 실행 없는 최소 모델 연구의 대조 — 2026-10-27



[[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#source-analysis-12-exact-submodels-and-limits|12의 유도]]와 [[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#source-analysis-13-method-blueprints-and-decision|13의 방법 초안]]에서 이미 알려진 부분을 분리하기 위해 확인했다. 검색의 자동 Published/Crawled 날짜 대신 문서 서지를 따른다.



| ID | 1차 출처 | 실제 확인 수준·접근 제한 | 연구에 주는 제한 |

|---|---|---|---|

| R25 | Li & Stinis, [Mori–Zwanzig reduced models for uncertainty quantification](https://arxiv.org/abs/1803.02826), 2018 preprint; [출판사](https://www.aimsciences.org/article/doi/10.3934/jcd.2019002?viewType=HTML), 2019 | 저자 preprint 초록·서지, 출판사 검색의 초록/그림 설명. 전체 세부 증명 정독 아님 | 미해결 상태를 제거하면 기억이 남고 이를 auxiliary dynamics로 근사한다는 발상은 기존. memory 비용 자체가 클 수 있음 |

| R26 | Åström & Murray, *Feedback Systems*, [저자 기관의 2007 원고](https://www.cds.caltech.edu/~murray/books/AM08/pdf/cds101-complete_20Sep07.pdf), 2008 출판본의 전신; [저자 강의 서지](https://murray.cds.caltech.edu/CDS_110b%3A_State_Estimation) | PDF 검색 색인의 observability/Kalman decomposition 관련 §7 설명·식, 저자 강의의 책/장 안내 | observability rank와 exact reduction 하한은 표준 제어 이론. MDSSP path의 삼각행렬 증명은 이 개념의 적용 |

| R27 | Bui-Thanh, Willcox, Ghattas & van Bloemen Waanders, 2007, [Goal-oriented, model-constrained optimization for reduction of large-scale systems](https://doi.org/10.1016/j.jcp.2006.10.026), [저자 기관 PDF](https://kiwi.oden.utexas.edu/papers/Goal-oriented-basis-optimization-Bui-Willcox.pdf) | 출판사 검색에서 제공한 초록·서론·목적식 관련 section snippets; 직접 페이지 open은 403. 저자 PDF 전체는 미열람 | output functional·input 범위·governing equation을 맞춘 축소와 역문제/제어 적용은 기존. 보호 loss에 맞춘 basis 자체가 새 기여 아님 |

| R28 | Fischer, Roth, Wick, Chamoin & Fau, 2024, [MORe DWR](https://research.uni-hannover.de/en/publications/more-dwr-space-time-goal-oriented-error-control-for-incremental-p/), [DOI](https://doi.org/10.1016/j.jcp.2024.112863) | 저자 기관 초록·서지, 출판사 검색에 노출된 DWR/basis refinement 설명. DOI 직접 open 실패. 논문 전체 정독 아님 | 목표 오차를 추정하며 online basis를 확장하는 방법은 기존. high-fidelity 추가 계산 비용도 포함해야 함 |

| R29 | Chen, [Flow* 저자 저장소](https://github.com/chenxin415/flowstar), [RWTH 학위논문: Reachability analysis of non-linear hybrid systems using Taylor Models](https://publications.rwth-aachen.de/record/465295) | 저자 저장소 README의 time-bounded flowpipe/Taylor model/safety 설명, 기관 학위논문 소개. 실제 라이브러리 실행/정확성 검증 없음 | interval/Taylor enclosure·hybrid reachability·안전 판정은 기존. 현재 GPU 구현의 인증으로 전용하려면 부동소수/분기 계약이 별도로 필요 |

| R30 | Li, Huang, de Goes, Jin, Bao & Desbrun, 2014, [Space-time editing of elastic motion through material optimization and reduction](https://authors.library.caltech.edu/records/0d7xh-bb970) | 저자 기관 서지·초록만. PDF 본문 미열람 | 제어력·재질 최적화·reduced simulation을 편집에 결합한 선례. MDSSP의 선택적 보호를 불가능하다고 단정할 근거 아님 |

| R31 | de Amorim, Kori & Muroya, 2026, [A Framework for Coalgebraic Reward-Sensitive Bisimulation](https://arxiv.org/abs/2604.01103) | 저자 preprint 초록·서지만, 심사 완료 여부 미확인 | 누적 reward의 차이까지 보존/제한하는 abstraction 개념은 존재. 이번 p 식과 동일한 알고리즘이라는 주장은 아님 |

| R32 | Ren & Wang, 2023, [Frequency-Limited Model Reduction for Linear Positive Systems](https://doi.org/10.3390/app13064039) | 출판사 검색에서 제공된 초록·서론·결론, 세부 BMI 증명 미검토 | positivity와 목적 범위 오차를 함께 제한하는 reduction도 기존. ‘비음수 축소’만으로 새 Contribution 아님 |

| R33 | Zhang & Shu, 2011, [Maximum-principle-satisfying and positivity-preserving high order schemes for conservation laws](https://www.math.purdue.edu/~zhan1966/research/paper/review.pdf), [저자 출판 목록](https://www.dam.brown.edu/people/shu/pub.html) | 저자 기관 PDF 검색 색인의 초록·서론·maximum principle 설명과 저자 목록. 전 증명 정독 아님 | convex-combination/maximum-principle 기반 bound는 기존. MDSSP 유입 전환에 대한 §2.5 유도와 본문 고차 scheme를 같은 알고리즘으로 보지 않음 |



접근 주의: `flowstar.org`는 이번 조회에서 연구 프로젝트와 무관한 내용으로 연결되어 **문헌 근거에서 제외**했다. 저자 GitHub/RWTH 기록을 사용했다. 관련성이 낮은 검색 결과와 2차 요약은 신규성 근거로 채택하지 않았다. 위 대조는 완전한 선행연구 소진이나 해당 방법들의 MDSSP 지원/미지원 판정이 아니다.



#### 잔류 budget의 추가 고찰 — 2026-10-28



| ID | 1차 출처 | 실제 확인 범위 | 연구에 주는 제한 |

|---|---|---|---|

| R34 | Bertsekas & Tsitsiklis, 1991, [An Analysis of Stochastic Shortest Path Problems](https://doi.org/10.1287/moor.16.3.580) | 출판사 초록·서지. 전체 증명 미열람 | graph의 누적 비용과 동적 계획법은 오래된 분야. MDSSP potential을 새 원리로 주장하지 않음 |

| R35 | Rosenberg, Cohen, Mansour & Kaplan, 2020, [Near-optimal Regret Bounds for Stochastic Shortest Path](https://proceedings.mlr.press/v119/rosenberg20a.html), [저자 PDF](https://proceedings.mlr.press/v119/rosenberg20a/rosenberg20a.pdf) | PMLR 서지·초록, PDF 검색 색인의 Lemma 2.1/2.2 및 Bellman 부등식 설명. 전체 학습 알고리즘/증명 정독 아님 | value가 즉시 cost+다음 value를 상회하면 누적 cost를 제한하는 구조의 선례. 그 논문의 확률적 SSP 가정을 MDSSP에 자동 적용하지 않고 14에서 결정론적 장부로 직접 유도 |



[[90_Research-Archive/Research-Notes-2026-10/03_Solver/03_Error-Control-and-Sparse#source-analysis-14-residual-budget-critical-review|14의 시간별 potential]]은 위 기존 원리와 구조적으로 겹친다. 본문에서 새로 필요한 부분은 실제 coupled GPU 모델의 허용 edge/flux/입력 budget을 좁고 싸게 유지하는 방법이며 아직 미확보다. 논문의 regret/학습 성능을 MDSSP의 계산 속도 근거로 사용하지 않는다.



#### 공동 상태 오차·적응 갱신의 대조 — 2026-10-28



| ID | 1차 출처 | 실제 확인 범위·접근 제한 | 연구에 주는 제한 |

|---|---|---|---|

| R36 | Crandall & Tartar, 1980, [Some relations between nonexpansive and order preserving mappings](https://doi.org/10.1090/S0002-9939-1980-0553381-X); Nussbaum & Shustin, 2001, [Nonexpansive Periodic Operators in l1…](https://sites.math.rutgers.edu/~nussbaum/Pubs/nonexpansiveJDDE.pdf) | Crandall–Tartar 원문 AMS PDF는 403으로 **직접 열람하지 못함**. 저자 기관의 Nussbaum–Shustin 44p PDF 중 인쇄 pp.388 전후의 단조/적분 보존 정의와 Crandall–Tartar 성질·적용 증명을 확인 | L1 비팽창의 단조/총량 보존 논리는 기존. 15에서는 유입 반응이 이 성질을 그대로 유지하지 않는 제한 반례를 따로 유도 |

| R37 | Cohen, Kaber, Müller & Postel, 2003, [Fully adaptive multiresolution finite volume schemes for conservation laws](https://doi.org/10.1090/S0025-5718-01-01391-6), [저자 기관 PDF](https://www.ljll.fr/~postel/HDR/publications/Multiresolution.pdf) | 저자 PDF의 검색 색인에 노출된 적응 오차/수축 논의 일부, 후속 저자 논문의 참고문헌에서 서지 확인. PDF 직접 open은 반복 timeout, AMS 직접 PDF는 403. **전체 증명 정독 아님** | adaptive grid/flux 계산·오차 누적을 연결하는 방법은 기존. MDSSP의 정확한 적용 조건·강한 Baseline 명세를 정하려면 원문 추가 확인 필요 |

| R38 | Bendahmane, Bürger, Ruiz Baier & Schneider, 2008 저자 preprint, [Adaptive multiresolution schemes with local time stepping for two-dimensional degenerate reaction-diffusion systems](https://arxiv.org/abs/0807.1359) | arXiv 서지·초록. finite volume, graded tree/threshold, reference 오차와의 관계, local time stepping 설명 확인. 세부 수치법·증명·출판본 차이는 미검토 | Reaction·적응형 축소·오차 제어·국소 시간 갱신의 조합도 선례가 있음. 동적 Geometry/Incoming 장부의 지원 여부는 초록만으로 단정 불가 |



[[90_Research-Archive/Research-Notes-2026-10/03_Solver/03_Error-Control-and-Sparse#source-analysis-15-joint-state-error-and-geometry-bounds|15의 2V 상한·Lipschitz recurrence·정적 LS 민감도]]는 표준 수지/안정성/미분 도구를 현 코드에 적용한 분석이다. 아직 ‘기존 적응법에 Geometry bound를 붙이면 충분한가’를 반박하지 못했다. 위 논문들의 속도 수치를 MDSSP 성능 근거로 사용하지 않는다.



#### 저비용 범위·다음 갱신 시점의 대조 — 2026-10-28



| ID | 1차 출처 | 실제 확인 범위·제한 | 연구에 주는 제한 |

|---|---|---|---|

| R39 | Guermond, Popov & Tomas, 2019, [Invariant domain preserving discretization-independent schemes and convex limiting for hyperbolic systems](https://doi.org/10.1016/j.cma.2018.11.036), [2018 저자 preprint](https://arxiv.org/abs/1807.02563) | 저자 preprint 초록·서지, 저자 기관 PDF 검색 색인의 표지/초록. 모든 정리·증명 정독 아님 | 불변 집합과 convex limiting은 기존 원리. 16의 공동 하한/simplex 지름을 새 원리로 주장하지 않음. 해당 hyperbolic scheme와 MDSSP가 동일하다는 뜻은 아님 |

| R40 | Anta & Tabuada, 2010, [To sample or not to sample: Self-triggered control for nonlinear systems](https://doi.org/10.1109/TAC.2010.2042980), [2008 저자 preprint](https://arxiv.org/abs/0806.0709) | preprint 초록·서지, IEEE Control Systems Society의 출판 서지. 현재 상태에서 다음 측정/제어 시점을 결정하는 목적과 적용 nonlinear class 확인. 본문 세부 식/증명 미검토 | 매번 판단하는 대신 다음 갱신 시점을 예측하는 기본 발상은 기존. 실제 MDSSP 조건에 맞는 기간/비용 입증이 별도로 필요 |

| R41 | Daniel Silvestre, Paulo Rosa, João P. Hespanha & Carlos Silvestre, 2018, [Self-Triggered and Event-Triggered Set-Valued Observers](https://doi.org/10.1016/j.ins.2017.10.029) | 출판사 검색 색인의 초록·서지·소개 일부. 직접 open은 403. discrete-time LPV 대상, 전체 SVO 계산 사이 저복잡도 state overbound 사용 확인. 전체 알고리즘/증명 미검토 | 싸고 보수적인 범위로 비싼 계산을 줄이는 구조도 선례. 이를 처음 제안했다고 주장하지 않음. 현재 coupled nonlinear Solver에 대한 적용 가능성/미지원 여부는 단정하지 않음 |



[[90_Research-Archive/Research-Notes-2026-10/03_Solver/03_Error-Control-and-Sparse#source-analysis-16-low-cost-certificates-and-sleep-horizons|16]]의 남은 차별화 후보는 유입 장부·형상 cap·동적 수송을 포함하는 실제 모델에서 충분히 좁은 공통 범위를 낮은 비용으로 유지하는 구체적 방법이다. simplex 지름·질량 bin·만료 예약의 조합만으로 신규성을 확정하지 않는다.



#### 열린 경계·모델 합성의 대조 — 2026-10-28



| ID | 1차 출처 | 실제 확인 범위·접근 제한 | 연구에 주는 제한 |

|---|---|---|---|

| R42 | [AMReX 공식 문서: Using FluxRegisters](https://amrex-codes.github.io/amrex/docs_html/AmrCore.html?highlight=fluxregister); Berger & Colella, 1989, [Local adaptive mesh refinement for shock hydrodynamics](https://doi.org/10.1016/0021-9991(89)90035-1) | AMReX 공식 문서 검색 색인에서 Using FluxRegisters 절의 coarse/fine mismatch·시간/면적 가중·reflux 설명 확인. Berger–Colella 출판사 초록/서지 확인, LBL PDF 직접 접근은 404로 본문 미열람 | 경계 flux 장부와 보존 보정은 기존. 보존 일치만으로 반응/형상 결과의 근접성을 주장하지 않음 |

| R43 | Smetana & Patera, 2016, [Optimal Local Approximation Spaces for Component-Based Static Condensation Procedures](https://doi.org/10.1137/15M1009603), [같은 제목의 ECCOMAS 1p 초록](https://www.eccomas2016.org/proceedings/pdf/7753.pdf); Smetana, 2018 preprint, [Static condensation optimal port/interface reduction and error estimation for structural health monitoring](https://arxiv.org/abs/1808.02946) | SIAM 검색 색인의 초록·transfer operator 설명, ECCOMAS 1p 초록, 2018 저자 preprint 초록·서지. SIAM 직접 열기는 timeout. 전체 방법/증명 정독 아님 | port/interface 축소·geometry 변화·재사용도 이미 연구됨. 현재 비선형 유입 반응/limiter 모델에 대한 지원/미지원은 미확인 |

| R44 | Rungger & Zamani, 2015 preprint / 2016 v3, [Compositional Construction of Approximate Abstractions of Interconnected Control Systems](https://arxiv.org/abs/1507.05014), [연결된 출판 DOI](https://doi.org/10.1109/TCNS.2016.2583063) | 저자 preprint 초록·서지·개정 이력. simulation function·small-gain 조건의 목적과 linear subsystem 예제 설명 확인. 전체 증명 미검토 | subsystem 오차 보장을 연결하는 원리도 기존. 17의 수지 합성이 해당 논문의 가정을 만족한다는 주장이나 새 small-gain 정리 주장은 하지 않음 |



[[90_Research-Archive/Research-Notes-2026-10/03_Solver/03_Error-Control-and-Sparse#source-analysis-17-open-boundary-contracts-and-composition|17]]에서는 표준 수지/비음수 하한으로 열린 block의 조건부 상한을 직접 유도했다. 단순 합성은 기존 전체 영역 상한보다 느슨하다는 자체 반론을 함께 기록했다. 다음 후보인 limiter/geometry 의존성 기반 경계 계약도 위 기존법 대비 구체적인 필요성과 비용 이점을 입증해야 한다.



#### 연구 방향 확장에 대한 대조 — 2026-10-28



[[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-18-research-landscape-and-selection|18]]에서 편집·규칙 의미·재사용·상태 식별·LOD·진단·Meso 강건성을 비교했다. 아래는 방향별 직접 또는 인접 선례이며, 완결된 신규성 조사가 아니다.



| ID | 1차 출처 | 실제 확인 범위·제한 | 연구에 주는 제한 |

|---|---|---|---|

| R45 | Hellander & Petzold, 2016 preprint / JCP 2017, [Reaction rates for reaction-diffusion kinetics on unstructured meshes](https://arxiv.org/abs/1609.07687), [출판 DOI](https://doi.org/10.1063/1.4975167) | 저자 preprint 초록·서지, PMC 검색 색인의 Eq.14–17 및 mesh 의존성 설명. PMC 직접 open은 CAPTCHA로 본문 열람 실패. 전체 알고리즘/증명 미검토 | mesh에 맞는 반응률·전처리 자체는 기존. 분자 stochastic RDME이며 MDSSP의 saturation 차이 기반 net flux와 동일하지 않음. 그 극한 결과를 MDSSP에 그대로 적용하지 않음 |

| R46 | Shi et al., 2020, [MATch: Differentiable Material Graphs for Procedural Material Capture](https://dspace.mit.edu/entities/publication/c1487622-b0ae-4002-9a59-4f26c9388494), [DOI](https://doi.org/10.1145/3414685.3417781) | MIT 저장소 초록·저자·서지 확인. 저자 PDF 검색 색인의 기여 설명. 전체 PDF 정독 아님 | 절차 재질의 미분 가능한 graph·관측 적합·편집 가능 출력은 기존. ‘재질 parameter를 최적화한다’만으로 신규 기여 아님 |

| R47 | Gu et al., 2006, [Time-varying Surface Appearance: Acquisition, Modeling, and Rendering — 저자 기관 프로젝트](https://cave.cs.columbia.edu/projects/categories/project?cid=Physics-Based+Vision&pid=Time+Varying+Surface+Appearance) | 저자 기관 페이지 검색 색인의 STAF 정의·26 sample·transfer/control/extrapolation 설명과 서지. 직접 open은 JavaScript shell만 반환. 논문 PDF는 이번에 읽지 않음 | 시간변화 외관의 재사용·속도 제어·외삽은 오래된 선례. 새로운 반응 입력에 대한 상태 동작을 본문에서 어디까지 다루는지는 미확인 |

| R48 | Memmel et al., 2024, [ASID: Active Exploration for System Identification in Robotic Manipulation](https://arxiv.org/abs/2404.12308), [ICLR 2024 PDF](https://openreview.net/pdf?id=jNR6s6OSBT) | arXiv v2 초록·서지, OpenReview PDF 검색 색인의 첫 페이지/초록. v2 저자 목록과 ICLR PDF 첫 페이지 목록에 차이가 있어 동일 판본으로 취급하지 않음. 방법 전체 정독 아님 | simulator로 informative input을 설계해 숨은 parameter를 식별하는 발상은 기존. cap/반응에 대한 목표별 식별의 이점은 별도 입증 필요 |

| R49 | Ghazal, Yusuf & Gaaloul, 2026, [Towards Counterfactual Explanation and Assertion Inference for CPS Debugging](https://arxiv.org/abs/2604.07679) | **preprint 초록·서지만**. 2026-04-09 제출, 심사 완료 여부 미확인 | 실패 입력을 수정해 성공 조건을 설명·추론하는 일반 구조가 이미 제안됨. 초록의 minimal/necessary/sufficient 표현을 MDSSP의 검증된 보장으로 옮기지 않음 |

| R50 | Quéau, Durou & Aujol, 2017 preprint, [Variational Methods for Normal Integration](https://arxiv.org/abs/1709.05965) | 저자 preprint 초록·서지. 비직사각 영역·자유 경계·깊이 불연속을 다루는 목적 확인. 전체 방법/비교 정독 아님 | 복원 경계·불연속 처리 자체는 기존. 동작 강건성 관측이 별도 과제인지 입증해야 함 |

| R51 | Dorsey, Pedersen & Hanrahan, 1996, [Flow and Changes in Appearance — 저자 기관](https://graphics.cs.yale.edu/publications/flow-and-changes-appearance) | 저자 기관 초록·서지, 기관 PDF 검색 색인 첫 페이지/서론 일부 | 현상학적 표면 물 흐름과 흡수·용해·퇴적의 결합은 오래된 선례. ‘물리적으로 완전하지 않아도 연출 가능’이나 ‘흐름 후 잔류’ 자체를 새 기여로 주장하지 않음 |

| R52 | Flynn et al., 2021, [Generalized fluid carving with fast lattice-guided seam computation](https://doi.org/10.1145/3478513.3480544) | ACM 출판사 검색 색인의 제목·서지·선행 fluid carving의 particle/rectangular 제한 설명. 직접 open은 403. 전체 알고리즘 미검토 | 시뮬레이션 결과의 변경/재사용은 기존 분야. post-processing과 새 입력에 대한 온라인 반응 재사용을 구분하되 선행의 지원 범위를 단정하지 않음 |



18의 거리 지수 보정은 표준 감쇠의 합성 성질을 대입한 것으로 별도 신규성 주장이 아니다. R45는 가장 직접적인 수학적 동일 모델이 아니라 mesh/반응 일관성의 비교 분야다. 향후 실제 알고리즘을 고르면 초록 수준의 선례를 원문 수준으로 좁혀 비교해야 한다.



#### 보호 편집·규칙 의미의 심화 대조 — 2026-10-28



[[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-19-protected-editing-feasibility-and-response-coordinates|19]]와 [[90_Research-Archive/Research-Notes-2026-10/03_Solver/01_Forward-and-Reaction-Semantics#source-solver-20-transport-reaction-semantics-and-refinement|20]]의 조건부 유도에서 표준 원리와 새 연구 공백을 구분하기 위해 확인했다.



| ID | 1차 출처 | 실제 확인 범위·제한 | 연구에 주는 제한 |

|---|---|---|---|

| R53 | Kong, Payne, Zhu & Johnson, 2023 preprint / 2024 v3, [Saltation Matrices: The Essential Tool for Linearizing Hybrid Dynamical Systems](https://arxiv.org/abs/2306.06862), [출판 DOI](https://doi.org/10.1109/JPROC.2024.3440211) | 저자 preprint 초록·서지·개정 이력. 전체 유도/증명 정독 아님 | event timing·jump sensitivity는 기존. MDSSP의 연속 kink/고정 discrete step에 reset 공식을 무조건 적용할 수 없음 |

| R54 | Paulus et al., ICLR 2026, [Differentiable Simulation of Hard Contacts with Soft Gradients for Learning and Control](https://proceedings.iclr.cc/paper_files/paper/2026/hash/44039e59aaf6a41b16f1fc5b27bcd409-Abstract-Conference.html) | 공식 proceedings 초록·저자·출판 상태. full paper는 이번에 읽지 않음. adaptive integration과 backward-only CFD/straight-through 설명 확인 | 아직 발생하지 않은 event의 0 gradient를 surrogate로 해결하는 발상은 기존. 이를 MDSSP에 적용했다는 것만으로 신규성 아님. 정량 성능은 비교하지 않음 |

| R55 | University of Washington CHEM452, [Chemical Kinetics §7.3.4 Parallel Reactions](https://faculty.washington.edu/jcv2/CHEM452_WI2023/html/chemical_kinetics.html) | 대학 강의 원문 §7.3.4의 병렬 1차 반응 ODE와 합산 rate·생성물 비율 식 확인. 연구 논문이 아니라 표준식을 확인한 교육 원문. 같은 페이지의 고차 반응식은 사용/검증하지 않음 | 경쟁하는 1차 rate를 합쳐 지수로 적분하고 생성물을 rate 비율로 배분하는 것은 기존. 시간 대신 거리 매개변수를 쓴 것만으로 새 해법 아님 |

| R56 | Descombes et al., 2014, [Analysis of operator splitting in the non-asymptotic regime for nonlinear reaction-diffusion equations](https://arxiv.org/abs/1402.1828), [SIAM DOI](https://doi.org/10.1137/130926006) | preprint v2 초록·서지. 비선형 RD·큰 gradient/반응 시간척도·splitting local error의 목적 확인. 전체 정리/식 미검토 | reaction/transport 순서·큰 step에서의 오차는 기존 연구 문제. 현 Geometry 결합에 대한 해당 bound의 적용은 별도 확인 필요 |

| R57 | 2013, [L∞ estimates and uniqueness results for nonlinear parabolic equations with gradient absorption terms](https://doi.org/10.1016/j.na.2013.06.013) | 출판사 검색 색인의 초록·서론 식 `u_t−νΔu+|∇u|^q=0`·서지. 직접 open은 403. 개별 정리의 q 범위/경계 조건 전체 미검토 | diffusion+gradient absorption 계열 자체는 기존. 20의 q=1 제한 모델에 논문의 모든 정리를 적용한다고 주장하지 않음 |

| R58 | 1994, [The morphological structure of images: the differential equations of morphological scale-space](https://doi.org/10.1109/34.334389), [IEEE 원문 항목](https://ieeexplore.ieee.org/document/334389/) | IEEE 검색 색인의 초록·서지, contour propagation/gradient/erosion/dilation 설명. 직접 open은 JavaScript 검증 화면. 전체 PDE·증명 미검토 | gradient 기반 형태 변화·PDE 표현은 오랜 선례. flux 크기 sink를 새 수학 원리라고 주장하지 않음 |



위 대조는 신규성 주장을 제한한다. 19의 ODE 해·log-product·nullspace는 표준 분석이며, 20의 방향 함수와 경쟁 반응 반례는 제한 모델에 대한 직접 유도다. 전체 제품의 해상도 오류 재현·새 알고리즘의 성공으로 기록하지 않는다.



#### 양수 입력 구성·반응 경쟁 계약의 대조 — 2026-10-28



[[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-21-constructive-controls-and-reaction-contracts|21]]의 분석에서 기존 원리와 남은 연구 질문을 구분하기 위해 확인했다.



| ID | 1차 출처 | 실제 확인 범위·제한 | 연구에 주는 제한 |

|---|---|---|---|

| R59 | Bonvin, Srinivasan, Rodrigues, Billeter & Amrhein, 2016 preprint / 2017 conference, [Concept and Applications of Extents — EPFL](https://infoscience.epfl.ch/entities/publication/400596a8-2af7-4b06-9c90-eda50af5cf1b), [저자 PDF](https://infoscience.epfl.ch/bitstreams/91b63f67-65b3-485d-9bee-7848d474f086/download) | EPFL Graph Search의 해당 출판물 초록·서지 및 PDF 검색 색인의 첫 페이지. PDF 직접 열람은 429 실패. reaction/열·물질전달/inflow 분리라는 목적 확인; 전체 방법 미검토 | 반응 진행 좌표로 dynamics를 분해하는 발상은 기존. 21의 log-Ice 좌표를 논문의 선형 stoichiometric extent와 동일시하지 않음 |

| R60 | Faiz, Agrawal & Murray, 2001, [Differentially Flat Systems with Inequality Constraints: An Approach to Real-Time Feasible Trajectory Generation](https://murray.cds.caltech.edu/Differentially_Flat_Systems_with_Inequality_Constraints:_An_Approach_to_Real-Time_Feasible_Trajectory_Generation) | Caltech 저자 페이지의 초록·서지. output parameterization, state/input/endpoint 제약, inner polytope 설명 확인. 연결 preprint 전체는 미검토 | 출력 경로에서 입력을 복원하고 제약을 만족시키는 것은 기존. 21의 양수 입력 구성만으로 새 control 방법을 주장하지 않음 |

| R61 | ACC 2018, [Flatness-based constrained optimal control of reaction-diffusion systems](https://doi.org/10.23919/ACC.2018.8431201), [IEEE 항목](https://ieeexplore.ieee.org/document/8431201/) | 출판사 검색 색인의 초록·서지. flat output/도함수에서 state·input parameterization 및 constrained optimization 목적 확인. 직접 열람은 JavaScript 검증으로 실패. 저자·전체 방법은 추가 확인 필요 | reaction-diffusion에서도 flatness 기반 제약 제어 선례가 있다. MDSSP 공간·contact·비매끄러운 제약에 직접 적용 가능하다고 단정하지 않음 |

| R62 | Almeida, Bubba, Perthame & Pouchol, 2018 preprint / 2019, [Energy and implicit discretization of the Fokker–Planck and Keller–Segel type equations](https://arxiv.org/abs/1803.10629), [출판 DOI](https://doi.org/10.3934/nhm.2019002) | arXiv 초록·서지와 출판사 검색 색인의 초록/서론 일부. gradient flow/Scharfetter–Gummel, energy·정상상태·비음수성·질량 보존 목적 확인. 전체 증명 미검토 | 구조 보존 drift–diffusion 이산화는 기존. MDSSP의 임의 Profile·동적 Geometry가 같은 energy 구조를 갖는지는 별도 입증 필요 |



21의 양 끝 조건을 고정한 bump 구성·음함수 정리·2-pool 자원 배분·양방향 flux 상쇄는 표준 수학 적용이다. 문헌에 없는 것으로 확인한 새 알고리즘이라는 판정을 하지 않는다. 앱·수치 실험 없이 코드 분석과 제한 모델 유도만 추가했다.

---
