# Local Reactions — State 전이와 상호작용

상태: **현재 구현** · 2026-10-26

`.SRProfile` version 5의 `reactions`가 같은 texel 안의 전이·생성·소비와 State에 의한 촉진·억제를 정의한다. State는 기존처럼 texel별 총량이고 저장 상한은 없다. Capacity는 조건 임계값이나 생성 상한으로 재사용하지 않는다.

개념적으로 **State 전이는 Reaction의 하위 유형**이다. `Heat`를 소비하고 `Burn`을 같은 양만큼 생성하려면 version 5 Reaction의 `consumes: {"heat": 1}`과 `produces: {"burn": 1}`로 선언한다. 아래에서 말하는 version 3/4 `transitions`는 이전 파일 형식의 호환 규칙으로, 이 소비형 전이와 의미가 다르다.

## Profile 선언

기존 `states` 파라미터는 유지한다. version 5는 `transitions` 대신 `reactions` 배열을 요구하며 두 키를 혼용하지 않는다. version 3/4의 Transition은 GPU 패킹 시 source를 소비하지 않는 조건부 생성 규칙으로 변환한다. 기존 source Saturation 임계값은 `threshold × source.stateCapacity`의 기준 면적 임계량으로 환산한다.

| 필드 | 현재 계약 |
|---|---|
| `id` | Profile 안에서 유일한 비어 있지 않은 식별자 |
| `enabled` | 기본 true. false 규칙은 검증 후 GPU 실행 목록에서 제외 |
| `mode` | 기본 `continuous`; 또는 `inflowFraction` |
| `consumes`, `produces` | `{State 이름: 반응량 1당 계수}`. 계수는 유한한 양수이며 한쪽 목록은 비어 있어도 됨 |
| `rate` | continuous의 `model`, `coefficient`, 모델별 `state` 또는 `factors` |
| `modifiers` | continuous의 선택적 조건 배열. 배율을 모두 곱함 |

참여하는 모든 State는 해당 Profile의 `states`에 있어야 한다. 이름은 기존처럼 trim/lowercase 정규화하고 실제 GPU에서는 Registry ID를 쓴다. 고정 State 개수나 고정 ID를 가정하지 않는다.

## 면적과 반응 속도

`a = WorldTexelArea / ReferenceArea`이고 `ReferenceArea = 1/256²`다. 조건은 **기준 면적당 양으로 통일**한다. Rule별 `sample` 선택은 지원하지 않으며 로더가 거부한다.

| 속도 모델 | 반응 진행률 |
|---|---|
| `constant` | `a × coefficient`; 계수 단위는 기준 면적당 반응량/초 |
| `stateProportional` | `coefficient × S[state] / consumes[state]`; 지정 State의 소비 비율/초 |
| `massAction` | `a × coefficient × Π(S[f.state] / (a × f.reference))^f.exponent` |

`stateProportional`의 기준 State는 소비 목록에 있어야 한다. `massAction.factors` 원소는 `state`, 양수 `reference`, 양수 `exponent`를 가진다. factor에 포함된 것만으로는 State를 소비하지 않는다.

Modifier는 `state`, `function`, 함수에 필요한 `threshold`/`scale`을 가진다. `x=S[state]`, `h=a×threshold`, `w=a×scale`로 평가한다. 임계량은 비음수이고 scale은 양수다.

| 함수 | 배율 |
|---|---|
| `above` | `x >= h`이면 1, 아니면 0 |
| `below` | `x <= h`이면 1, 아니면 0 |
| `rampAbove` | `max(0, (x-h)/w)` |
| `rampBelow` | `max(0, (h-x)/w)` |
| `linear` | `x/w` |
| `inhibit` | `1/(1+x/w)` |

저장 총량을 기준으로 계산한 뒤 조건의 threshold와 scale을 모두 면적으로 환산한다. State 보유량에 비례하는 속도에는 면적을 다시 곱하지 않는다. 서로 다른 단위의 소비·생성량 관계는 각 term의 계수로 정한다. 모든 채널 숫자의 단순 합이 보존된다고 가정하지 않는다.

## 신규 유입에 따른 전환

`inflowFraction`은 `rate`/`modifiers` 대신 `inflowState`, `inflowSources`, `[0,1]`의 `fraction`을 요구한다. inflowState는 소비 대상이어야 한다.

- `positiveInput`: 이번 Pass 2에서 적용한 **순 event delta의 양수 부분**. 여러 Add/Remove 사건은 기존 InputDelta 계약대로 먼저 상쇄된다.
- `incoming`: source outgoing 제한을 반영한 실제 이웃 유입량.
- 요청 반응량은 `선택한 신규 유입량 × fraction / consumes[inflowState]`다. 실제 보유량을 넘겨 소비하지 않는다.
- 모든 유입 전환을 동일 snapshot에서 함께 제한·적용하고, 그 뒤 continuous 적분을 시작한다. `dt`를 곱하지 않고 내부 시간 분할에서도 재적용하지 않는다.

## GPU 실행과 자원 경쟁

실행 순서는 Pass 1 → Pass 2의 Input/Transport/Decay → **Reaction Pass** → 최종 Height/Geometry다. Reaction 없는 Scene은 기존 두-pass 경로를 유지한다.

Pass 분기는 **Scene 전체**에서 활성 규칙이 하나라도 있는지로 결정한다. 한 Profile에만 규칙이 있어도 그 Scene에서 Solver dispatch 대상인 모든 instance의 Pass 2가 각 채널의 기본 결과·유입 정보를 scratch에 기록하고 Reaction Pass를 실행한다. Reaction Pass는 texel의 Profile별 규칙 수를 조회한다. 규칙이 0개인 Profile의 texel은 기본 결과를 그대로 최종 Next State에 기록한다.

각 invocation이 하나의 texel을 소유한다. 각 반응 구간에서 모든 요청량을 같은 snapshot으로 계산하고 State별 총 소비량을 모은다. 소비량이 보유량을 넘으면 State별 공급 비율을 계산하고, 각 반응은 소비 대상들의 최소 비율만큼 **소비·생성을 함께** 줄인다. 결과를 다른 snapshot plane에 기록한 뒤 교체한다. Rule 실행 순서가 연쇄 반응 순서를 결정하지 않는다.

Continuous는 최대 `1/240초` 및 가능한 경우 source의 `10%` 이하 소비를 목표로 내부 세분화한다. 내부 구간은 최대 256회이며, 예산을 넘으면 마지막 구간에서 남은 시간을 공급량 제한과 함께 처리하고 진단 flag를 남긴다. 이는 속도 기반 세분화이며 오차 추정 적응 적분기나 임의의 강직 반응에 대한 정확성 보장은 아니다. 산술 비유한 요청은 제외하고, 비유한 결과가 생기는 구간은 적용하지 않으며 오류를 기록한다.

Reaction이 만든 State는 다음 Solver step부터 이웃으로 이동한다. 동일 Solver step의 뒤쪽 Reaction 내부 구간에서는 앞 구간의 생성물을 참조할 수 있다.

## Sparse와 Geometry

Reaction Scene의 Pass 2는 실행 중인 workgroup에서 모든 채널의 기본 결과와 유입 정보를 준비한다. 비어 있는 생성 대상 채널을 생략하지 않는다. Reaction Pass가 최종 State를 기록하고 다음 global/WG 채널 마스크·인접 workgroup·accumulation dirty schedule을 생성한다. `Current > 0` 조건으로 기존 A/B buffer 정리 세대를 유지한다. 초기 full seed는 빈 texel에서 가능한 생성 규칙도 평가한다.

## ABI와 진단

binding 23은 Scene 공유 uint-word Reaction table이다. Profile별 Rule 범위와 16-word Rule, 가변 길이 소비/생성/조건/factor 목록을 보관한다. binding 24는 instance별 float scratch다.

`N × (5K + M + 4)` float: 두 State snapshot, 소비 비율, positive input, incoming의 다섯 K-plane, M개의 요청/실제 extent, 네 진단값이다. K는 Scene 채널 수, M은 Profile당 최대 활성 규칙 수다. Reaction 없는 Scene에서는 dummy scratch 하나만 할당한다.

Texel Inspector는 내부 구간 수·공급량 제한 횟수·시간 예산 초과·산술 오류를 표시한다. Profile 패널은 Rule의 소비·생성·조건을 조회한다. Rule 수정은 `.SRProfile` 편집 후 Scene 재로드로 적용한다. GPU query의 기존 Pass 2 timestamp 구간에는 Reaction 실행이 포함된다.

## Lit 예제

`Assets/Scenes/IceReactionLab.Scene`은 MarbleCube 판 세 개와 render-only stage/벡터 라벨을 사용한다. State는 `ice`, `waterfilm`, `heat`다. 용융은 Ice 1과 Heat 0.45를 소비해 WaterFilm 1을 생성하며, 별도 냉각 규칙은 물을 소비하지 않고 Heat 감소를 촉진한다. 계수는 현상학적 모델 값이며 실제 열량 단위로 보정한 열역학 모델은 아니다.

`Assets/Scenes/ReactionTransitionComparison.Scene`은 같은 Fuel 초기량과 1초 시점의 동일한 Heat 입력을 세 판에 적용한다. 왼쪽은 Reaction 규칙이 없다. 가운데는 version 5의 일대일 소비형 전이 `Heat → Burn → Scorched`를 적용한다. 오른쪽은 `Fuel + Heat → Burn`, `Burn → Scorched`의 다중 입력 Reaction을 적용한다. 가운데는 Fuel이 남고 오른쪽은 연료 공급량에 따라 Burn 생성이 제한된다. Burn은 일시적인 발광 효과, Scorched는 남는 어두운 표면 흔적이다. 이 Scene에는 Lava·Ice·WaterFilm State가 없다. 계수는 시각 설명용이며 물리 단위로 보정한 연소 모델은 아니다. 제작 및 재생 방법은 `Assets/Scenes/ReactionTransitionComparison.README.md`에 둔다.

비교 씬의 각 판은 위쪽 한 면만 State solver에 등록하고 나머지 다섯 면은 render-only 메시로 그린다. 기존 여섯 면 시뮬레이션은 눈에 보이지 않는 옆면까지 Reaction Pass를 실행해 프레임 비용이 컸다. 같은 UV 윗면의 State는 변경 전후 같은 step에서 비트 단위로 일치한다. 측정값은 [[05_Experiments/02_Performance-Measurements#PERF-044 — Reaction Transition 비교 씬의 윗면만 시뮬레이션|PERF-044]]에 기록한다.

Ice는 기존 Mud와 별도의 State/재질/coverage로 처리한다. Lit에서는 서리 noise·푸른 산란색·유전체 반사를 사용하고 WaterFilm을 합성한다. 기존 RGBA render texture는 유지하고 다섯 번째 Ice 채널은 State buffer에서 별도로 sample한다. 네 적층 재질(Mud/WaterFilm/Lava/Ice)의 occupancy·coverage·smoothing 경로를 지원한다.

Burn·Scorched도 동적 Registry ID를 Lit shader에 전달해 State buffer에서 직접 sample한다. Burn은 주황색 발광, Scorched는 어두운 무광 tint로 표시한다. 두 State 모두 이 비교 씬에서 형상 적층을 만들지 않는다.

구현: `SRProfileLoader.cpp`, `SurfaceReactions.cpp`, `ProfilePacking.cpp`, `SurfaceReaction.comp`, `ReactionBindings.glsl`, `StateFinalize.glsl`.
