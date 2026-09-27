# Branch 3 — OutgoingFluxScale Debug View

브랜치: `feat/solver-outgoing-flux-debug`  
선행 조건: `feat/solver-transfer-weights` 병합  
관련 설계: [[03_Architecture/0006_Rendering|Rendering]], [[03_Architecture/0007_Surface-GPU-Data-Layout|Surface GPU Data Layout]], [[05_Development/Notes/0003_Surface-State-GPU-Resource|Surface State GPU Resource]]

## 목표

Solver Pass 1이 저장하는 채널별 `OutgoingFluxScale`을 Surface Debug 그룹에서 시각화한다. State Heatmap과 같은 Registry 채널 선택 흐름을 사용하고, 화면에서 값의 의미를 판독할 수 있게 한다.

## 구현 범위

- Surface Debug view enum/name에 `Outgoing Flux Scale`을 추가한다.
- Debug fragment shader가 해당 instance의 alpha buffer와 선택된 State channel을 읽는다.
- Graphics descriptor layout/set에 필요한 binding 및 stage visibility를 연결한다. Compute descriptor만 수정하는 것으로 끝나지 않도록 실제 graphics path를 확인한다.
- `[0,1]` 값을 구별 가능한 연속 ramp로 렌더링하고 legend를 표시한다: `0`은 outgoing을 강하게 제한, `1`은 제한 없음.
- unsupported/invalid texel 색상은 기존 Surface Debug view와 동일한 기준을 따른다.
- GPU ABI/binding 변경이 있으면 layout 문서를 함께 갱신한다.

## 검증

- 작은 GPU solver fixture의 alpha readback과 화면 shader가 같은 channel/index를 참조한다.
- `RawOutgoing == 0`인 texel은 alpha 1, invalid/unsupported channel은 정의한 neutral/invalid 색으로 나타난다.
- 여러 Registry channel 중 선택한 channel만 view에 반영된다.
- 기존 State Heatmap, Validity, Surface ID, Neighbor Count, UV Seam view가 유지된다.
- Vulkan validation에서 descriptor binding/stage 오류가 없다.

## 완료 조건

Demo Scene에서 State channel별 alpha를 볼 수 있고 색상 범례가 값의 의미와 일치한다.

## 제외 범위

Solver pause/step/reset, 통계 UI, Outgoing/Incoming raw flux readback, 시각화 성능 최적화.
