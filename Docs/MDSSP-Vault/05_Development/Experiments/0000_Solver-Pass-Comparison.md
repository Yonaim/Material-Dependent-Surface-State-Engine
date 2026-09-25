# 실험 — Solver Pass 비교

- 상태: **계획**
- 근거: [[07_Assets/Documents/0005_Next-State-Calculation.pdf|Next State 계산]]

## 가설

이웃 alpha를 매번 재계산하는 1-Pass보다 `alpha`를 저장하는 2-Pass가 중복 계산을 줄인다.

## 비교 조건과 방법

- 동일한 State Field와 8-neighbor 조건.
- 동일한 SRProfile과 입력 이벤트.
- texel 수와 활성 texel 비율 변화.
- GPU 시간, 임시 메모리, 메모리 대역폭, Barrier 비용 측정.

## 결과

미실시. 현재 설계 기본안은 **2-Pass + alpha 저장**이다.
