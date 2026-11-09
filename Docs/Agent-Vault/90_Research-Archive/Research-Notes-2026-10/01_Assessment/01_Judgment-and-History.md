# 연구 판단과 이력

초기 시스템 기여 검토, 완료 실험의 해석, 후보 우선순위 변경, 대화 결정과 여러 방향의 교차 분석을 한곳에 모았다. 여러 시점의 판단이 함께 있으므로 현 결론은 [[06_Research/00_Overview|현재 연구 안내]]를 우선한다.

## 먼저 볼 결론

- 완료 실험은 특정 조건에서의 결합 기전을 보여준다. 합리적 모듈식 대조 대비 시스템 기여 우위는 확보하지 못했다.
- 연구 탐색은 보호 편집(A), 반응·수송 의미(B), 미래 반응을 고려한 계산 축소(E) 순이다. 이 순서는 여러 차례 바뀐 판단의 **최신 정리**이며 신규성 판정은 아니다.
- 초기 개요·후보 장부·대화 이력에는 작성 당시의 ‘현재’가 섞여 있다. 현 상태는 [[06_Research/00_Overview|활성 연구 안내]]를 따른다.

## 이 문서의 원본 절

- [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-00-overview|00_Overview.md]]
- [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-02-evidence-and-current-judgment|02_Evidence-and-Current-Judgment.md]]
- [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-03-contribution-candidates|03_Contribution-Candidates.md]]
- [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-08-research-agenda|08_Research-Agenda.md]]
- [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-18-research-landscape-and-selection|18_Research-Landscape-and-Selection.md]]
- [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-reference-00-overview-historical-notes|90_Reference/00_Overview-Historical-Notes.md]]
- [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-reference-01-discussion-history|90_Reference/01_Discussion-History.md]]
- [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-reference-09-analytical-findings|90_Reference/09_Analytical-Findings.md]]

원본 25개 파일의 수정 전 사본과 SHA-256 목록은 `Original-25-Notes.zip`에 있다. 아래 절은 원문의 수식과 당시 판단을 보존하며, 링크와 제목 수준만 새 위치에 맞췄다.

---

## source-00-overview

원본: `00_Overview.md`

### MDSSP 연구 안내

마지막 정리: **2026-11-03 / Asia/Seoul**. 이 문서는 연구 폴더의 **읽는 순서와 현재 판정**이다. 자세한 유도와 이전 판단은 아래 주제 폴더에 보존했다.

#### 현재 판정

MDSSP는 다중 Surface State·Reaction·State-Driven Geometry Coupling을 구현한 현상학적 표면 시뮬레이션 시스템이다. 지금 확인된 것은 결합 기전과 구현 범위이며, **SIGGRAPH Technical Paper 수준의 새로운 Contribution은 아직 입증되지 않았다.** 기존 범용 연구 및 합리적인 모듈식 비교군에 대한 우위가 확인되지 않아 현 학술 판정은 **③ 시스템 통합에 가까움**이다. 물리적 부피 Solver로 바꾸는 것을 합격 조건으로 삼지 않는다.

사용자가 제외한 **축적 순서/층 표현 방향4는 다시 추진하지 않는다.** 로컬 앱·GPU·빌드·벤치마크·학습 실행은 명시적 재개 요청 전까지 중지한다. 현재 이 폴더의 후속 작업은 코드·기록·문헌 읽기와 수식/문서 분석이다.

#### 꼭 읽을 문서

| 우선순위 | 문서 | 무엇을 얻는가 | 필요한 읽기 범위 |
|---|---|---|---|
| **필독 1** | **이 문서** | 현재 결론·방향·문서 지도 | 전체 |
| **필독 2** | [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-02-evidence-and-current-judgment|확보한 증거와 현 판정]] | 실제 구현·실험·미검증을 구분 | §1–5 |
| **필독 3** | [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-03-contribution-candidates|Contribution 후보 장부]] | 현재 후보와 강한 심사 반론 | 맨 앞 **현재 후보 표**. 자세한 반론은 §1–4, 후속 판단은 §10–14 |

다음 실험/판정 순서가 필요하면 [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-08-research-agenda|연구 과제]]를 본다. 여러 방향을 다시 비교할 때만 [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-18-research-landscape-and-selection|방향 확장 비교]]를 읽는다. 출처를 확인할 때는 [[90_Research-Archive/Research-Notes-2026-10/04_Literature/01_Related-Work#source-reference-07-related-work|문헌 장부]]에서 해당 주제의 ID와 실제 열람 범위를 확인한다.

#### 현재 연구 우선순위

| 순위 | 방향 | 지금까지 얻은 것 | 아직 없는 것 | 다음 판정 질문 |
|---|---|---|---|---|
| **1** | **A — 보호 편집·목표 진행 역설계** | 제한 연속 모델의 조건부 입력 해, 현 자산에서 동일 stamp Heat 사건 48개와 예산 중립의 입력 조정 방향 | 실제 contact로 최종 State/후속 효과를 보호한 사례, 표준 optimizer 대비 필요한 새 방법 | 제한된 접촉 입력으로 원하는 변화와 보호를 함께 달성할 수 있는가? 공유 형상이 그 가능 범위를 바꾸는가? |
| **2** | **B — 반응·수송 규칙의 의미 일관성** | 신규 유입 기반 요청과 전체 보유량 소비의 차이, 순수송/양방향 유입·방향/분할 의존성의 제한 모델 반례 | 표준 배분·kinetics·이산화로 해결되지 않는 실제 문제와 새 해법 | 현 규칙의 의미를 고정했을 때 장면·해상도 변경에도 예측 가능한가? |
| **3** | **E — 미래 반응을 고려한 계산 축소** | 제한 모델의 공동 오차 상한과 계산 생략 충분조건 | 열린 경계·다중 반응·동적 Geometry에서 유용한 저비용 방법 | fine Solver 및 기존 sparse/LOD보다 실질적 품질–비용 이점이 있는가? |
| 보류 | 공통 시스템 자체, Meso 복원 강건성 | 구현·제한 실험은 있음 | 강한 기존법 대비 학술적 차이 | 중요한 새 실패/이점이 확인될 때 재평가 |

위 순위는 **연구 탐색 우선순위**이며 채택 가능성의 확률이 아니다. A/B/E 모두 새 알고리즘·SIGGRAPH 기여로 확정되지 않았다. 기능을 많이 구현했거나 기존 코드가 빨라졌다는 사실을 신규성으로 세지 않는다.

#### 폴더별 문서 지도

루트에는 현재 판단을 확인하는 **5개 문서**만 둔다. 기존 연구 문서 24개의 내용을 보존했고, 이전 긴 개요를 별도 보존해 총 25개다. 심화 문서는 목적별로 나눴다. 번호는 작성 순서를 보존하므로 같은 폴더에서 연속하지 않을 수 있다.

| 위치 | 들어 있는 것 | 읽는 경우 |
|---|---|---|
| **루트** | 이 문서, 증거, 후보, 연구 과제, 방향 비교 | 현재 결론·우선순위가 궁금할 때 |
| **`10_Authoring/`** | [[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-04-inverse-authoring|목표 역설계]], [[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-05-protected-editing|보호 편집]], 19·21·22·23의 가능성·입력 감사 | **A를 연구할 때**. 05→22→23을 먼저 읽고 필요한 수식은 19·21 참조 |
| **`20_Solver-Semantics/`** | [[90_Research-Archive/Research-Notes-2026-10/03_Solver/01_Forward-and-Reaction-Semantics#source-solver-10-forward-model-and-controls|실제 forward 계약]], [[90_Research-Archive/Research-Notes-2026-10/03_Solver/01_Forward-and-Reaction-Semantics#source-solver-20-transport-reaction-semantics-and-refinement|수송/반응 의미]] | **B를 연구하거나 구현 의미를 확인할 때** |
| **`30_Deep-Analysis/`** | [[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#source-analysis-06-coarse-reactive-transport|coarse 문제]]와 11–17의 최소 모델·오차·경계·방법 검토 | **E의 수학적 근거와 실패 조건을 검토할 때**. 06→12→16→17 순서 추천 |
| **`90_Reference/`** | [[90_Research-Archive/Research-Notes-2026-10/04_Literature/01_Related-Work#source-reference-07-related-work|문헌 장부]], 대화 이력, 초기 분석, 이전 긴 개요 보존본 | 원래 주장·출처·판단 변경을 추적할 때 |

#### 증거를 읽는 방법

- **[C]** 코드를 직접 확인한 현재 구현, **[E]** 실행된 실험, **[P]** 문헌에서 확인한 범위, **[D]** 조건부 수식 유도, **[H]** 제안 또는 미검증 가설로 구분한다.
- 2026-10-27 이전 완료된 실험의 정본은 [[05_Experiments/02_Performance-Measurements|Performance Measurements]]와 [[05_Experiments/01_Validation-and-Tests|Validation]]이다. 연구 폴더에는 해석과 링크만 둔다.
- 이전 문서의 ‘1순위’는 **그 시점의 판단**이다. 현재 우선순위는 이 문서와 후보 장부의 최신 절을 따른다.
- 제품의 현행 계산 계약은 [[02_Architecture/03_System-Map|Architecture]], 진행 중인 일은 [[04_Development/00_Current-Work|Current Work]]를 따른다. 긴 이전 개요는 [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-reference-00-overview-historical-notes|보존본]]에 있다.

---

## source-02-evidence-and-current-judgment

원본: `02_Evidence-and-Current-Judgment.md`

### 확보한 증거와 현재 연구 판정

마지막 확인: 2026-10-27. 이 문서는 원자료·실험 기록으로 가는 **연구용 색인과 해석**이다. 실행 결과의 숫자/실패 정본은 기존 Experiments 문서와 manifest에 둔다.

#### 1. 구현·설계·새 제안 구분

| 범위 | 현 상태 | 해석 |
|---|---|---|
| 공통 scalar State, Profile, Input/Transport/Decay | 구현·제한된 검증 | 시스템의 기반. 그 자체로 신규성 아님 |
| 2-pass gather, outgoing limiter | 구현·기존 GPU/연구 검사 | 비음수·수지 계약의 근거. 새 보존 알고리즘 주장 아님 |
| Profile v5 Reaction, 공유 소비량 경쟁, 내부 시간 분할 | 구현 | 일반 stiff 반응의 오차 보장 적분기라고 하지 않음 |
| Normal Map Meso·cavity·State 적층·Dynamic Geometry/Weights | 구현·일부 조건 검증 | 물리 부피·열량 보존 모델과 구분 |
| Sparse·cache·precision 경로 | 구현·경로별 검증 범위 상이 | 모든 Scene/결합의 정확성 또는 우위가 검증된 것은 아님 |
| 목표 역설계·보호 편집·gradient·coarse closure | 새 연구 제안, 미구현 | 확정된 기존 기능 또는 검증된 해법으로 쓰지 않음 |
| 제작 과제의 입력/목표·같은 Step 계측 | 준비/구현, 첫 후보 중단 | 제작 성공·사람 생산성 결과 없음 |

#### 2. 완료 실험이 지지하는 것

| 기록 | 완료 범위/핵심 결과 | 지지하는 주장 | 중요한 한계 |
|---|---|---|---|
| PERF-041 Coupling | 성공 31회·25,740 Step, 감사 오류 0. 판의 D/E 물막 차이 공급량 대비 2.95223% | 해당 판에서 지속적 형상 갱신이 후속 수송에 영향을 줌 | 한정된 기전. Bunny seam 링크 누락 94개, Mud 효과 제한, meso=0. 새로운 수치법 필요/외관 우위 미입증 |
| PERF-042 모듈식 대조 | 성공 26회·평가 13,260 Step. 본 7 U/M 쌍의 저장 배열/전체 Step 장부 비트 일치 | 같은 수치 모델의 모듈 조정자도 선언 확장 과제를 처리 | 구조적 중단 적용. 256 진단 4회·정식 비용 40회 미실행. 확장 부담 우위 지지 안 됨. 모델 비신규성의 증명은 아님 |
| PERF-043 Meso | 35회(CPU 12+GPU 19+추가 대조 4), GPU 5,580 Step | 표준 LS의 높이 정확도 개선과 수송 영향은 다른 문제 | 합성128 높이 오차 16.0417%→0.2401%와 달리 brick128 물막 차이는 공급량 대비 0.00062779%, 같은 장면 dt 민감도 0.00243%보다 작음. 한 평면/한 실제 map/4초 |

상세: [[05_Experiments/02_Performance-Measurements|PERF-041/042/043]]. 위 %는 서로 다른 지표이므로 직접 우열 순위로 비교하거나 속도 개선율로 쓰지 않는다. 실험별 수지 검사가 형상 부피/열역학 보존을 뜻하지 않는다.

PERF-043 최초 33회의 production SPIR-V 사전 보관 누락은 남아 있다. 대표 2조건의 재현·44개 배열 일치로 보완했으나 모든 최초 실행에 사전 archive가 있었다고 소급하지 않는다.

#### 3. 제품 검증과 연구 신규성의 구분

- invalid-texel render regression은 테스트가 compact render 정점 ID를 texel ID로 오해한 문제였다. 테스트 수정 후 같은 exe로 재실행해 통과했다. 제품 Geometry를 새로 만든 결과가 아니다.
- scratch 저장 형식 변경 시 descriptor layout/set을 삭제한 문제를 buffer 재연결로 수정했다. 12회 형식 전환 회귀·기존 GPU 검사·실제 앱 6조건/2,172 Step 검증을 통과했다. 수정 전후 18개 State 배열 일치. 일반적인 GPU 자원 수명 수정이다.
- 제작 계측을 켠 기존 6초 대조는 최종 State 3개 배열이 이전 정상 실행과 비트 일치했다. 전체 저장소 테스트 또는 물리 정확도 검사로 확대하지 않는다.

정본: [[05_Experiments/01_Validation-and-Tests|Validation and Tests]], [[04_Development/07_Authoring-Study-Protocol|제작 명세 §7–10]]. 이전 OS 차단·잘못된 경로·불완전 캡처·Vulkan 오류는 별도 보존돼 있다.

#### 4. 첫 제작 후보의 중단 상태

AUTH-GOAL-01 첫 Profile 후보는 spread .30, gravity .05, Waterfilm decay .10, 유입 잔류 전환 .20, 잔류 capacity .5였다. 기존 Ice/Heat/융해·냉각 계수는 유지했다. 이것은 시도한 설정이지 정답 또는 성공 preset이 아니다.

빈 camera track을 로더가 거부한 입력 준비 오류를 고정 track으로 보정한 뒤, 사용자 요청으로 약 10초에서 종료했다. 필요한 12/14/16초·저장 재실행 결과가 없어 **G3/G4 및 전체 목표는 미평가**다. 중단을 모델 실패로 세지 않는다. 현재 인간 session 0, 독립 품질 판정 0이다.

#### 5. 학술적 결론과 남은 공백

현재 지지되는 설명은 **고정 UV graph의 다중 비음수 State 수송·국소 반응·공유 형상 갱신을 공통 계약으로 제공하는 현상학적 시스템**이다. 구현이 작동한다는 증거와 기존 범용 모델보다 중요한 제약을 해결했다는 증거는 다르다.

아직 없는 증거는 선행모델 대비 중요한 표현/제어 이점, 새 알고리즘의 필요·효과, 사람의 제작 생산성, 새 Mesh 일반화, 미측정 비용 우위다. 현재 분류는 **③ 시스템 통합**이며, [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-03-contribution-candidates|후속 후보]]를 검증한 뒤에만 판정을 재검토한다.

---

## source-03-contribution-candidates

원본: `03_Contribution-Candidates.md`

### Contribution 후보 장부



현재 안내 갱신: **2026-11-03**. 아래 유력도는 채택 확률이 아니라 **현재 근거와 연구 추진 가치의 질적 판단**이다. 현재 강한 Contribution으로 확정된 후보는 없다. §1–9는 탐색 과정의 판단을 보존한다. 현재 판단은 다음 표와 §10–14, 빠른 읽기는 [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-00-overview|연구 안내]]를 따른다.



| 현재 순위 | 후보 | 확인된 출발점 | 가장 큰 공백 | 판정 |

|---|---|---|---|---|

| **1** | **A: 목표 진행·보호 편집** | 결합 Solver와 실제 Heat 접촉 48개의 시간 조정 가능성, 제한 연속 모델의 조건부 입력 해 | 실제 접촉으로 목표와 보호를 함께 달성한 사례 및 표준 최적화 대비 필요한 새 방법 | 목적 적합성이 가장 높음. **기여 미입증** |

| **2** | **B: 반응·수송 규칙의 의미 일관성** | 현재 `inflowFraction` 요청/소비 의미와 경로·방향·경쟁 반례 | 표준 배분/kinetics/이산화로 충분하지 않은 실질적 문제와 해법 | 모델 계약을 먼저 고정. **알고리즘 기여 미입증** |

| **3** | **E: 미래 반응을 고려한 계산 축소** | 제한 모델의 수지·오차 상한과 계산 생략 조건 | 열린 경계·다중 반응·동적 형상에서 값싸고 유용한 방법 | 별도 탐색. **실용성·신규성 미입증** |

| 보류 | 공통 State–Reaction–Geometry 시스템 자체, Meso 강건성 | 구현과 제한된 실험 | 가까운 범용 시스템/합리적 모듈식 대조 대비 학술적 차이 | 현 상태의 시스템 통합 주장은 약함 |



**읽는 방법:** 지금 결론만 필요하면 위 표와 [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-02-evidence-and-current-judgment|증거·현 판정]]을 읽는다. 수식·반례를 검토할 때만 해당 절의 링크로 내려간다. [C] 코드, [E] 실행 증거, [D] 조건부 유도, [H] 제안을 혼동하지 않는다.



#### 1. 후보와 판정



| 후보 | 유형 | 현재 근거/차이 | 강한 심사 반론 | 현재 강도 | 추진 조건 |

|---|---|---|---|---|---|

| 공통 State–Reaction–Geometry 모델 | 시스템 | 선언 규칙·국소 flux·형상 결합 구현, 기전 실험 | Günther/Chen/Lo의 범용·결합 선례가 있고 모듈식 대조도 확장 가능 | 낮음 | 기존 모델의 중요한 제작 부담/표현 제약을 해결하는 사례 필요 |

| 목표 진행·후속 반응 역설계(방향1) | 응용·제어/알고리즘 | 현재 미구현. 관측 적합과 후속 동작을 동시에 지정 | loss를 추가한 표준 최적화와 무엇이 다른가? | 낮음 | 모호성의 중요성과 표준 optimizer의 구체적 한계. 방향2 평가 층으로 우선 결합 |

| 다른 현상을 보존하는 수정(방향2) | 알고리즘·제작 시스템 | 현재 미구현. 제한 입력·반응·공유 형상의 보상 문제 | 국소 편집·제약 보존은 기존. SQP로 충분하지 않은가? | 미입증, 주력 탐색 가치 중간 | 강한 동일 조작 Baseline 대비 edit/protection 품질–비용의 실질적 차이 |

| 혼합/경로 coarse 표현(방향3) | 모델·알고리즘 | 평균화/내부 이동 반례와 제한된 두 texel의 exact closure·tail bound 유도. 범용 closure 미확보 | 기존 compartment·memory ROM·subgrid/microgrid의 적용 아닌가? | 문제·최소 해법 구체화, 해법 신규성 미입증 | 외부 port/동적 Geometry의 독립 갱신, 실제 진행 영향과 기존법 대비 총 비용 증거 |

| 후속 오차를 고려한 Sparse Geometry 갱신 | 알고리즘 | 결합과 선택형 Sparse 구현. 새 오차 제어 없음 | dirty/halo/표준 오차 추정으로 충분할 수 있음 | 매우 낮음 | Dense 대조에서 중요한 실패와 단순 해법의 한계가 먼저 필요 |

| Meso 복원·cavity 강건성 | 모델·알고리즘 | 현재 구현과 PERF-043 | 표준 LS/국소 모델 정리로 충분한 것 아닌가? | 매우 낮음 | 실제 asset에서 중요한 수송/표현 실패를 새로 확인할 때 재개 |



#### 2. 신규성으로 간주하지 않는 항목



GPU 사용, 여러 State 채널, 2-pass gather, outgoing limiter, Normal Map 적분, cavity filling, active tile/halo, 기존 속도 최적화, 버그 수정, 이름에 따른 기본 외관 binding은 단독 Contribution으로 주장하지 않는다. 이들이 새 문제의 구체적 해법에 필수인 경우에만 그 역할을 설명한다.



‘State가 형상을 바꾸고 그 형상이 다음 이동에 참여한다’도 넓은 최초성 주장은 불가하다. 형상에 반영하는 양이 Capacity로 제한되는 현 계약에서 저장량 보존과 부피 보존을 동일시하지 않는다.



#### 3. 보류·제외와 재개 조건



- **사용자 제외:** 축적 순서/층 표현 방향4. 새 기능 목록에 다시 넣지 않는다.

- **현재 보류:** 물리적으로 정확한 다중 물질 Solver로의 전환, Chart 대표 높이의 독립 Contribution화, 새로운 Sparse scheduler의 선제 개발.

- **비교 중단 유지:** 같은 선언/수치 provider를 쓰는 U/M backend 선택만으로 제작 시간 우위를 측정하는 연구.

- **연구 재개:** 방향1·2·3의 문헌/수식 분석은 진행한다. 실제 앱/실험 실행은 명시적 재개 요청 전 중지한다.



#### 4. 논문 기여로 전환하는 최소 기준



문제의 중요성, 강한 기존 방법의 구체적인 한계, 새 방법의 실질적 차이, 독립적인 재현·미사용 조건 검증을 함께 요구한다. 새로운 문제나 분석만의 기여도 가능하지만, 일반적인 평균화/관측성/제약 최적화 성질을 MDSSP에 대입한 설명만으로 새 정리를 주장하지 않는다.



실패 기준을 피하려고 목표·조작 범위·Baseline을 사후 변경하지 않는다. 변경이 필요하면 이전 결과와 이유를 보존한다. 사람의 생산성을 주장하려면 실제 제작 과제/참가자 증거가 필요하며, AI 보조 실험은 그 범위를 명시한다.



세부 정식화와 제안은 [[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-04-inverse-authoring|방향1]], [[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-05-protected-editing|방향2]], [[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#source-analysis-06-coarse-reactive-transport|방향3]]을 따른다.



#### 5. 폴더 정리 후 분석에 따른 갱신



[[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-reference-09-analytical-findings|후속 분석]]에서 방향1의 capped 관측 모호성, 방향2의 threshold 아래 0 민감도, 방향3의 동일 이차 모멘트/상이한 융해 request와 capped 높이 평균화 반례를 유도했다. 문제의 구체성은 높아졌지만 실제 허용 rollout·보호 과제·새 closure의 성공을 확인하지 않았으므로 위 유력도는 상향하지 않는다.



다음 판별의 초점은 **1의 문제를 2의 목표/후속 평가에 사용하고, 3에서는 표준 양수 quadrature가 충분한지 먼저 따지는 것**이다. 추가 문헌 R12–R13 때문에 ‘소수 대표 상태를 저장’ 자체의 신규성 주장은 더욱 제한된다.



후속 [[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#source-analysis-11-contribution-proof-obligations|심화 추론]]은 방향3에서 **같은 전체 State histogram·같은 고정 graph·외부 순유량 0이어도 내부 이동량과 inflowFraction 전환량이 다른 네 texel 반례**를 제시했다. 알고리즘 탐색의 우선순위는 방향3이 가장 구체적이다. 다만 hop 기반 규칙의 격자 의존성을 보존할 필요, edge 정보의 독립 갱신, 표준 graph/microgrid/fine Sparse 대비 비용을 입증해야 한다. 신규성이 확보됐다는 평가로 상향하지 않는다.



방향2는 보호 오차의 수지 하한·도달 가능성 바깥쪽 근사를 추가 후보로 삼되, 동일한 사전 검사를 쓰는 표준 optimizer와 비교한다. 방향1은 단독 기여보다 방향2의 목표/평가로 유지한다. 대표 상태·cubature·적응 가중치·도달 불가능 진단·행동 equivalence의 기존 연구 R14–R24도 확인했다.



#### 6. 실험 없는 추가 연구 이후



[[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#source-analysis-12-exact-submodels-and-limits|정확한 최소 모델]]에서 닫힌 두 texel의 내부 이동/유입 전환은 총량 m과 잔여 반응 잠재량 p로 독립 갱신할 수 있고, 내부 모드를 생략할 때의 미래 총 잔류 오차를 p로 제한할 수 있음을 유도했다. **일반 block의 외부 port·잔류 위치·동적 Geometry에서는 아직 닫히지 않는다.** 두 셀의 변수 변환 자체가 차원/속도 개선은 아니며, 이미 구현된 기능도 아니다.



같은 분석은 path 확산의 미래 관측에 대한 차원 하한, cap 아래 형상 보호의 융해량 제한, 한 reaction substep의 Heat/height 보호 witness도 포함한다. 실제 production U-input의 feasible witness와 GPU 일치는 미확인이다.



추가로 닫힌 graph의 saturation-only 수송·단일 유입 전환·충분한 dt에서는 동적 Geometry를 허용하는 잔류/물 오차 bound를 유도했다. 이것은 임의 다중 State 전체 Solver의 인증이 아니다.



[[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#source-analysis-13-method-blueprints-and-decision|방법별 판정]]에서 A(내부 반응 잠재량·port 기반 block)를 알고리즘 탐색 1순위, B(보호 목적에 맞춘 편집/축소)를 목적 적합성 1순위로 유지한다. C(제한적 보호 인증)는 보조 후보, 방향1은 B의 목표/평가다. R25–R33의 memory·목적별 ROM·hybrid verification·reward abstraction·positive reduction·maximum principle 선례 때문에 단순 조합의 신규성은 여전히 낮다.



2026-10-28 [[90_Research-Archive/Research-Notes-2026-10/03_Solver/03_Error-Control-and-Sparse#source-analysis-14-residual-budget-critical-review|추가 고찰]]에서 A를 더 좁혔다. 열린 경계/중력에도 조건부 potential 상한을 만들 수 있지만, cycle을 무기한 허용하면 전체 물량 bound로 퇴화한다. 유한 시간·outgoing 비율·gross inflow budget은 이 한계를 완화하나, 잔류 오차만으로 물 수송/Geometry의 생략을 정당화할 수 없다. 첫 방법은 fine 메모리를 유지하는 제한적 연산 생략으로 검토하고, 수송/경계 관측을 함께 보호해야 한다. R34–R35의 표준 Bellman 원리와 겹치므로 후보의 신규성 평가는 상향하지 않는다.



#### 7. 공동 오차·형상 민감도 분석 이후



[[90_Research-Archive/Research-Notes-2026-10/03_Solver/03_Error-Control-and-Sparse#source-analysis-15-joint-state-error-and-geometry-bounds|15]]에서 단일 Incoming 전환 모델의 **물+잔류 공간 L1 오차**를 실제 flux 차이로 제한했다. raw edge 일부 삭제는 limiter 재분배를 고려해도 한 Step 2V 상한을 갖는다. 한편 고정 계수에서도 반응이 실행 간 오차를 증폭하는 두 texel 반례가 있으므로, 총량 보존을 근거로 장기 오차 비증폭을 가정할 수 없다.



현재 `effectiveLocalNormal`의 LS 행렬은 정적 mesh 기준이어서 높이→Normal 민감도 일부를 사전 계산할 수 있다. 방향3의 저비용 판정과 방향2의 형상 보호 평가에 활용할 여지가 있지만, 중력 정규화 margin·동적 Concavity·다중 반응·실제 비용을 해결하지 못했다. **방향3은 알고리즘 탐색의 구체성 1순위, 방향2는 제작 목적 적합성 1순위, 방향1은 방향2와 결합**하는 판단을 유지한다. 기존 adaptive multiresolution/RD 선례 R36–R38 때문에 이 분석 자체를 새 Contribution으로 평가하지 않는다.



#### 8. 저비용 판정 구성 이후



[[90_Research-Archive/Research-Notes-2026-10/03_Solver/03_Error-Control-and-Sparse#source-analysis-16-low-cost-certificates-and-sleep-horizons|16]]에서는 민감도 recurrence 대신 공통 하한·수지를 써서 두 실행의 공동 오차를 제한했다. 닫힌 saturation-only 영역의 장기 2B 상한, 중력/limiter를 포함하는 outgoing 비율 α 상한, α 구간별 초기 물량을 사용하는 O(K) 기간 판정을 구성했다. 따라서 방향3은 **저비용 충분조건의 구체적 구성**까지 진전했다. 다만 실제 기간/비용·열린 경계·다중 반응의 유용성은 미입증이다.



cap 위 State 변화의 높이 재계산 생략은 이미 `StateFinalize.glsl`에 구현돼 있어 별도 새 기여에서 제외한다. 높은 해상도에서 α가 1로 퇴화할 수 있다는 분석과 열린 block 반례는 후보의 적용 범위를 좁힌다. R39–R41의 invariant-domain/self-triggered/set-valued 선례를 고려해 **신규성 평가는 상향하지 않는다.** 이전과 마찬가지로 방향3은 알고리즘 탐색, 방향2는 제작 목적 적합성이 가장 구체적이다.



#### 9. 열린 경계 분석 — 단순 합성안 보류



[[90_Research-Archive/Research-Notes-2026-10/03_Solver/03_Error-Control-and-Sparse#source-analysis-17-open-boundary-contracts-and-composition|17]]은 공통 유입 cohort·port-time outgoing을 포함한 오차와 block 합성을 유도했다. 그러나 **단순 α-bin block 합성은 동일 조건의 전체 영역 bound보다 느슨**하며, 정확하게 유지되는 이동까지 예산에 넣는다. 이 하위안을 주력 구현 후보에서 보류한다. 방향3 전체의 불가능 판정은 아니다.



코드에서 더 구체적인 문제를 확인했다. 경계 source의 양과 경계 raw가 같아도 내부 raw outgoing 합이 달라지면 source limiter 때문에 경계 Flux가 달라진다. 또 채널별 물질 flux가 0이어도 적층→Normal/weight를 통해 외부 수송이 달라질 수 있다. 다음 탐색 후보는 **내부 outgoing 범위·Geometry stencil 영향을 포함해 내부 갱신 생략의 충분조건을 판정하는 경계 계약**이다. 일부 branch 조건은 유도했지만 저비용 갱신·실제 중요성은 미입증이다. R42–R44의 port reduction/보존 장부/합성 선례와 경쟁해야 하며 신규성 평가는 유지한다.



#### 10. 연구 목적을 넓힌 최신 비교 — 2026-10-28



[[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-18-research-landscape-and-selection|18]]에서 기존 1·2·3과 새 분기를 일곱 방향으로 비교했다. 세부 후보가 모두 독립된 Contribution이라는 뜻은 아니다.



| 방향 | 현재 판정 | 기존 장부와 달라진 점 |

|---|---|---|

| A 보호 편집 | 목적 적합성 기준 주력 탐색, 신규성 미입증 | 기존 1+2 유지. 진단 F와 재사용 C는 보조 평가로 먼저 사용 |

| B 해상도 변화에 대한 재질 규칙의 의미 일관성 | **새 후보 중 우선 판별**, 새 해법 필요성 미확인 | 유입당 전환의 경로 분할 의존성·Incoming 합의 거리 정보 손실을 분리. 표준 거리 보정/가중 gather로 충분하면 독립 기여 중단 |

| C 동작 명세 기반 자산 재사용 | 조건부 시스템/제작 후보 | profile 복사에서 반응 순서·배출 비율 등 동작의 재사용으로 과제 명확화. 기존 weathering transfer와 경쟁 |

| D 미래 반응을 위한 상태 식별 | 사용 시나리오가 있을 때만 추진 | cap 모호성의 별도 응용. 내부 State를 이미 아는 정상 재생에는 필요 없음 |

| E 미래 상호작용을 보존하는 LOD | 제한된 독립 알고리즘 탐색 | 기존 방향3. 분석량이 많다는 이유로 전체 연구 1순위로 두지 않음. 단순 block bound 보류 유지 |

| F 실패 원인·수정 가능성 진단 | 독립 기여 가능성 낮음, A의 보조 | 장부와 인과효과 구분. 표준 ablation/optimizer보다 필요한 차이 미입증 |

| G Meso 불확실성에 대한 동작 강건성 | 보류 | PERF-043의 약한 실제 효과 때문에 새 실패 증거 전 주력화하지 않음 |



이 표의 우선도는 실험 재개/구현 지시가 아니다. 사용자가 제외한 기존 방향4는 계속 제외한다. 새 기여가 확보됐다는 판정도 하지 않는다.



#### 11. A/B 심화 이후 — 문제 근거와 해법 신규성의 분리



[[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-19-protected-editing-feasibility-and-response-coordinates|19]]에서 A의 제약 종속성과 반응 진행 좌표를 유도했다. Heat 전 과정 보호는 제한 모델에서 Ice 진행을 고정하므로 잘못된 편집 과제를 제거할 수 있다. 공급 비율/substep을 포함한 이산 Z는 최종 Ice 보호를 표현하지만, 그 자체는 정보 압축/새 optimizer가 아니다. **A는 목적 적합성 주력 후보를 유지하되 실제 입력 witness·표준법 대비 필요성은 미입증**이다.



[[90_Research-Archive/Research-Notes-2026-10/03_Solver/01_Forward-and-Reaction-Semantics#source-solver-20-transport-reaction-semantics-and-refinement|20]]에서 B의 순수송/교환량 차이, 거리 보정 이후 stencil 방향성, 경쟁 전환의 분할 반례를 유도했다. **B의 문제 정의는 더 구체적**이다. 그러나 표준 병렬 kinetics·gradient absorption PDE·operator splitting을 강한 대조로 추가했으므로 해법 신규성은 올리지 않는다. 네 이웃의 분석을 현 여덟 방향 GPU 오류의 실측으로 취급하지 않는다.



현재 탐색의 우선순위는 A와 B를 유지하고 E는 제한된 별도 탐색이다. ‘문제를 찾았음’, ‘조건부 유도’, ‘구현 성공’, ‘기존법 대비 새 기여’는 계속 구분한다.



#### 12. 양수 입력 구성 이후의 판정



[[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-21-constructive-controls-and-reaction-contracts|21]]에서는 A의 제한 연속 모델에 양수 입력을 직접 구성했다. 입력 여유가 있으면 동일 초기·최종 Ice/Water/Heat와 동일 총 Heat 입력에서도 중간 진행을 바꿀 수 있다. **과제의 조건부 가능성 근거가 강화됐으나, 현 contact/GPU의 성공·새 optimizer의 기여는 아니다.** A를 주력 탐색으로 유지하되 표준 제어/최적화에도 이 구성과 초기값을 허용한다.



B는 현 inflowFraction이 신규 유입 기반 요청을 전체 보유량에서 처리함을 확인했다. 신규분만 소비하는 대안은 다른 모델 계약이며 표준 자원 배분으로 다룰 수 있다. 따라서 B의 우선 과제는 **일관된 의미와 재사용 조건을 정의하는 것**이다. 새 수치 알고리즘의 필요성은 아직 입증하지 못했다. 기존 rule 결과 불변·새 rule의 양의 소비·유한 공급을 동시에 요구하는 조합성 기준은 불가능하므로 사용하지 않는다. E는 이번 분석으로 평가가 달라지지 않았다.



#### 13. 실제 편집 자유도·형상 경로를 통한 A의 축소



[[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-22-protected-editability-and-geometry-mediation|22]]는 21의 양수 입력 witness가 기존 입력의 시간 재배치를 허용한다는 점을 명시했다. 추가만 가능한 조작과 동일 총 입력을 동시에 요구하면 비자명한 입력 편집은 불가능하다. 유한 contact basis에서는 보호 제약의 rank와 입력 bound가 실제 자유도를 결정한다. cap 아래 Ice↔Water 높이의 보호 한계는 이미 12에서 유도했으므로 신규 기여로 중복 계산하지 않는다.



A의 강한 실험적 질문은 **공유 Geometry가 보호 편집의 가능 범위에 실제로 어떤 차이를 주는가**다. 기준 궤적의 시간별 Geometry를 재생하는 진단은 초기에 형상 하나를 고정한 기존 E ablation과 다르며 현재 미구현이다. full과 이 재생 대조가 같은 기준 궤적에서 출발하게 해야 기전 비교가 해석 가능하다. 기전 효과가 확인돼도 표준 최적화/모듈식 구조 대비 연구 기여는 별도 입증 사항이다. A의 우선순위는 유지하지만 신규성 등급을 올리지 않는다.



#### 14. 코드가 제공하는 실제 입력 자유도



[[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-23-contact-control-space-audit|23]]에서 `b_heated`의 서로 다른 48개 시점에 같은 spatial stamp의 Heat Add가 존재함을 확인했다. 두 strength를 `+ε/−ε`로 조정하면 현재 asset 형식 안에서 texel별 시간 합산 **요청 입력**을 보존할 수 있다. 무한차원 연속 입력 해와 실제 contact basis 사이의 한 공백을 좁혔다. 그러나 최종 State·Geometry·보호 지표를 유지하는 구체적인 접촉 입력은 아직 없다. A의 **문제 실행 가능성 근거만 부분 강화**, 알고리즘 신규성·실험적 성공 등급은 그대로다.

---

## source-08-research-agenda

원본: `08_Research-Agenda.md`

### 연구 질문과 판정 순서



마지막 갱신: 2026-10-28. [[04_Development/08_Research-Directions-123|최초 통합 검토]]에서 분리한 후속 연구 정본이다. 이 파일의 내용은 제안·수식 분석이며 새 구현/실험 결과가 아니다. 로컬 앱·GPU·빌드 실행 중지를 유지한다.



#### 최신의 넓은 연구 선택



[[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-18-research-landscape-and-selection|18]]에서 일곱 방향을 비교했다. 다음 비실행 판별은 **보호 편집의 작은 과제**와 **재질 규칙의 사건·거리·시간 의미 및 해상도 일관성**을 우선한다. 계산 축소는 제한된 별도 탐색으로 남기며, 단순 α-bin block 합성은 보류한다.



새 방향 B는 기존 fine 모델을 유지하는 최적화가 아니라 필요하면 규칙 의미를 바꾸는 연구다. 먼저 표준 거리 보정·가중 Incoming gather·continuous reaction으로 충분한지를 분석한다. 후보 A–G 문자는 18의 연구 방향 구분이며, 아래 과거 순서표 A–E와 별개다. 기존 방향4인 층/순서 모델은 제외한다.



이후 재사용 C는 보호 편집의 미사용 자산 평가로, 진단 F는 실패 해석으로 먼저 좁힌다. 상태 식별 D는 불완전 관측의 사용 시나리오가 있을 때만, Meso 강건성 G는 중요한 실제 영향의 근거가 생길 때만 추진한다. 아래 기록은 각 시점의 분석을 보존한다.



#### 기존 방향1·2·3의 판별 절차



**A/B 심화 이후의 구체화:** [[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-19-protected-editing-feasibility-and-response-coordinates|19]]의 A0–A3와 [[90_Research-Archive/Research-Notes-2026-10/03_Solver/01_Forward-and-Reaction-Semantics#source-solver-20-transport-reaction-semantics-and-refinement|20]]의 B0–B4를 후속 명세로 사용한다. A는 보호 요구가 편집 자체를 막는 조건과 실제 입력의 자유도를 먼저 구분한다. B는 거리·방향·경쟁의 표준 해법을 먼저 검토하고 Geometry 결합에서 남는 공백을 찾는다. 둘 다 미실행 명세이며, 성능/제작 효과/신규 알고리즘 성공 기록이 아니다.



공통 근거는 [[90_Research-Archive/Research-Notes-2026-10/03_Solver/01_Forward-and-Reaction-Semantics#source-solver-10-forward-model-and-controls|forward 모델]], 문헌 출처는 [[90_Research-Archive/Research-Notes-2026-10/04_Literature/01_Related-Work#source-reference-07-related-work|문헌 확인 수준]], 이후 연구는 [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-reference-09-analytical-findings|분석 노트]]를 따른다.





| 순서 | 산출물 | 계속할 조건 | 중단/축소할 조건 |

|---|---|---|---|

| A | 방향 1의 관측-후속 반응 모호성 진단, 방향 2의 허용 조작/보호 지표·실행 가능한 단위 과제 | 제작상 중요한 trade-off가 실제 있음 | 목표가 수치 floor 안의 차이이거나 UI 문제뿐임 |

| B | 작은 차원의 **전역 표준 constrained optimizer** 결과 | 같은 의미·예산에서 해결 안 되는 정확한 이유가 남음 | 표준 방법으로 충분하면 새 특화 알고리즘 보류 |

| C | 방향 2의 coupling-aware 수정 초안·gradient/범위 Ablation | forward 검증과 보호 오차를 맞추며 유용한 품질–비용 차이 | 단순 loss/기본 최적화 조합 이상 차이 없음 |

| D | 방향 3의 reaction-only 최소 반례·표준 closure/microgrid 대조 | 정보 손실이 실제 진행/외관에서 중요하고 기존 closure의 한계가 있음 | 단위 보정/표준 mixing correction으로 충분 |

| E | 한 연구 축만 선택해 사례/미사용 입력/mesh 확대 | 중요한 모델 차이와 해결 방법을 설명 가능 | 세 문제를 통합했다는 이유만으로 범위 확대하지 않음 |



A/B와 D는 독립적인 문제 탐색이지만 실행 자동화나 동시 GPU 부하를 지금 시작하지 않는다. 사람의 제작 시간 연구, UI 제작, 모든 GLSL의 differentiable rewrite, 대규모 학습 데이터 생성은 초기에 필요하지 않다. 1+2의 forward가 지나치게 비싸면 먼저 짧은 headless 상태 목표 진단을 고려하되, production과 일치한 범위·renderer/접촉 생략을 명시한다.



**최종 연구 권고:** 1은 2의 목표·평가 층으로 통합한다. 2는 ‘제약된 조작으로 결합 결과를 선택적으로 수정’하는 방법이 표준 최적화보다 필요한지를 먼저 확인한다. 3은 반응 평균화의 코드 기반 반례에서 출발하되 기존 closure를 반드시 상대한다. 지금 확정할 것은 문제/기각 조건이며, 확정 Contribution이나 채택 확률이 아니다.



#### 이번 분석 이후 구체화된 최소 질문



[[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-reference-09-analytical-findings|분석 노트]]에서 ANA-1/2/3의 가정·강한 대조·기각 조건을 정리했다. 이 식별자는 실험 실행 번호가 아니다.



- **ANA-1:** Ice의 숨은 초과 저장량 반례를 U-seed 진단과 초기 고정 U-input 과제로 구분한다. 후자에서 실제 허용 rollout 쌍이 생기는지는 미확인이다.

- **ANA-2:** threshold 아래의 작은 gradient만으로 불가능을 판정하지 않는다. event margin 기반 초안보다 먼저 다중 크기 FD+SQP·multi-start·derivative-free의 능력을 확인한다.

- **ANA-3:** 평균의 곱 반례에 더해 동일 이차 모멘트/상이한 ramp request, capped height의 평균화 반례를 분리한다. 표준 2/4-node 양수 quadrature와 microgrid가 첫 강한 대조다. compact feature의 독립 갱신이 없으면 아직 해법이 아니다.



새 문헌 R12–R13은 대표 상태/분포 closure의 기존성을 강화한다. 따라서 연구 질문은 ‘어떤 통계를 추가할까’에서 **현재 반응·점유·경로에 필요한 정보를 어떤 저비용 갱신으로 유지할까, 표준법보다 왜 필요한가**로 좁힌다. 로컬 실행은 계속 중지한다.



#### 심화 추론 이후의 우선 판별



[[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#source-analysis-11-contribution-proof-obligations|심화 추론 §9]]의 H3-E/H3-C/H2-P를 따른다. 방향3은 histogram만으로 빠지는 **내부 이동에 따른 유입 전환**을 추가로 다룬다. 먼저 고정 fine 규칙을 보존할 제작 필요와, 표준 microgrid/graph/fine Sparse로 충분하지 않은 이유가 있어야 한다. 방향2는 보존량/수지로 불가능한 요구를 제외한 뒤, 유한 event를 넘는 보호 편집을 표준 optimizer와 비교한다. 세 방향 모두 실행 증거는 추가되지 않았다.



#### 실행 없는 연구에서 확보한 다음 출발점



[[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#source-analysis-12-exact-submodels-and-limits|최소 모델]]의 두 texel exact closure를 **실험 재개 이후의 첫 sanity reference**로 쓸 수 있다. 아직 실행/구현하지 않는다. 범용화를 위해서는 외부 입력을 받을 signed 내부 상태, port별 장부, 잔류의 위치와 Geometry 재구성, 폐기한 정보의 복원 비용이 필요하다. 잃어버린 fine 정보를 refinement만으로 복구할 수 있다고 가정하지 않는다.



‘모든 미래를 보존하는 작은 모델’은 path observability 예제에서 반박된다. 대신 관측·입력 범위·시간·허용오차를 먼저 고정한다. 후보 A/B/C의 알고리즘 공백·강한 Baseline·기각 조건은 [[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#source-analysis-13-method-blueprints-and-decision|방법 명세]] §6–8을 따른다. 현재 실행 허가는 없고, 새 PERF 결과도 없다.



#### 공동 오차 분석 이후의 다음 이론 과제



[[90_Research-Archive/Research-Notes-2026-10/03_Solver/03_Error-Control-and-Sparse#source-analysis-15-joint-state-error-and-geometry-bounds|15]]에서 한 Step의 물·잔류 공동 오차와 고정 계수 전파 상한을 유도하고, 정적 LS 계수로 Normal 민감도를 사전 계산하는 가능성을 확인했다. 다음은 cavity/Decay OFF·단일 Incoming 전환·제한된 거리/중력 margin 조건에서 **State 범위→높이/Normal 범위→계수 차이→오차 장부**를 끝까지 연결하는 일이다.



원래 fine flux를 먼저 계산하지 않고 상한을 얻어야 한다. block 범위가 너무 넓어 생략을 거의 허용하지 않거나, 판정이 fine 계산만큼 비싸면 이 설계는 중단/수정한다. 기준은 잔류 총량만이 아니라 물/잔류 공간 분포와 필요한 경계 관측이다. 고정 주기·높이 threshold뿐 아니라 같은 오차 정보와 사전 계수를 허용한 기존 적응형 대조를 상대한다. **문헌·수식 연구의 다음 단계이며 실행 재개 지시가 아니다.**



16에서 [[90_Research-Archive/Research-Notes-2026-10/03_Solver/03_Error-Control-and-Sparse#source-analysis-16-low-cost-certificates-and-sleep-horizons|공통 하한·α-bin 기간 판정]]이라는 대안을 구성했다. 제한 모델에서는 Geometry의 미분 상수 Γ 대신 전체 형상/이동량 범위로 충분조건을 만들 수 있다. 다음 비실행 질문은 **그 범위가 너무 넓어지는 조건을 얼마나 줄일 수 있는가**이다. 특히 열린 경계의 gross 유출입, 높은 해상도에서 α=1로 퇴화하는 조건, 기존 cap 생략과 중복하지 않는 실제 절감 대상을 구분한다. 유용한 기간이 0–1 Step뿐이거나 범위 유지에 fine 갱신만큼의 계산이 들면 추진하지 않는다. 기존 self-triggered/적응형 방법에도 같은 사전 정보와 요약을 허용해야 한다.



#### 열린 경계 분석 이후의 범위 축소



[[90_Research-Archive/Research-Notes-2026-10/03_Solver/03_Error-Control-and-Sparse#source-analysis-17-open-boundary-contracts-and-composition|17]]의 단순 α-bin block 합성은 같은 조건의 전체 영역 bound보다 느슨해 주력 구현안에서 보류한다. 다음 이론 과제는 **근사 때문에 달라진 경계 응답만 제한하면서, 내부 fine 갱신을 피하는 충분조건**이다.



먼저 같은 boundary State/raw에서 내부 outgoing 합 범위 [L,U]가 source limiter와 실제 경계 Flux에 미치는 영향을 사용한다. limiter 비활성 인증 M≥S+U일 때 내부 합의 정확한 값은 불필요하다. 이어서 물질 port와 별개인 Geometry stencil 영향을 포함해야 한다. 외부에서 필요한 높이/Normal/Concavity 범위와 계산 시점, 내부 합 범위를 fine 계산보다 싸게 유지할 수 없으면 이 후보도 중단/축소한다. 표준 microgrid·port reduction·적응형 오차법에 같은 정보/branch 판정을 허용하는 대조가 필요하다. **실행 재개 지시는 없으며 새 PERF 결과도 없다.**



#### 최신 A/B 과제 — 제한 모델의 가능성에서 실제 조작 범위로



[[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-21-constructive-controls-and-reaction-contracts|21]]에서 A의 양수 입력·동일 endpoint·조건부 동일 총 입력의 해를 구성했다. 다음 비실행 질문은 **그 연속 입력을 현재 contact 위치·event 시점·양으로 표현할 수 있는가**, 그리고 공간 수송/Geometry를 복원하면 보호 조건이 무엇을 더 요구하는가다. analytic seed+표준 constrained optimizer를 강한 대조로 삼는다. 국소 모델에서 만든 해를 production 성공으로 집계하지 않는다.



B에서는 현재의 ‘유입에 비례한 요청’과 제안된 ‘신규분만 소비’를 구분했다. 비교할 모델 의미를 먼저 고정하고 표준 자원 배분·경쟁 kinetics·순 flux 상쇄가 해결하는 범위를 제외한다. rule 추가 후 모든 기존 결과가 그대로여야 한다는 불가능한 조건 대신, 공유 자원의 경쟁과 결과 변화가 예측 가능한지 평가한다. **실험 재개나 제품 모델 변경을 지시하는 문서가 아니다.**



#### 편집 가능 영역과 Geometry 경로의 판별



[[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-22-protected-editability-and-geometry-mediation|22]]에 따라 실제 contact 조작이 추가만 허용하는지 기존 사건의 양·시간·위치를 재배치할 수 있는지, 독립 입력 수와 보호 조건 수를 먼저 고정한다. 같은 총 입력·항상 빠른 융해·형상 경로 완전 보호 같은 조건이 국소 모델에서 충돌할 수 있으므로, 최적화 실패 사례로 만들기 전에 배제한다.



실험 재개 시에는 기준 입력에서 같은 궤적을 갖는 **시간별 Geometry 재생 대조**를 고려한다. 기존의 한 시점 Geometry 고정 E와 목적을 구분한다. full coupling이 보호 편집의 가능 범위를 넓히는지, 좁히는지, 변화시키지 않는지 검사한다. 이 대조와 같은 입력·초기값·계산 예산의 표준 최적화 비교가 없으면 A의 새로운 방법 기여를 인정하지 않는다. 현재는 설계/추론이며 실행 허가가 아니다.



#### 실제 입력 basis에 맞춘 A의 최소 과제



[[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-23-contact-control-space-audit|23]]의 `b_heated` Heat Add 48개를 기준 입력으로 삼을 수 있다. 우선 두 사건의 strength를 `±ε`로 바꾸어 명목상 texel별 총 요청 입력을 보존하는 **입력 공간 대조**를 정의한다. 이것이 최종 Ice/Heat/Water 또는 후속 수송을 보존한다는 가설은 세우지 않는다. 다음은 event strength의 제한된 집합만 조정해 중간 목표와 최종/이웃 보호 지표를 동시에 만족하는지 판정하는 과제다. 다른 instance의 사건을 보상 변수로 섞어 쉬운 성공을 만들지 않는다. 필요하면 이후에 위치·시간 조정까지 허용하되 변경된 조작 범위를 명시한다. 현재 실험 실행은 중지 상태다.

---

## source-18-research-landscape-and-selection

원본: `18_Research-Landscape-and-Selection.md`

### 연구 방향의 확장 비교와 선택

작성: **2026-10-28 / Asia/Seoul**. 사용자 요청: ‘이번엔 여러 방향으로 연구 방향을 생각하고 정리해봐’.

**현재 구현의 신규 Contribution은 여전히 미입증이다.** 이 문서는 연구 목적을 넓혀 비교한 제안이며, 실험 결과가 아니다. 앱·GPU·빌드·벤치마크·학습·수치 실험을 실행하지 않았고 제품 코드도 변경하지 않았다. 기존 실행 증거는 [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-02-evidence-and-current-judgment|02]], 계산 계약은 [[90_Research-Archive/Research-Notes-2026-10/03_Solver/01_Forward-and-Reaction-Semantics#source-solver-10-forward-model-and-controls|10]]을 따른다.

#### 1. 이번에 바꾸는 관점

최근 14–17은 방향3의 계산 생략과 경계 오차에 집중했다. 이 문제는 구체적이지만, **MDSSP의 연구 전체가 이 하위 문제의 성공에 달린 것은 아니다.** 사용자가 만들려는 것은 제어 가능한 현상학적 표면 상태 시스템이다. 다음 세 질문을 분리한다.

- 제작자는 무엇을 더 잘 지정·수정·재사용할 수 있는가?
- 그 약속을 지키려면 어떤 상태·반응·형상 정보를 계산 모델에 남겨야 하는가?
- 그 모델을 어느 비용으로 실행할 수 있는가?

물리적 정확성은 선택한 모델의 목적에 따라 평가한다. 예술적 모델에서도 수지, 비음수성, 조작의 의미, 해상도 변화에 대한 약속은 명시해야 한다. 반대로 모든 현실 현상을 정확히 재현하거나 모든 mesh에서 같은 픽셀 결과를 만들 필요는 없다.

아래 A–G는 이번 비교용 문자다. 사용자가 제외한 **기존 방향4인 축적 순서/층 표현은 포함하지 않는다.**

#### 2. 한눈에 비교

‘우선도’는 **다음에 판별할 가치**이며 SIGGRAPH 채택 확률이나 확보된 신규성 점수가 아니다. 모든 새 방법은 미구현·미검증이다.

| 방향 | 사용자가 얻을 결과 / 연구 질문 | 기존 검토와의 관계 | 기여 유형 | 현재 근거 | 탐색 우선도 / 주된 위험 |
|---|---|---|---|---|---|
| A. 다른 현상의 진행을 보호하는 편집 | 물길을 바꾸면서 지정한 냉각·융해·외관 진행을 유지할 수 있는가? | 기존 방향1+2의 중심 | 제작 방법·알고리즘 | cap/threshold/수지 분석, 기존 결합 구현 | **주력 후보**. 표준 제약 최적화로 충분할 위험 |
| B. 해상도가 달라도 의미를 유지하는 재질 규칙 | texel 개수가 달라져도 같은 ‘스며드는 정도·축적·반응’ 설정이 같은 뜻을 갖는가? | 격자 의존성을 독립 연구 질문으로 승격한 새 분기 | 모델·수치 방법 | 유입당 전환 코드, 경로 분할의 대수적 반례 | **새 후보 중 먼저 판별**. 표준 단위/거리 보정으로 끝날 위험 |
| C. 동작을 지정해 다른 자산에 옮기는 현상 재사용 | 표면이 달라져도 지정한 반응 순서·배출 비율·진행 시간을 재사용할 수 있는가? | 기존 ‘규칙 복사’보다 요구를 좁힌 새 분기 | 제작 시스템·역설계 | 선언 Profile·기전 실험, 일반화 증거는 제한 | **조건부 후보**. 일반 retargeting/최적화와 중복 |
| D. 짧은 자극으로 미래 반응을 구분하는 상태 추정 | 현재 외관이 같은 재질 중 후속 입력에 맞게 반응할 상태를 어떻게 식별하는가? | 방향1의 관측 모호성에서 나온 새 분기 | 역문제·능동 관측 | cap 위 저장량의 모호성 분석 | **조건부 후보**. 엔진이 이미 상태를 아는 과제라면 필요 없음 |
| E. 미래 반응을 유지하는 시뮬레이션 LOD | 현재 외관뿐 아니라 다음 상호작용에 필요한 정보까지 적은 계산으로 유지할 수 있는가? | 기존 방향3, 경계 계약 포함 | 모델 축소·알고리즘 | 평균/경로 반례, 조건부 bound·17의 실패 결과 | **독립 보조 축**. closure·경계 비용과 기존 ROM이 강함 |
| F. 결합 현상의 원인과 수정 가능성 진단 | 왜 의도한 변화가 안 생겼고, 무엇을 바꿔야 하는가? | 보호 편집의 진단 기능을 별도 검토 | 분석·제작 도구 | 수지 불가능 조건, limiter·Geometry 의존성 | **A의 보조로 우선**. 로그 시각화·일반 counterfactual 최적화 수준 위험 |
| G. 불확실한 Meso 형상에서도 일관된 연출 | Normal Map 복원의 불확실성이 결과를 얼마나 바꾸며 어떤 규칙을 신뢰할 수 있는가? | 기존 meso 후보의 목표를 복원 정확도에서 동작 강건성으로 변경 | 강건 모델·불확실성 분석 | 복원/수송 코드, PERF-043은 필요성에 약한 근거 | **보류**. 실제 영향이 작거나 표준 복원으로 충분할 가능성 |

현재 가장 먼저 검토할 두 문제는 **A의 제작 목적**과 **B의 모델 의미**다. E는 수학적으로 가장 많이 분석했지만 그 이유만으로 1순위를 유지하지 않는다. 특히 단순 α-bin block 합성은 [[90_Research-Archive/Research-Notes-2026-10/03_Solver/03_Error-Control-and-Sparse#source-analysis-17-open-boundary-contracts-and-composition|17]]에서 보류했다.

#### 3. A — 다른 현상의 진행을 보호하는 편집

**과제:** 사용자가 Heat 입력의 위치·세기·시간을 조절해 융해 패턴을 바꾸되, 다른 영역의 WaterFilm 경로와 냉각 진행을 허용 범위 안에 둔다. 모든 State를 원하는 값으로 직접 덮어쓰는 조작은 이 과제의 허용 입력과 다르다.

중요한 것은 최종 이미지 한 장의 일치가 아니라 **후속 반응의 선택적 수정**이다. 같은 공유 형상을 쓰므로 수정 영역 바깥도 영향을 받을 수 있고, cap/threshold 때문에 작은 미분이 0이어도 유한한 수정은 가능할 수 있다. 반대로 수지 때문에 어떤 보호 요구는 본질적으로 불가능하다. 이 사실들은 기존 문서의 조건부 분석이며 실제 제작 실패율을 측정한 결과가 아니다.

제안하는 연구 범위는 짧은 horizon, 기존 Ice–WaterFilm–Heat, 제한된 입력 basis, 두 개 이하의 보호 관측으로 시작한다. 단순히 loss에 보호항을 더하는 것은 Contribution이 아니다. 후보 방법은 **수지 기반 불가능 조건 → threshold/cap 전환을 포함한 후보 생성 → 실제 forward에서 보호 오차 확인**의 결합이다. 각 부분이 알려진 방법이므로 이 구조가 필요한 과제군과 비용 이점을 입증해야 한다.

- **선행/강한 대조:** R2의 adjoint control, R4의 국소 유체 편집, R5의 제약 편집. 동일 조작·수지 사전 검사·multi-start를 허용한 SQP/FD 또는 AD, derivative-free 대조.
- **판정 지표:** 목표 오차–최악 보호 오차의 Pareto 관계, feasible 해 발견률, forward 호출 수, event 경계에서의 실패, 실제 재생 비용. 사람의 생산성은 별도 제작 과제 증거가 있을 때만 주장한다.
- **기각:** 표준 optimizer에 같은 정보·입력을 주면 비슷한 품질과 비용으로 해결되거나, 좋은 결과가 직접 State 조작에서만 가능하면 특화 알고리즘 주장을 접는다.
- **현재 판단:** 프로젝트 목적과 가장 잘 맞지만 새로운 해결책의 필요성은 아직 미입증이다. ‘물길 설계’나 ‘진행 속도 역설계’는 초기 응용 과제이며 서로 독립된 신규 Contribution으로 세지 않는다.

#### 4. B — 해상도가 달라도 의미를 유지하는 재질 규칙

##### 4.1 왜 새로 살펴볼 만한가

[reactionRequest](../../../../../Shaders/Simulation/Solver/SurfaceReaction.comp)의 inflow 분기는 positive Input·Incoming에 `fraction`을 곱한다. [Pass2](../../../../../Shaders/Simulation/Solver/SurfaceSolverPass2.comp)는 이웃별 유입을 합쳐 Incoming scratch에 기록한다. 면적 기반 Capacity가 있어도 **이동할 때마다 일어나는 전환의 횟수**까지 보정되는 것은 아니다.

한 줄 경로에서 물량 Q가 n번의 수송/전환을 차례로 거치고, 다른 Input·Decay·반응·경쟁·유출 분기가 없으며 각 단계에서 남은 물이 다음 지점으로 모두 이동한다고 가정하자. 일정 fraction f이면 최종 물량은

$$
Q_n=Q(1-f)^n.
$$

같은 경로를 두 배로 세분화해도 같은 f를 쓰면 Q(1-f)^(2n)이 된다. 0<f<1에서 결과가 다르다. **이는 제한된 수송/전환 연쇄의 대수적 사실이며, 현재 GPU 장면의 해상도 오차 수치나 수렴 실패를 실행으로 확인한 것이 아니다.** 이동 시간·부분 이동·동적 흐름은 이 식에 포함하지 않았다.

따라서 ‘재질 계수가 같은 뜻을 유지한다’는 주장은 무엇을 기준으로 하는지 먼저 정해야 한다. 다음 세 의미는 서로 다르다.

| 규칙 의미 | 전환의 기준 | 현재 모델과의 관계 |
|---|---|---|
| 접촉 사건당 전환 | 새 외부 Input 1회 | 기존 inflowFraction의 Input 선택으로 표현 가능 |
| 이동 거리당 전환 | 표면을 따라 이동한 거리 | 현재 Incoming 합×fraction만으로 일반 edge별 거리 가중을 직접 표현하지 않음 |
| 체류 시간당 전환 | 해당 재질과 접촉한 시간·양 | 기존 continuous reaction 계열이 출발점. 정지 상태에서도 진행하므로 사용자 의도와 확인 필요 |

##### 4.2 먼저 상대해야 하는 간단한 해법

거리가 ℓ인 이동에 f(ℓ)=1−exp(−κℓ)을 사용하면, 위 단일 경로의 도착 물량은

$$
Q\prod_e(1-f(\ell_e))=Q\exp\left(-\kappa\sum_e\ell_e\right).
$$

이는 익숙한 지수 감쇠의 합성 성질이다. **새 알고리즘이 아니다.** 경로 세분화와 무관한 것은 여기서는 최종 생존량뿐이다. 잔류의 공간 분포, 도달 시간, 다른 반응·공급 경쟁·State-Driven Geometry Coupling의 동등성은 따라오지 않는다.

코드에서 확인할 수 있는 추가 정보 문제도 있다. 유입 합 I만 같지만 edge 길이가 다른 두 상황에서는 Σ f(ℓ_e)F_e가 달라질 수 있다. 따라서 I 하나만 읽는 기존 request는 일반적인 거리별 전환량을 복원할 수 없다. **가중 유입 합 J를 gather에서 추가 계산하는 것은 가장 먼저 검토할 표준 확장**이다. 이것으로 충분하면 연구 알고리즘을 억지로 만들지 않는다. 외부 Input은 경로 길이 0이라고 소실시키지 말고 별도의 사건 규칙으로 다룬다.

##### 4.3 남을 수 있는 연구 문제

정확히 같은 물리 표면의 격자 분할만 바뀌는 경우를 먼저 다룬다. 다른 mesh 형상의 예술적 retargeting은 C다. 반응 이후 Geometry가 바뀌어 후속 경로도 바뀌므로, 단일 경로의 보정이 전체 과정의 일관성을 보장하지 않는다. 연구 후보는 **사건·거리·시간의 의미가 명시된 규칙을 비균일 UV graph·seam·다중 반응에 옮길 때, 지정된 동작 관측을 일관되게 유지하는 모델**이다.

이를 위해서는 edge metric·입력 적분·반응 경쟁·Geometry 갱신의 어느 부분에서 관측 오차가 발생하는지 분리해야 한다. 목표를 ‘기존 모든 fine-grid 세부 결과의 정확 보존’으로 잡지 않는다. B는 필요하면 규칙의 의미를 바꾸는 연구이며, 고정 fine 모델을 근사하는 E와 다르다. 변경 모델과 기존 모델의 차이를 속도 향상으로 숨기지 않는다.

- **선행/대조:** R3의 RD 해상도/mesh 전이, R9의 비척도불변 coarse 반응, R45의 비정규 mesh 반응률 연구, R51의 현상학적 수송·흡수 모델. 먼저 면적/단위 정리 + 거리 가중 유입 + 기존 continuous reaction + 적절한 dt를 비교한다.
- **주의:** R45는 분자 random-walk의 RDME다. 현재 `rawFlux`는 saturation 차이의 양의 부분을 쓰므로 균일 상태에서 그 항은 0이다. RDME의 양방향 gross jump·격자 극한을 그대로 MDSSP의 증명으로 가져오면 안 된다.
- **향후 지표:** 공통 표면 좌표로 환산한 물·잔류 분포, 반응 총량, 도달 시간, Geometry/경로 차이, 해상도·chart·방향별 추가 tuning 수. 먼저 시간 오차와 seam 누락 등 구현 결함을 분리한다.
- **현실적 범위:** 같은 평면/완만한 곡면, 단일 이동 전환 → 기존 연속 반응 하나 → Geometry ON 순. 모든 현상·임의 형상으로 시작하지 않는다.
- **기각:** 표준 거리 보정과 metric 정리로 관측 차이가 충분히 줄면 중요한 모델 개선이지만 독립 논문 알고리즘 주장은 낮춘다. 현재 실제 자산에서 문제가 중요하다는 증거도 아직 충분하지 않다.

**평가:** 새 방향 가운데 첫 판별 비용이 작고 핵심 목적과 가깝다. ‘유력한 신규 기여’로 평가를 올린 것이 아니라 ‘비교적 명확히 반증할 수 있는 새 질문’이다.

#### 5. C — 다른 자산으로 옮길 수 있는 동작 명세

**과제:** 판에서 만든 ‘입력 이후 융해가 시작되고, 물이 특정 출구로 흘러 냉각을 유발하는 효과’를 다른 표면에 옮긴다. 픽셀 복사나 파라미터 복사 대신, 사용자에게 중요한 동작을 먼저 지정한다.

제안 명세는 소수의 관측으로 제한한다. 예를 들어 지정 영역의 반응 개시 구간, 출구별 배출 비율, 후속 입력에 대한 변화 방향이다. 정확한 물길은 새 표면의 Geometry에 따라 달라도 된다. 반대로 중력·연결성 변화로 요구가 불가능해지면 이를 알려야 한다. **다른 물리 조건에서도 같은 결과를 강제로 내는 것을 재사용성으로 정의하지 않는다.**

방법 후보는 source 효과를 이러한 동작 명세로 변환하고, target에 허용한 소수 조절 변수만으로 맞추는 것이다. source/target 영역 대응과 관측 자체를 사람이 지정한다면 그 작업량도 포함한다. target마다 전용 규칙과 예외를 추가한다면 범용 retargeting 주장은 약해진다.

- **현재 활용 가능:** 선언 Profile, 반복 입력, State/Flux 장부, PERF-041의 결합 사례. 그러나 Bunny의 seam 문제와 제한된 Mud 효과 때문에 이미 일반화됐다고 말할 수 없다.
- **선행:** R1 Image-Guided Weathering, R3 RD 전이, R47 시간변화 외관의 transfer, R52 fluid carving. 이들은 재사용·전이 자체의 선례다. 반응/공유 형상에 대한 온라인 후속 응답이 본 제안의 비교 지점이며, 선행 방법이 이를 절대 지원하지 못한다고 단정하지 않는다.
- **강한 대조:** 단위/면적 보정된 profile 재사용, 수동/자동 parameter fit, 동일 관측과 변수의 표준 optimizer. 장면별 full fit도 비용 상한 대조로 포함한다.
- **향후 지표:** 미사용 자산에서 동작 명세 위반, 재조정 변수 수·범위, 대응 지정 비용, 적합 forward 횟수, 불가능 요구의 오판. 인간 편의성은 별도 평가한다.
- **기각:** 소수 전역 계수나 기존 이미지/시계열 적합으로 같은 결과를 쉽게 얻으면 새로운 시스템 주장을 낮춘다.

**평가:** 재사용이라는 기존 목표를 측정 가능한 과제로 바꾼다는 가치가 있다. 독립 solver 혁신보다는 제작 시스템 가능성을 검토하는 경로다. A와 동시에 크게 개발하기보다 A의 미사용 조건 평가로 먼저 사용한다.

#### 6. D — 미래 반응을 위한 상태 식별과 자극 선택

**과제:** 이미지나 부분 관측으로 초기 표면 상태를 가져온 뒤, 주어진 Heat/Water 입력에도 의도대로 반응하도록 만든다. **MDSSP 내부 State 배열을 이미 알고 있는 정상 재생에는 이 역문제가 없다.** 외관만 주어지는 asset 초기화·불완전 상태 import라는 사용 시나리오가 먼저 필요하다.

`accumulationHeight`는 저장량의 Capacity 초과분을 형상에 넣지 않는다. 따라서 형상이 같아도 이후 반응이 달라질 상태가 존재할 수 있다. 기존 4a/8a Ice 반례는 이 구조를 보여주지만, 실제 허용 초기 입력으로 생성된 이미지 쌍이나 센서 식별 성공을 입증하지 않았다.

제안은 한 장의 외관을 맞추는 데 그치지 않고, 가능한 상태 집합을 유지하면서 **허용된 짧은 자극 중 미래 예측을 가장 잘 구분할 자극**을 선택하는 것이다. 알고 싶은 것은 모든 숨은 값의 정확 복원이 아니라 지정된 미래 입력/관측 범위에서의 차이다.

다만 cap 아래로 충분히 내려가지 않고 외관 관측이 계속 포화하면 probe를 줘도 구분되지 않을 수 있다. 완전한 State 읽기를 몰래 관측으로 쓰면 과제가 무너진다. probe에 따른 표면 변경을 허용하는지, 복사한 가상 상태에서만 가능한지도 고정해야 한다. 실제 실험 비용·관측 잡음·복구 가능성이 모두 달라진다.

- **선행/대조:** R46 MATch의 역절차 재질, R47 시간 외관 모델, R48 ASID의 능동 시스템 식별, 기존 관측성/최적 실험 설계. 무작위·고정 probe, 동일 비용의 정보이득 기반 표준 probe, 다중 프레임 적합.
- **후보 차이:** cap/threshold·다중 반응 아래에서 제작 목표에 필요한 미래 응답만 식별한다는 제한. ‘능동 probe’ 자체는 신규성이 아니다.
- **향후 지표:** 미사용 입력에 대한 미래 분포 오차, 구분 불가능 상태의 정직한 범위, probe 수·변형량, 관측 종류별 성능.
- **기각:** 완전 상태 저장/불러오기로 실제 제작 문제가 사라지거나, 일반 정보이득 probe와 같으면 독립 연구 우선도를 낮춘다.

**평가:** A보다 과제 전환 폭이 크고 응용 수요가 불확실하다. 학술적으로는 흥미롭지만 현재 프로젝트에서 즉시 최우선으로 할 이유는 부족하다.

#### 7. E — 미래 상호작용을 위한 시뮬레이션 LOD

**과제:** 멀리 보이거나 한동안 입력이 없는 영역에서 계산을 줄이되, 다시 Heat/Water가 들어왔을 때 중요한 반응이 유지되게 한다. 실제 카메라 거리만 기준으로 삼겠다는 뜻은 아니다. 관측·입력 범위·horizon·오차를 먼저 정한다.

기존 11–17은 평균/전체 histogram만으로 내부 이동 반응을 정할 수 없고, 모든 미래를 정확히 보존하는 작은 표현도 일반적으로 불가능함을 제한 모델에서 분석했다. 또한 **물질이 건너가지 않아도 공유 Geometry가 외부 수송을 바꿀 수 있고, 내부 outgoing 합이 경계 limiter를 바꾼다.** 이는 새 모델이 무시할 수 없는 의존성이다.

후보는 필요한 내부 mode·반응 잠재량·경계 응답을 남기거나, fine 메모리는 유지한 채 일부 갱신만 생략하는 것이다. 현재 단순 α-bin block 합성은 전체 영역 bound보다 느슨하여 보류다. 이를 성공한 알고리즘으로 소개하지 않는다.

- **선행/대조:** R9–R13 closure, R21 lumping, R25 memory ROM, R27–R28 목적별 축소, R37–R44 적응형/경계/합성. fine Sparse, microgrid, 같은 정보의 표준 reduced model을 포함한다.
- **향후 지표:** 물·잔류의 공동 공간 오차, 경계 시계열, 재활성화 후 반응, Geometry 영향, 전체 메모리/연산 비용과 준비 비용. 총량만 맞추는 평가는 부족하다.
- **현실적 범위:** 단일 유입 전환, 짧은 horizon, 소수 port와 정해진 Geometry 범위. 한 번에 일반 다중 반응 전체를 인증하지 않는다.
- **기각:** 요약 유지가 fine 갱신만큼 비싸거나, 유용한 생략 기간이 거의 없거나, 표준 microgrid/ROM이 더 좋으면 이 하위안을 중단한다.

**평가:** 가장 많은 분석 자산이 있지만 계산비용 이점과 신규성이 입증되지 않았다. 모델 의미 B를 바꾸면 E의 기존 반례/식 중 어느 것이 유지되는지 다시 감사해야 한다.

#### 8. F — 왜 결과가 바뀌는지와 어디까지 고칠 수 있는지

**과제:** ‘Heat를 더 넣었는데 원하는 물길이 생기지 않았다’는 실패에서 원인이 공급량 부족, 반응 임계값, 높이 cap, 형상 경로, limiter 중 무엇인지 설명하고 최소한의 허용 수정을 제안한다.

**수지 장부는 양의 출처를 설명하지만 인과 효과까지 자동으로 설명하지 않는다.** Reaction OFF 비교는 공급량과 Geometry를 동시에 바꿀 수 있다. 단순 차이를 특정 경로의 순수 효과라고 부르면 안 된다. 진단은 실제 solver replay에서 정의한 개입, 필요하면 중간 형상/반응을 고정한 인위적 개입을 구분해야 한다. 후자는 제작 가능한 조작이 아닐 수 있다.

새 연구가 되려면 dependency graph나 로그 화면을 넘어서, 지정 실패에 대한 **검증 가능한 수정/불가능 조건**을 제한 비용으로 얻어야 한다. A의 최적화에 실패 사유와 수정 가능한 제약 집합을 돌려주는 기능으로 먼저 좁힐 수 있다.

- **선행/대조:** R20의 상태 방정식·CEGAR, R29 hybrid verification, R49의 CPS counterfactual debugging. R49는 2026 preprint 초록 확인 수준이며 전체 증명·효과를 재검증하지 않았다.
- **향후 지표:** 실제 replay에서 수정 성공, 제시 원인의 개입 일치, 거짓 불가능 판정, 제약 완화량·탐색 비용. 설명의 이해도/제작 생산성은 사용자 평가 없이 주장하지 않는다.
- **기각:** brute-force ablation과 표준 제약 optimizer가 충분하면 도구 기능으로 유지하고 독립 기여로 세지 않는다.

**평가:** 독립 SIGGRAPH 기여의 근거는 약하다. A의 실패 해석과 반증 절차에는 실용적 가치가 크다.

#### 9. G — Meso 불확실성을 견디는 현상 규칙

**과제:** 하나의 Normal Map에 대해 복원 경계 조건·노이즈·기준 높이 선택이 달라도 지정 효과가 유지되는지 평가하고, 민감한 부분은 추가 정보나 보정을 요청한다. 현상학적 모델이라도 불확실성을 무시하고 유일한 물리 높이라고 주장하면 안 된다.

후보는 가능한 높이 집합에서 동작 관측의 범위를 보거나, 그 범위 안에서 유지되는 제어를 찾는 것이다. 임의로 넓은 집합을 만들면 항상 불확실하다고 나오므로 normal 잔차·경계·스케일의 허용 범위를 먼저 정의해야 한다. 같은 chart에 상수를 더했을 때 변하지 않는 양과 chart 사이 상대 높이를 구분한다.

- **선행/대조:** R50의 경계/불연속을 다루는 변분 normal integration, 표준 LS/Poisson·ensemble·강건 최적화. ‘Normal Map을 적분한다’는 신규성이 없다.
- **현재 반증 쪽 증거:** PERF-043에서 실제 brick의 Water 차이는 당시 시간 민감도보다 작았다. synthetic 복원 개선만으로 후속 수송의 중요한 실패를 주장하지 않는다.
- **향후 지표:** 정상적으로 허용한 복원 집합에서 관측 범위, 실제 입력에 대한 coverage, 추가 높이 제약의 필요량. height 오차만으로 시스템 이점을 평가하지 않는다.
- **기각:** 표준 복원으로 충분하거나 실제 효과가 작으면 보류를 유지한다. chart 대표 높이를 texel별로 바꾸는 선택 자체도 기여가 아니다.

**평가:** 독립 주력으로 추진할 현재 근거가 가장 약하다. B/C의 강건성 점검으로 포함할 수 있다.

#### 10. 조합과 선택 — 일곱 방향을 동시에 개발하지 않는다

| 연구 묶음 | 중심 주장 후보 | 함께 쓰는 항목 | 지금 제외할 범위 |
|---|---|---|---|
| 제작 중심 | 제한된 입력으로 원하는 변화를 만들며 지정한 다른 현상의 진행을 보호한다 | A 중심, F 진단, C를 미사용 조건 평가로 사용 | 완전 상태 복원 D, 범용 ROM E, 신규 물리 Solver |
| 모델 중심 | 사건·거리·시간의 의미가 명시된 규칙을 다른 discretization에서도 일관되게 적용한다 | B 중심, C의 재사용 평가, G의 필요한 강건성만 | 모든 fine 결과 보존, 물리적 부피 모델 강제 |
| 계산 중심 | 지정된 미래 입력/관측에 필요한 내부 정보만 유지해 계산을 줄인다 | E 중심, A의 보호 관측을 평가에 활용 가능 | 모든 미래의 exact closure, 전체 다중 반응 즉시 지원 |

현재 권고는 **A와 B의 가장 작은 질문을 우선 비교하고, E를 제한된 별도 탐색으로 남기는 것**이다. ‘A+B+E를 다 만들면 논문’이라는 계획은 권하지 않는다. 서로 다른 중심 주장과 검증 기준이기 때문이다.

실험 없이 다음에 할 수 있는 구체적 산출물은 다음과 같다.

1. **B:** Input/Incoming/continuous 규칙을 사건·거리·시간 의미로 대응시키고, 단일 경로 보정과 weighted gather가 해결하는 범위·못 하는 범위를 문서화한다. 현 모델을 바꾸기 전에 비교 기준을 고정한다.
2. **A:** 보호 관측 두 개와 허용 입력을 고정한 작은 과제에서 수지 불가능 조건·cap/threshold 후보를 정리한다. 일반 SQP가 실패할 것이라고 미리 가정하지 않는다.
3. **C/D/F/G:** 실제 사용 시나리오와 관측을 명확히 할 수 있는지 먼저 판별한다. 필요하지 않은 센서·UI·학습 데이터 수집을 선행하지 않는다.
4. **E:** 내부 raw 합/Geometry 경계 계약이 기존 방법보다 싼 충분조건이 되는지 제한 모델에서만 계속 검토한다. 17의 보류안을 반복 확장하지 않는다.

실험 재개는 별도다. 이후 실행할 때도 한 과제와 강한 표준 대조부터 시작한다. 새로 발견한 문제를 고쳤다는 사실과 학술적 기여를 구분한다.

#### 11. 근거와 최종 판정

- **코드 확인:** `reactionRequest`, `rawFlux`, Pass2 Incoming 합, `accumulationHeight`. 위 링크와 [[90_Research-Archive/Research-Notes-2026-10/03_Solver/01_Forward-and-Reaction-Semantics#source-solver-10-forward-model-and-controls|10]] 참조.
- **기존 실행:** PERF-041/042/043. 새 실행 없음. 기존 모듈식 대조의 부정 결과를 다른 과제의 부정 결과로 확대하지 않음.
- **새 유도:** 단일 경로의 분할 의존성, 거리 지수 보정의 제한된 합성 성질, aggregate Incoming의 거리 정보 손실. 새 수학 정리/범용 수렴 보장이라고 주장하지 않음.
- **문헌:** 기존 R1–R44와 이번 R45–R52. 접근 수준은 [[90_Research-Archive/Research-Notes-2026-10/04_Literature/01_Related-Work#source-reference-07-related-work|07]]의 해당 행에 기록. 초록만 본 논문에 대해 미지원 기능을 단정하지 않음.

**현재는 ‘현재 구현만으로 새 기여 확보’가 아니라, ‘기존 시스템을 토대로 서로 다른 연구 문제를 선택할 수 있으나 새 방법의 필요성과 이점을 판별해야 하는 단계’다.** A는 목적 적합성, B는 모델의 약속을 점검하는 명확성, E는 분석의 구체성이 장점이다. 셋 중 어떤 방향도 아직 SIGGRAPH 수준의 기여가 입증된 것은 아니다.

#### 12. 유망 후보 심화 이후

[[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-19-protected-editing-feasibility-and-response-coordinates|19]]는 A의 보호 관측을 좁혔다. 제한된 융해식에서 전체 Heat 진행을 고정하면 Ice 진행도 고정된다. 최종 Ice만 보호할 때는 연속 exposure 또는 실제 공급 비율/substep을 포함한 이산 반응 진행 좌표로 조건을 나타낼 수 있다. 중간 진행의 자유도가 남을 수 있지만 실제 U-input의 feasible witness는 없다. 같은 보호 조건을 받는 표준 constrained optimizer가 강한 대조다.

[[90_Research-Archive/Research-Notes-2026-10/03_Solver/01_Forward-and-Reaction-Semantics#source-solver-20-transport-reaction-semantics-and-refinement|20]]은 B에서 같은 순수송/다른 incoming 반응, 거리 보정 후에도 남는 네 이웃 모델의 방향 의존성, 두 경쟁 전환의 분할 비일관성을 유도했다. 동시에 표준 병렬 kinetics와 gradient absorption/Hamilton–Jacobi 모델이 대안임을 확인했다. **B의 문제 구체성은 강화됐으나 표준 방법 이상의 해결책 필요성은 아직 미입증**이다. 앱·GPU·수치 실험을 하지 않았으며 A/B의 우선 판별 권고는 유지한다.

---

## source-reference-00-overview-historical-notes

원본: `90_Reference/00_Overview-Historical-Notes.md`

### 이전 연구 개요 보존본

2026-10-28까지 누적된 긴 개요를 기록용으로 보존했다. 최신 판단과 읽는 순서는 [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-00-overview|현재 연구 안내]]를 따른다.

### MDSSP Research

작성: **2026-10-27**, 마지막 갱신: **2026-10-28 / Asia/Seoul**

이 폴더는 MDSSP의 **연구 목적, 대화에서의 판단 변화, 기여 후보, 문헌 근거, 미검증 가설과 후속 연구**를 관리한다. 사용자의 ‘05번 리서치 폴더에 지금까지의 대화를 여러 문서로 기록한 뒤 연구를 진행’하라는 요청으로 만들었다.

#### 현재 결론과 작업 범위

- 현 MDSSP의 학술적 판정은 **기존 연구의 시스템 통합에 가깝고, 새로운 Contribution은 아직 미입증**이다.
- 연구 목표는 물리적으로 정확한 부피 모델보다 **제어 가능한 현상학적·예술적 점유 모델**에 가깝다. 물리 Solver로 바꾸는 것을 논문 통과 조건으로 가정하지 않는다.
- 새 연구는 **1 목표 진행·후속 반응 역설계 + 2 다른 현상을 보존하는 편집**을 한 축으로, **3 혼합·경로 정보를 유지하는 coarse 표현**을 별도 축으로 검토한다.
- [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-18-research-landscape-and-selection|18의 확장 비교]]는 일곱 연구 방향을 구분한다. 이후 19–22의 분석을 반영한 우선순위는 **보호 편집(A)**의 실제 입력 가능성, **재질 규칙(B)**의 의미 계약, 계산 축소(E)의 별도 제한 탐색이다. 기존 방향1·2·3의 분석을 폐기하는 것이 아니라 연구 목적별 우선순위를 넓혀 재평가한 것이다.
- 사용자가 **4 축적 순서/층 표현 방향은 제외**했다. 이를 후속 필수 기능으로 다시 넣지 않는다.
- **로컬 앱·GPU·빌드·벤치마크·학습 실행은 중지 상태**다. 이후의 ‘연구 진행’은 명시적 재개 지시 전까지 문헌·기존 코드/자료 읽기·수식 분석·문서 작성으로 수행한다.

#### 문서 지도

| 문서 | 내용 |
|---|---|
| [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-reference-01-discussion-history|대화의 논점과 판단 변화]] | 최초 요청·평가 원칙, 물리/예술 모델 선택, 잘못된 전제의 수정, Baseline 중단, 아이디어 선택과 실행 중지 |
| [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-02-evidence-and-current-judgment|확보한 증거와 현 판정]] | 완료 실험·한계·중단된 과제·구현/설계/제안 구분. 원자료의 정본 위치 |
| [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-03-contribution-candidates|Contribution 후보 장부]] | 후보별 실제 차이, 기대 기여, 반론, 현재 강도·추진 조건·제외 항목 |
| [[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-04-inverse-authoring|방향 1: 목표 진행·후속 반응 역설계]] | 조작 범위·관측 모호성·목적함수·대조·중단 기준 |
| [[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-05-protected-editing|방향 2: 다른 현상을 보존하는 편집]] | 제한된 조작·수정/보호 제약·Geometry 민감도·실행 가능성·범위 선택 |
| [[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#source-analysis-06-coarse-reactive-transport|방향 3: 혼합·경로 coarse 표현]] | 코드에서 유도한 평균화 반례, closure 후보와 한계, 반응/수송/결합의 분리 |
| [[90_Research-Archive/Research-Notes-2026-10/04_Literature/01_Related-Work#source-reference-07-related-work|후속 연구 문헌과 확인 수준]] | 역설계·편집·coarse 연구의 1차 출처, 실제 읽은 범위·접근 실패·미확인 사항 |
| [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-08-research-agenda|연구 질문·판정 순서]] | 현재 질문, 강한 Baseline, 최소 판별 과제, 진행/기각 조건, 실행 재개 전 필요한 고정 사항 |
| [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-reference-09-analytical-findings|후속 수식·구조 분석 노트]] | 폴더 정리 이후 수행한 연구. 유도 결과·가정·반례·알고리즘 초안과 미검증 구분 |
| [[90_Research-Archive/Research-Notes-2026-10/03_Solver/01_Forward-and-Reaction-Semantics#source-solver-10-forward-model-and-controls|공통 forward 모델·코드 근거]] | 세 방향이 공유하는 실제 연산 계약, 비매끄러운 지점, 재사용할 입력/계측과 기존 실행의 한계 |
| [[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#source-analysis-11-contribution-proof-obligations|Contribution을 위한 심화 추론]] | 내부 총 이동·유입 반응의 coarse 반례, hinge/cap 구조, 대표 상태의 존재와 한계, 보호 오차 하한·불가능 판정, 방법 초안과 입증 조건 |
| [[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#source-analysis-12-exact-submodels-and-limits|정확한 최소 모델과 불가능 조건]] | 두 texel의 독립 갱신·남은 잔류량 bound, memory 필요성, 정확 압축의 차원 하한, cap별 보호 제약·반응 단계 witness |
| [[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#source-analysis-13-method-blueprints-and-decision|방법 초안·돌파 지점·중단 조건]] | 내부 mode 기반 block, 보호 목적별 편집/축소, 제한적 인증의 실제 공백·강한 대조·현재 우선순위 |
| [[90_Research-Archive/Research-Notes-2026-10/03_Solver/03_Error-Control-and-Sparse#source-analysis-14-residual-budget-critical-review|미래 반응 오차와 계산 생략의 고찰]] | 열린 경계·중력 flux의 potential, cycle에서의 한계, 유한시간 budget, 누적량/실행 오차/전체 품질의 구분 |
| [[90_Research-Archive/Research-Notes-2026-10/03_Solver/03_Error-Control-and-Sparse#source-analysis-15-joint-state-error-and-geometry-bounds|물·잔류 공동 오차와 형상 민감도]] | limiter 재분배를 포함한 edge 삭제 상한, 유입 반응의 오차 증폭 반례, 정적 LS 계수 기반 Normal bound, 중력/Concavity 조건·저비용 판정 초안 |
| [[90_Research-Archive/Research-Notes-2026-10/03_Solver/03_Error-Control-and-Sparse#source-analysis-16-low-cost-certificates-and-sleep-horizons|저비용 판정과 계산 생략 기간]] | 공통 하한 기반 장기 공동 오차, 중력/limiter 포함 α 상한, 이동률 구간 질량·O(K) 기간 판정, 열린 경계/해상도 한계·기존 cap 최적화 확인 |
| [[90_Research-Archive/Research-Notes-2026-10/03_Solver/03_Error-Control-and-Sparse#source-analysis-17-open-boundary-contracts-and-composition|열린 경계 계약과 오차 합성]] | 공통 유입 cohort의 공동 오차·block 합성, 단순 합성의 열세, 내부 outgoing 합·Geometry 매개 경계 영향, branch별 충분조건 |
| [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-18-research-landscape-and-selection|연구 방향 확장 비교와 선택]] | 보호 편집·규칙의 해상도 일관성·동작 retargeting·미래 상태 식별·LOD·진단·Meso 강건성의 일곱 방향, 코드 기반 경로 분할/정보 손실 분석, 강한 대조·기각 조건·우선순위 |
| [[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-19-protected-editing-feasibility-and-response-coordinates|보호 편집 가능 범위]] | Heat 진행 보호의 불가능 조건, 반응 진행 좌표와 이산 공급 제한의 관계 |
| [[90_Research-Archive/Research-Notes-2026-10/03_Solver/01_Forward-and-Reaction-Semantics#source-solver-20-transport-reaction-semantics-and-refinement|수송·반응 의미]] | 유입과 순수송의 차이, 방향성, 경쟁 반응과 경로 분할의 제한 |
| [[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-21-constructive-controls-and-reaction-contracts|양수 입력·반응 계약]] | 제한 연속 모델의 입력 구성, 현재 inflowFraction의 요청·소비 계약 |
| [[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-22-protected-editability-and-geometry-mediation|편집 자유도·형상 매개 판정]] | 유한 접촉 입력의 가능 조건, 동일 예산의 한계, 시간별 Geometry 재생 대조 |
| [[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-23-contact-control-space-audit|실제 Contact 입력 공간 감사]] | 현 DemoAnim의 48개 Heat 사건, 예산 중립의 유한 입력 방향, Remove와 재배치의 의미 구분 |

12·13의 분석에서는 방향3의 **제한된 두 texel에서 독립 갱신과 미래 총 잔류 오차 상한**을 유도했다. 추가로 닫힌 graph의 saturation-only 수송·단일 전환·dt 조건에서는 동적 Geometry를 허용하는 미래 잔류/물 L1 bound도 유도했다. 외부 입력·다중 반응의 일반 해법과 실제 비용/신규성은 미검증이다. 방향2에는 cap별 보호 가능 범위와 반응 한 substep의 대수적 witness를 추가했다. 이것은 앱 실행이나 실제 U-input 실험 결과가 아니다. 세부 판단은 12·13을 따른다.

2026-10-28의 후속 고찰(14)은 외부 유입·중력/비대칭 flux를 허용하는 경로 potential과 유한시간 budget을 추가 유도했다. 무한시간에 양의 전환 cycle을 허용하면 bound가 전체 물량으로 퇴화한다. **잔류량 bound가 작다는 사실만으로 물/Geometry/다중 State의 계산 생략은 정당화되지 않는다.** 저비용 유지와 수송/경계 오차 보장이 남아 있으며 실행·신규성 검증은 추가되지 않았다.

같은 날 이어진 15의 분석은 **물·잔류의 공간 분포를 함께 제한하는 한 Step 오차**로 범위를 넓혔다. 일부 raw edge 삭제 시 limiter 재분배를 포함해 2V 상한을 유도했고, Incoming 전환은 총량 보존 상태에서도 실행 간 L1 오차를 키울 수 있음을 보였다. 현재 Normal 복원의 LS 행렬이 정적이라는 코드 근거로 사전 민감도 계산 가능성을 찾았다. 중력 정규화·동적 Concavity·다중 반응·오차 장부 비용은 별도 조건/미해결점이다. 기존 적응형 multiresolution/반응–확산 선례 R36–R38과 대조했으며 **신규성 평가는 상향하지 않았다.**

16의 후속 연구에서는 **공통으로 유지되는 물량 하한**을 이용해 매 Step 민감도를 다시 계산하지 않는 대안을 유도했다. 닫힌 saturation-only 영역은 기간에 무관한 2B, 중력/limiter를 허용하는 단일 유입 전환 영역은 초기 물량과 outgoing 비율 α로 유한 기간의 공동 오차를 제한한다. α 구간별 질량을 모으면 감사 이후 O(K) 기간 판정이 가능하다. 열린 block 경계·높은 해상도·다중 반응에서는 효과/보장이 제한되며, 현재 cap 위 높이 생략은 이미 구현돼 있어 새 기여에서 제외했다. **조건부 방법 구성의 진전이며 실행·신규성 입증은 아니다.**

17의 후속 연구는 공통 유입과 port-time 출력 이력을 포함하는 상한을 유도하고, 제한 모델에서 block 경계 오차 항을 상쇄했다. 그러나 **동일한 α/기간의 단순 block 합성은 전체 영역 상한보다 느슨해 주력안에서 보류**했다. 현재 코드에서 경계 Flux가 내부 outgoing 합의 영향을 받고, 물질 이동이 없어도 Geometry로 외부 수송이 변할 수 있음을 확인했다. 다음 후보는 이 두 의존성을 반영해 내부 갱신 생략의 충분조건을 판정하는 경계 계약이다. 미구현·미실험이며, R42–R44의 보존 장부/port reduction/compositional abstraction 선례 때문에 신규성은 아직 미입증이다.

#### 기록 원칙

19·20에서는 A의 반응 진행 보호와 B의 순수송/교환·방향·경쟁 반응 의미를 구체화했다. [[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-21-constructive-controls-and-reaction-contracts|21]]은 제한 연속 모델에서 같은 최종 상태·총 입력을 유지하며 진행을 바꾸는 입력의 존재를 구성하고, 현 inflowFraction의 전체 보유량 제한을 확인했다. [[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-22-protected-editability-and-geometry-mediation|22]]는 양수 입력과 추가만 허용하는 편집을 구분하고, 유한 contact basis의 국소 자유도 및 시간별 Geometry 재생 대조를 제안했다. 최신 [[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-23-contact-control-space-audit|23]]은 현 `b_heated`의 48개 Heat 사건에서 예산 중립의 **명목상 입력 방향**을 코드·자산으로 확인했다. **최종 결과 보호·새 해법의 신규성 평가는 여전히 미입증**이다. 현재 Research는 24개 문서이며 실험·제품 코드 변경은 없다.

이것은 **대화 전체의 축어록이 아니라, 현재 대화와 연결된 저장소 증거를 재구성한 주제별 기록**이다. 전달된 다른 에이전트의 답변은 인용된 의견으로 취급하며, 실제 코드/문헌/실험으로 확인한 내용과 분리한다. 보이지 않는 과거 발언·정확한 발언 시각을 만들어내지 않는다.

판정 표시는 **[U] 사용자 요청/선택, [C] 코드 확인, [E] 실행 증거, [P] 문헌 확인, [D] 수식/구조 분석, [H] 제안/미검증 가설**로 구분한다. ‘좋아 보임’은 신규성의 증거가 아니며, 선행연구에 없는 것으로 확인하지 못한 기능을 불가능하다고 쓰지 않는다.

연구 정리는 이 폴더에서 갱신한다. 구현 계약은 [[02_Architecture/03_System-Map|Architecture]], 실제 검증은 [[05_Experiments/01_Validation-and-Tests|Validation]], 성능 실험은 [[05_Experiments/02_Performance-Measurements|Performance]]가 정본이다. raw를 이 폴더로 복사하거나 실험마다 별도 결과 설명 문서를 만들지 않는다.

`03_Development/03–07`은 이전 시점의 명세·분석과 결과 연결을 보존한다. 특히 [[04_Development/08_Research-Directions-123|1·2·3 방향의 최초 통합 검토]]는 역사적 snapshot이며, 이 폴더의 분리 문서가 후속 연구의 정본이다.

---

## source-reference-01-discussion-history

원본: `90_Reference/01_Discussion-History.md`

### 대화의 논점과 판단 변화

작성: 2026-10-27. 축어록이 아닌 주제별·논리적 순서의 재구성이다. 단계별 실행 시각은 각 manifest를 따른다.

#### 1. 최초 연구 검토 요청

[U] 사용자는 MDSSP를 SIGGRAPH Technical Paper 수준으로 발전시킬 수 있는지, 제시한 Related Work/Contribution 평가를 참고해 **소스·설계·실험을 직접 분석하고 비판적으로 판단**하도록 요청했다. 처음에는 분석만 요청했고 이후 실험·후속 작업을 순차적으로 승인했다.

초기 질문은 (Q1) 현재 해결한 특수한 계산 문제, (Q2) 기존 방법의 조합을 넘는 설계 선택, (Q3) 현재 구조에서 발생하는 후속 연구 질문이었다. GPU 사용·여러 State·Normal Map 적분·Cavity Filling·Sparse 처리 자체를 신규성으로 가정하지 않고, 구현/설계/새 제안을 구분하며 근거와 불확실성을 밝히도록 요구했다.

원한 결과는 후보 최대 5개, 유망 후보 1–3개의 심층 분석, 강한 Reject 사유, 최종 ① 현 구현의 유의미한 기여 / ② 새 해결책 필요 / ③ 시스템 통합 판정이었다. 현재 기록의 판정은 ③이며, 후속 질문이 ②로 발전할 가능성을 별도로 검토한다.

지정 논문: Günther 2012, Chen 2005, Lo 2007, Vantzos 2018, Azencot 2015, Su 2023, Jain 2024. 후속 직접 대조는 [[04_Development/06_Prior-Model-Differentiation|기존 모델 분석]]을 따른다.

#### 2. 연구 목적과 명칭의 정리

[U] 사용자는 물리적 부피 모델보다 예술적 점유 모델이 목적에 가깝다고 설명했고, 물리적 모델이어야 논문에 유리한지 질문했다. 이후 논의는 **제어 가능한 현상학적 표면 상태 모델**을 우선 대상으로 삼았다. 물리 정밀도를 높이면 신규성이 자동 생긴다는 전제는 채택하지 않았다.

[U] 차트별 대표 높이는 큰 높이에서의 시각적 결함을 줄이기 위한 선택이었으며, texel별 높이로 회귀할 수 있다고 밝혔다. 따라서 Chart-independent Cavity Filling을 억지로 독립 Contribution으로 키우지 않는다. 렌더 전용 보정인지 후속 수송의 입력인지에 대한 계산 계약은 명확히 해야 한다.

[U/논의 반영] 명칭은 **State-Driven Geometry Coupling**을 사용한다. State가 형상을 바꾸고 갱신된 형상이 후속 State 수송에 참여하는 구조를 뜻한다. ‘Geometry Feedback’ 또는 단순 외관 변화와 혼동하지 않는다.

#### 3. 초기 설명에서 바로잡은 전제

- **Reaction 미구현이라는 전제는 폐기:** 현행 Profile v5·공급량 경쟁·내부 시간 세분화가 구현돼 있다 [C].
- **적층 때 texel 면적이 재계산된다는 전제는 폐기:** 기준 면적은 macro footprint다. 그 전제를 바탕으로 제기한 자동 물질량 변화 우려를 현 구현의 직접 결함으로 사용하지 않는다 [C].
- **새 수치 Solver가 논문에 반드시 필요하다는 전제는 폐기:** 시스템 기여도 가능하되 학술적으로 실질적인 차이가 필요하다. 시스템 논문 허용은 현 MDSSP의 기여 인정과 다르다.
- **Günther를 독립 효과들의 모음으로 축소하지 않음:** 다중 재료·재사용 규칙·양에서 높이/Normal 생성·후속 입자 응답의 선례를 확인했다 [P]. 실제 OptiX 충돌 mesh를 매 Step 재구축하는지까지는 확인하지 못했다.
- **버그 수정과 Contribution을 구분:** Pass 간 형상 시점 의심, cache/Sparse 차이, GPU 자원 수명 문제는 정확성 검토 대상이다. 이를 발견·수정했다고 신규 알고리즘이 되는 것은 아니다.

#### 4. 시스템 기여 가설과 공정한 비교

초기 주제는 “여러 현상을 처리한다”에서 **“공통 모델이 복합 현상의 구성·제어 부담을 줄이고 규칙을 재사용하게 하는가?”**로 좁혀졌다.

[U/논의 반영] 합리적인 모듈식 Baseline은 공유 Geometry·Reaction 인터페이스와 자원 재사용을 허용해야 한다. 억지 중복 코드나 불편한 workflow로 MDSSP의 우위를 만들지 않는다. 반대로 같은 공통 Solver/선언을 쓰는 구현으로 수렴하면 그 비교가 무엇을 식별하는지 제한한다.

Ice–Waterfilm–Heat가 대표 사례로 선택됐다. Reaction OFF/ON × Coupling OFF/ON과 초기 축적 직후 형상 고정 조건 E 등을 비교했다. Ice와 Waterfilm 모두 형상에 기여하므로 ‘녹으면 높이가 감소하고 물길이 열린다’를 정답으로 고정하지 않았다. 실제 수송 변화의 방향을 관측한다.

PERF-041/042의 결과로 제한된 Coupling 기전은 확인했지만, 모듈식 대안 대비 확장 부담 우위는 지지되지 않았다. 상세한 완료/미실시 범위는 [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-02-evidence-and-current-judgment|증거 문서]]를 따른다.

#### 5. Meso·제작 과제와 실행 준비의 경과

[U] 실험 실행·결과 분석과 공정한 Baseline 명세의 후속 작업을 승인했다. 성능 개발 수치를 연구 우수성으로 포장하지 않는 원칙은 유지했다.

PERF-043은 표준 LS 대비 Meso 복원과 후속 수송을 비교했다. 현재 제한된 조건에서 새 복원 알고리즘의 필요성은 확인하지 못했다. 이후 제작 과제의 파일 저장·reload·잔류 흔적 외관·재사용 workflow를 점검했다. `wetness`라는 State 등록과 `absorbedwetness`의 기본 외관 binding을 구분했다.

앱은 한때 Smart App Control 오류 4551로 차단됐다. [U] 사용자가 직접 해제했다고 알리고 재시도를 요청했다. 에이전트가 OS 정책을 바꾼 것이 아니다. 재시도 후 발견한 descriptor layout 수명 오류를 수정·검증했고 이전 실패 기록은 보존했다.

현재 U/M은 같은 Profile workflow를 사용하며 M에 별도 제작 UI가 없다. 그래서 backend 선택만 바꾸는 인간 작업 시간 비교는 No-Go다. AI 보조 수행 시간을 인간 생산성으로 취급하지 않는다.

#### 6. 마지막 제작 pilot과 로컬 실행 중지

AUTH-GOAL-01은 입력을 4초에 종료하고 16초까지 부분 융해·영역 밖 수송·잔류 흔적·재실행을 확인하는 과제다. 목표는 실행 전 정했지만 이전 결과를 알고 설계했으므로 맹검/독립 사전등록이라고 하지 않는다.

같은 Step의 화면/State/Geometry 계측을 추가했다. 기존 정상 6초 장면과 최종 배열 3개가 비트 일치했다. 첫 후보는 빈 animation track 준비 오류를 보정한 뒤 실행했으나, [U] 사용자가 컴퓨터를 사용하기 위해 중단을 요청했다. **2026-10-27 18:35:06 KST** 해당 앱만 종료했다. 0/1/2/4/6/8/10초 자료가 남고 12/14/16초·재실행은 없어 과제 결과는 미판정이다.

이후의 ‘연구 진행’ 요청을 로컬 실행 재개 승인으로 간주하지 않는다. 새 로컬 앱·GPU·빌드·학습 실행은 명시적 재개 지시 전 하지 않는다. 문헌·코드·수식 분석과 기록은 계속한다.

#### 7. 새 방향의 제안과 사용자 선택

사용자는 기존 방향을 타개할 새 아이디어를 요청했다. 제안은 (1) 목표 진행/후속 반응 역설계, (2) 다른 현상을 보존하는 국소 수정, (3) 혼합/경로 정보를 유지하는 coarse 표현, (4) 축적 순서/층 표현이었다.

[U] **4번은 제외하고 1·2·3번을 진지하게 연구**하도록 선택했다. 코드·문헌·수식 검토 후, 1을 2의 목표/평가 체계로 결합하고 3을 별도 알고리즘 탐색 축으로 두도록 권고했다. 평균량이 같은 혼합/분리의 다른 초기 냉각률은 코드 식에서 유도했지만, 알려진 closure 문제이고 새 해법은 아니다.

[U] 마지막으로 이 대화를 새 Research 폴더의 여러 Markdown 문서로 기록한 뒤 연구를 이어가도록 요청했다. 이 폴더가 그 기록이며 이후의 분석은 [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-reference-09-analytical-findings|연구 노트]]에 연결한다.

#### 8. 계속 유지하는 사용자 요구

한국어로 소통한다. 독창성을 과장하지 않는다. 현재 코드·문서·테스트를 기준으로 하고 다른 에이전트의 보고만을 사실로 확정하지 않는다. 실패·미실행·미지원·중단을 구분한다. 현재 목적과 연결되지 않는 새 기능을 무작정 추가하지 않는다. 연구 결과의 채택 여부와 관계없이 기존 프로젝트의 구현 가치는 별도로 인정한다.

---

## source-reference-09-analytical-findings

원본: `90_Reference/09_Analytical-Findings.md`

### 후속 분석 — 관측 모호성·임계값 편집·coarse 정보 손실

작성: **2026-10-27 / Asia/Seoul**. 제목의 후속 분석은 `05_Research` 정리 후 수행한 코드·문헌·수식 연구를 뜻한다. **새 앱/GPU/빌드/학습/수치 실험은 실행하지 않았다.** 아래 수치는 현 코드 식에 대입한 유리수·산술 결과이며 PERF 실측이 아니다. 코드 버전은 HEAD `880af1ebcffe601f0bac27413cad94055ce92812`와 기존 미커밋 작업 트리다. 이번 작업에서 제품 코드는 바꾸지 않았다.

증거 표시는 [[90_Research-Archive/Research-Notes-2026-10/01_Assessment/01_Judgment-and-History#source-00-overview|Research 기록 원칙]]을 따른다. [D] 반례는 문제를 구체화하지만, 관측성·비매끄러운 최적화·moment closure의 일반 성질을 새 Contribution으로 주장하지 않는다.

#### 1. 이번 분석으로 달라진 판단

| 방향 | 새로 좁힌 문제 | 확인 수준 | 아직 성립하지 않는 주장 |
|---|---|---|---|
| 1 역설계 | Capacity 이상 Ice의 저장량은 외관/형상에서 숨겨지지만 후속 융해 request에는 남음 | 코드 경로 + 조건부 State 쌍의 대수적 반례 | 같은 초기 상태·U-input의 서로 다른 허용 rollout을 확보했다는 주장, 실제 화면 비트 일치 |
| 2 보호 편집 | Heat 임계값 아래에서는 융해 민감도가 0이어도 유한 입력으로 반응을 켤 수 있음 | local request의 정확한 식 분석 | 작은 Jacobian의 rank 부족으로 전역 불가능을 인증하거나, 새 optimizer가 필요하다는 주장 |
| 3 coarse | 평균뿐 아니라 모든 이차 State 모멘트가 같아도 threshold 반응률이 달라질 수 있음. 형상 clamp도 평균화와 교환되지 않음 | 현재 식에 대응한 두 반례 | 두 모멘트보다 모든 기존 closure가 부족하다는 주장, 새 compact 모델의 정확성·속도 |

**현재 판단:** 문제 존재의 근거는 더 구체화됐다. 해법의 신규성은 여전히 미확보다. 방향 3은 최소 반응 연산부터 분리해 분석하기 좋고, 방향 1은 방향 2의 목표/후속 검증으로 묶는다. 세 방향을 모두 구현하는 계획으로 확대하지 않는다.

#### 2. 공통 단위와 코드 근거

유효 texel i의 기준 면적 배율을 \(a_i=A_i/(1/256^2)=65536A_i>0\), 저장 State 총량을 \(q_i\), 면적당 양을 \(s_i=q_i/a_i\), Profile Capacity 계수를 c라 둔다. 저장량의 포화 기준은 \(C_i=ca_i\)다. 여기서 Heat의 면적당 양은 온도라는 물리 단위를 가정하지 않는다.

| 코드 | 이번에 확인한 계산 |
|---|---|
| [IceReactionLab.SRProfile](C:/Users/yonai/Documents/MDSS_Engine/Assets/SurfaceProfiles/IceReactionLab.SRProfile:6) | Ice c=4, spread/gravity/decay=0, accumulationFactor=1, cavityFillFactor=0, thickness=0.035. 융해 k=0.5, Heat ramp threshold=0.25, Heat 소비 계수=0.45 |
| [StateMath.glsl::accumulationHeight](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Solver/StateMath.glsl:48) | 형상에 쓰는 State를 `min(State, Capacity)`로 제한. `saturation`의 수송용 비율은 상한 없이 저장량/Capacity |
| [SurfaceReaction.comp::reactionRequest](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Simulation/Solver/SurfaceReaction.comp:18) | stateProportional은 저장량을 읽고 modifier는 State/면적으로 정규화. rampAbove는 `max(0,(X-Threshold)/Scale)` |
| [StateSampling.glsl::StateSaturationForProfile](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Rendering/Surface/StateTexture/StateSampling.glsl:39) | 표시량 `clamp(Amount/Capacity,0,1)` |
| [SurfaceLit.glsl](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Rendering/Surface/Lighting/SurfaceLit.glsl:141) | Ice 색/roughness에 위 포화 sampling 사용 |
| [TexelGeometry.comp::DisplayAccumulationHeight](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Rendering/Overlay/Geometry/TexelGeometry.comp:118) | 표시 형상도 채널별 `min(State,Capacity)` 사용 |
| [OverlayStateSampling.glsl::OverlayStateSaturation](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Rendering/Overlay/Coverage/OverlayStateSampling.glsl:55), [OverlaySurface.glsl](C:/Users/yonai/Documents/MDSS_Engine/Shaders/Rendering/Overlay/Shading/OverlaySurface.glsl:233) | Overlay coverage의 포화 비율 및 IceCoverage 제한. Ice shader는 별도 `Ice.glsl`이 아니라 이 경로에 있음 |
| [SurfaceContactApplication.cpp](C:/Users/yonai/Documents/MDSS_Engine/Source/SurfaceState/Contact/SurfaceContactApplication.cpp:356) | Falloff=0이면 support 내 가중치 1. 입력은 Strength×InputFactor×a. 저장량 상한과 동일시하지 않음 |

각 식은 연산의 해당 부분을 분석한다. 생산 Solver의 Transport→Decay→Reaction 순서, 공급량 경쟁·내부 시간 분할·Geometry 갱신을 생략한 request 분석을 전체 Step 실행 결과라고 쓰지 않는다.

#### 3. 방향 1: 같은 포화 외관·형상, 다른 후속 반응

##### 3.1 저장량과 표시량의 비단사성

[C,D] 고정된 동일 평면·Profile·재질·camera·lighting, 다른 State=0인 두 상태를 생각한다. 비교하는 모든 유효 texel에서

\[
I_i^{(x)}=4a_i,\qquad I_i^{(y)}=8a_i.
\]

Ice 표시 포화도는 두 상태 모두 1이다. Ice의 cavity 기여가 0이므로 world 기준 요청 축적 두께도 두 상태 모두

\[
g_i=0.035\min(I_i/a_i,4)=0.14
\]

다. 동일 변환/normal에서 local 높이 변환도 같다. 모든 관련 이웃의 capped 값이 같으므로 그 값에서 만드는 smoothing·형상 gradient도 같다. 초과 Ice는 immobile이고 decay=0이며, Heat=0이면 두 반응의 request는 0이다.

따라서 위 조건의 **clamped State 관측과 형상 관측은 저장량 차이를 식별하지 못한다.** 실제 최종 framebuffer의 완전한 동일성은 아직 실행 확인하지 않았다. overflow inspector·총량 지표를 관측에 포함하면 당연히 두 상태를 구별한다. 일반적인 모든 관측에 대한 비식별성 주장이 아니다.

##### 3.2 같은 Heat probe가 차이를 드러내는 이유

Heat 면적당 양을 \(h_i=H_i/a_i\)라 하면 현재 초기 융해 request는

\[
r_i=0.5I_i(h_i-0.25)_+.
\]

같은 probe로 h=0.5를 준 순간, 두 request는 각각 \(0.5a_i\), \(1.0a_i\)다. 이는 **초기 requested extent/time**이며 유한 GPU Step의 실측값이 아니다. 충분히 짧은 시간에는 Heat 소비 0.45와 Ice 공급량이 이를 제한하지 않는 조건을 만들 수 있다. 새 Waterfilm 냉각·수송·Heat Decay 때문에 이후 곡선은 이 고정률을 그대로 적분한 값이 아니다.

원래 4a인 Ice는 소량 융해 뒤 Capacity 아래로 내려가고 8a인 Ice는 한동안 포화 상태에 남는다. 이 때문에 후속 물막 생성뿐 아니라 이후 Geometry에도 차이가 생길 가능성이 있다. 전체 장면에서 유의미한 차이인지는 미검증이다.

##### 3.3 중요한 허용 조작 범위의 제한

이 쌍은 **State→관측의 비단사성**을 보인다. [[90_Research-Archive/Research-Notes-2026-10/02_Authoring/01_Goal-and-Protected-Editing#source-authoring-04-inverse-authoring|U-input]]은 초기 Ice를 고정하고 Heat/Waterfilm 입력만 조정하므로, 위 초기 Ice 쌍을 그 과제의 두 해라고 부를 수 없다.

별도 **U-seed 진단**으로 초기 Ice contact strength만 4와 8로 바꾸는 구성은 기존 입력 식과 양립한다. 균일 Profile에서 support 전체를 덮는 Falloff=0 contact가 그 후보이며 실제 파일/장면은 아직 만들지 않았다. 이것은 고정 초기 상태 과제에서의 허용 범위를 사후 바꾸는 대조가 아니다. U-seed와 기존 U-input 결과를 별도로 보고한다.

따라서 다음 두 검증을 분리해야 한다.

1. U-seed에서 이 코드 기반 반례를 실제 State/형상/외관/probe 계측으로 확인한다.
2. 원래 U-input에서도 관측 목표를 비슷하게 맞춘 서로 다른 admissible rollout이 생기고 그 차이가 제작상 중요한지 찾는다. 없다면 U-input 역설계의 핵심 동기로 쓰지 않는다.

**연구적 의미와 한계:** 외관 목표만 맞춰 향후 거동까지 맞는다고 간주할 수 없다는 구체적 예다. 숨은 저장량·관측 모호성 자체는 새 연구가 아니며, probe loss를 더한 표준 constrained optimization으로 충분하면 방향 1의 독립 기여는 약하다.

#### 4. 방향 2: 0 민감도와 유한 편집의 실행 가능성은 다르다

[D] request 하나를 분리해 Ice 면적당 양 i>0, \(h_0=0.20\)라 두면

\[
\rho(i,h)=0.5i(h-0.25)_+=0,\qquad
\partial_h\rho(i,h_0)=0.
\]

\(|\delta h|<0.05\)의 작은 차분도 융해 request 변화 0만 본다. 그러나 허용 bounds가 \(\delta h=0.10\)을 포함하면 새 request는 \(0.025i>0\)다. 한 request의 예이며 전체 보호 제약을 만족하는 rollout witness는 아니다.

이 ramp는 threshold에서 연속이나 미분 불가능하고, 아래쪽에는 평평한 구간이 있다. TeG가 다루는 parametric discontinuity와 완전히 같은 문제라고 하지 않는다. 작은 AD/차분의 0 gradient가 틀린 것도 아니다. **그 gradient로 전역 도달 가능성까지 판단하는 해석이 틀릴 수 있다.**

##### 4.1 수정한 알고리즘 탐색 명세 — 전부 미구현

1. 현재 active regime 안에서는 표준 bounded SQP/trust-region과 full-coupling 민감도로 편집/보호를 맞춘다.
2. 진전이 없으면 선언된 ramp/step threshold와 Capacity clamp까지의 margin을 검사한다. 입력 basis가 바꿀 수 없는 margin을 건너도록 무리하게 제안하지 않는다.
3. 허용 조작으로 event를 넘는 유한 trial을 검토하고, 원래 forward로 보호·수지·bounds를 검증한다. 이때의 추가 rollout 비용도 포함한다.
4. 성공한 regime에서 다시 국소 탐색한다. 실패하면 국소 탐색 실패/예산 소진으로 보고한다. 전역 불가능은 별도 보존식 등 인증이 있을 때만 말한다.

[H] 의미 있는 후보는 반응·형상 event의 구조를 이용해 불필요한 전역 trial을 줄이면서 보호 제약을 유지하는 방법이다. **event-aware 최적화 자체가 새롭다는 주장은 하지 않는다.**

강한 Baseline은 작은 차분 하나만 쓰는 gradient descent가 아니다. 다중 크기 차분+SQP, multi-start, 동일 budget의 derivative-free 방법을 포함한다. 이들이 충분하면 event 특화 알고리즘의 기여 주장을 중단한다. 현재는 전체 보호 과제의 필요성·성공률·비용 우위 모두 미확인이다.

#### 5. 방향 3A: 모든 이차 State 모멘트도 ramp 반응에는 부족한 반례

##### 5.1 여섯 개의 동일 면적 미세 셀

[C,D] Transport/Decay/Geometry 변화와 다른 반응의 영향을 분리해 `ice_melting`의 초기 request만 비교한다. 각 셀의 Ice 면적당 양 i=1, Waterfilm=0, Heat h는 다음과 같다.

| 분포 | 6개 셀의 h | 평균 h | 평균 h² | 분산 h | 평균 \((h-0.25)_+\) | 평균 융해 request/면적/시간 |
|---|---|---|---|---|---|---|
| X | 0, 0, 0, 0.5, 0.5, 0.5 | 1/4 | 1/8 | 1/16 | 1/8 | **1/16** |
| Y | 0, 0, 0.25, 0.25, 0.25, 0.75 | 1/4 | 1/8 | 1/16 | 1/12 | **1/24** |

Ice가 상수이므로 Ice–Heat 공분산은 모두 0이고, 나머지 채널도 같아서 **전체 State 벡터의 평균과 이차 모멘트가 일치**한다. 하지만 \(0.5\overline{i(h-0.25)_+}\)는 다르다. 동일한 이차 통계만 받는 모델은 두 초기 request를 동시에 정확히 재현할 수 없다.

이 반례는 threshold가 포함된 비다항 반응에서 covariance 한 항만 더하면 충분하다는 기대를 배제한다. 두 분포의 max와 활성 비율은 다르므로 min/max·histogram·quadrature까지 같은 정보라고 주장하지 않는다. 실제 공급량 제한·내부 시간 분할을 포함한 유한 Step 결과는 별도다. 이 반례 역시 일반적인 moment 문제의 한 사례이며 새 정리가 아니다.

##### 5.2 필요한 feature를 반응식에서 선택할 수 있는가?

초기 rate만 맞추려면 \(M_{\rm melt}=\overline{i(h-\tau)_+}\), 냉각에는 \(M_{\rm cool}=\overline{hw}\)처럼 실제 선언 rule의 request를 보존하는 feature가 직접적이다. 그러나 fine 정보를 매 Step 읽어 이를 다시 계산하면 compact coarse 방법이 아니라 oracle이다.

고정 면적·반응 rate의 연속시간 해석에서도 거의 모든 시점에서

\[
\frac{d}{dt}M_{\rm melt}=
\overline{\dot i(h-\tau)_+ + i\mathbf1_{h>\tau}\dot h}
\]

다. 여기에 현재 반응을 대입하면 활성 영역의 다른 곱/고차 정보가 필요하다. 수송까지 켜면 경계 상태와 공간 연결도 필요하다. feature를 하나 추가한 것만으로 닫힌 진화식을 얻은 것은 아니다.

[H] **선언된 반응과 형상 식이 실제로 사용하는 비선형 feature를 우선 보존하는 소수 양수 quadrature/적응형 microcell 표현**을 검토할 수 있다. 특정 threshold 양쪽의 대표 상태를 두는 방법도 후보지만, 알려진 presumed-PDF/QMOM/DQMOM과 비교해야 한다.

#### 6. 방향 3B 이전에 분리할 형상 clamp의 평균화 문제

[C,D] 평면·meso=0·cavity=0·한 축적 채널에서 \(g(s)=\kappa\min(s,c)\)다. \(\kappa\)는 accumulationFactor×thickness다. 두 동일 면적 셀을 합치면:

| 미세 State/면적 | 평균 s | 평균 fine 높이 | 평균 State에서 다시 만든 높이 |
|---|---|---|---|
| 0, 2c | c | κc/2 | κc |
| c, c | c | κc | κc |

같은 총 저장량이 같은 평균 축적 높이를 뜻하지 않는다. 이 차이는 Chart 최대 높이와 무관하고, 현 artistic occupancy의 Capacity clamp에서 직접 생긴다. concave clamp와 평균의 비가환성은 알려진 성질이다.

필요하다면 coarse 저장량 \(\bar s\)와 별도로 **capped occupancy** \(m_c=\overline{\min(s,c)}\)를 구별해야 한다. \(0\le m_c\le\min(\bar s,c)\)이며 초과 저장량을 버리라는 뜻이 아니다. \(m_c\)도 독립적인 시간 진화에는 cap 근처 분포 정보가 더 필요하다.

이 예는 평균 높이 관측만 분석한다. 같은 평균 높이라도 공간 배열/장벽 연결에 따라 Flux가 달라진다. 반응 feature와 capped occupancy를 저장한다고 수송 경로까지 해결되지는 않는다. 물리 부피 보존 문제로 이름을 바꾸지도 않는다.

#### 7. 추가 문헌 확인과 독창성 제한

[P] Akroyd et al.의 Cambridge preprint(2009, 후속 CES 2010) 원문 서론은 joint composition PDF로 반응 source를 닫고, 저차 모멘트의 presumed PDF는 가정한 분포에 의존하며, DQMOM의 가중 대표 상태로 다중 scalar를 다루는 기존 방법을 설명한다. singularity/boundedness도 이미 논의한다. 따라서 **‘평균 대신 소수 대표 상태로 비선형 반응을 계산’은 새 아이디어가 아니다.** [R12, [기관 원문](https://como.ceb.cam.ac.uk/media/preprints/c4e-Preprint-76.pdf)]

[P] Marchisio & Fox의 QBMM 장 요약도 모멘트에서 node/weight를 복원하는 방식과 다변량 확장을 명시한다. 본문 전체를 읽지는 않았다. [R13, [출판사 요약](https://doi.org/10.1017/CBO9781139016599.004)]

확인 범위와 접근 실패는 [[90_Research-Archive/Research-Notes-2026-10/04_Literature/01_Related-Work#source-reference-07-related-work|R12–R13 장부]]를 따른다. DQMOM이 MDSSP의 State-dependent transport/capped geometry에 그대로 가장 좋은 방법이라는 결론도 내리지 않는다. 남은 비교 질문은 **반응·점유·경로를 함께 다룰 때 표준 closure의 어떤 가정이 실제로 부족하며, 그 부족분을 작은 독립 갱신 상태로 해결할 수 있는가**다.

#### 8. 최소 판별 과제 — 실행 재개 전 명세

| ID | 입력/관측을 고정할 내용 | 첫 강한 대조 | 기각/축소 조건 |
|---|---|---|---|
| ANA-1 | U-seed와 U-input 분리. 관측에 overflow 총량을 포함하는지 명시. 같은 probe 및 초기/후속 State·형상·화면 비교 | 외관/상태 목표 + 표준 probe loss 최적화 | 원래 U-input에서 모호성이 없거나 probe 차이가 제작상 중요하지 않음 |
| ANA-2 | 작은 request 예 → 보호가 있는 rollout 순으로 확대. 허용 basis/bounds와 threshold 거리 보관 | 다중 크기 FD+SQP, multi-start, 동일 budget derivative-free | 표준 탐색으로 충분하거나 Geometry 경로가 중요하지 않음 |
| ANA-3 | 2-cell 냉각, 6-cell 융해, 2-cell capped height를 각각 분리. request와 적분 결과를 구분 | 평균/단위 보정, covariance, 표준 2/4-node 양수 quadrature, microgrid | 기존 compact closure가 충분하거나 대부분 fine 유지가 필요함 |

ANA는 분석 질문 식별자이며 실행 번호/PERF 결과가 아니다. 다음 구현 후보를 정하기 전 표준법의 수식·갱신 조건과 실제 비용을 먼저 확정한다. 실측이 생기면 기존 Experiments와 Performance-Data 구조에 기록하고 이 파일에는 해석을 연결한다.

**아직 미해결:** U-input의 실제 허용 해 쌍, 보호 편집의 nontrivial feasible witness, 표준 optimizer의 실패, coarse closure의 장시간 독립 갱신·양수성·수지·경계 통과와 비용, 외관/제작상 중요성. 현재 결론을 Contribution 확보로 상향하지 않는다.

이후 사용자의 심화 추론 요청에 따라 [[90_Research-Archive/Research-Notes-2026-10/03_Solver/02_Coarse-Models-and-Limits#source-analysis-11-contribution-proof-obligations|입증 조건 문서]]에서 내부 이동량/inflowFraction의 공간 반례, hinge/cap 분석, 대표 상태의 존재와 갱신 한계, 보호 하한·도달 불가능 조건을 추가했다. 이 노트의 초기 반례와 새로운 내용을 구별해 보존한다.

---
