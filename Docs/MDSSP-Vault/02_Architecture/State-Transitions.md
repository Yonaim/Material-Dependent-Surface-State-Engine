# State Transition

상태: **파라미터 구조 확정 / Solver 통합 시점 미정** · 근거: [[05_Assets/Documents/Surface-System-Data.pdf|시스템 데이터 구조]], [[05_Assets/Documents/Asset-Structure.pdf|에셋 구조]]

State Transition은 한 State가 조건을 만족했을 때 다른 State를 증가시키는 규칙이다.

예:

```text
Heat → Burn
```

| Parameter | 의미 |
|---|---|
| `source` | 전이의 원인이 되는 State |
| `target` | 전이 결과 증가하는 State |
| `threshold` | source Saturation의 임계값 |
| `transitionRate` | 조건 만족 후 target State의 단위 시간당 증가 속도 |

예를 들어 `threshold = 0.7`이면 source의 Saturation이 `0.7` 이상일 때 전이 조건을 만족한다.

현재 공통 State 갱신식은 Input / Transport / Decay 중심으로 정의되어 있다. Transition을 같은 Solver 패스에 합칠지 별도 패스로 둘지는 구현 단계에서 결정한다.
