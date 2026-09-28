# Next State 계산 메모

> **한 줄 요약:** 2-Pass Compute Shader에서 Surface State의 다음 값을 계산하는 순서와 중간 데이터를 설명한다.

상태: **2-Pass 기본안 / GPU Resource 기본안 작성됨** · 근거: [[08_Assets/Documents/0005_Next-State-Calculation.pdf|Next State 계산]]

Architecture 수준의 수식은 [[04_Architecture/0006_Surface-State-Update|Propagation Solver]]가 기준이다. 이 문서는 Compute Shader 계산 순서만 기록한다.

## 초과량 보존 계약 — 구현 완료 · GPU 실행 검증 대기

[[05_ADR/Simulation/0020-State-Overcapacity-Transport|ADR 0020]]의 전달용 Saturation은 `Current / Capacity`이며 1을 넘을 수 있다. Pass 2는 `Next = max(Current + InputDelta + Incoming - Outgoing - Decay, 0)`으로 Capacity 초과량까지 같은 State A/B에 저장한다. 받은 양은 A/B 교환 후 다음 Solver step의 RawFlux에 반영한다. Pass 1의 alpha는 전체 Current에서 Decay를 뺀 source 보유량으로 제한하며 수신 측 공간 제한이나 추가 pass·buffer를 만들지 않는다.

Shader의 Saturation `[0,1]` clamp와 Next Capacity 상한 clamp를 제거했다. 빌드는 통과했으며 기존 실행·테스트 기록과 대기 중인 새 계약의 GPU 검증을 구분한다. RawOutgoing 저장·재사용은 [[../../04_Architecture/0007_Surface-Solver-Cache|Solver 캐시]]를 따른다.

## Gather 방식

- 한 invocation / thread가 한 texel을 담당한다.
- Current State와 주변 texel만 읽는다.
- 자신의 Next State만 쓴다.
- Current / Next State는 ping-pong이 필요하다.

## alpha 문제

Outgoing은 Decay 이후 남은 State를 넘을 수 없다. 따라서 각 texel의 `alpha`가 필요하고, Incoming을 계산하려면 **이웃 texel의 alpha**를 알아야 한다.

## 기본안: 2-Pass

```text
Pass 1
texel i
├─ Decay[i]
├─ i → 주변 8개 RawFlux
├─ RawOutgoing 합
└─ alpha[i] 저장

Barrier

Pass 2
texel i
├─ 주변 j → i RawFlux 재계산
├─ alpha[j] 읽기
├─ Incoming / Outgoing 계산
└─ NextState[i] 기록
```

1-Pass에서 이웃의 `alpha[j]`를 재계산하면 각 이웃마다 다시 주변 8개 flux를 계산해야 해서 중복 계산이 커진다. 현재 기본안은 `2-Pass + alpha 저장`이다.

State A / State B / OutgoingFluxScale 리소스 타입, descriptor, barrier는 [[06_Development/Notes/0003_Surface-State-GPU-Resource|Surface State GPU Resource]]를 따른다.

## 구현 시 결정할 항목

- `ContactInput.falloff`가 제어하는 거리 감쇠 함수의 구체적인 형태.
- 상태 전이(예: Heat → Burn)를 Solver에 적용하는 순서와 같은 패스/별도 패스 여부.

위 항목은 Architecture에서 정의한 입력·전이의 의미를 바꾸지 않고, 구현과 검증 과정에서 정한다.
