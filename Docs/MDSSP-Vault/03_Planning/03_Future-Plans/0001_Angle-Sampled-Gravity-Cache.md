# 각도별 중력 GeometryDrive 사전 계산과 GPU 조회

> **한 줄 요약:** 고정 형상에서 회전 방향별 높이·방향 항을 CPU가 미리 계산해 GPU 상주 조회표로 보내는 방안을, 실제 GPU 병목이 확인되면 검토한다.

상태: **후속 작업 · 미채택 · 미구현** · 날짜: 2026-10-01

## 목표와 현재 경로

정적 Mesh도 회전하면 이웃 간 월드 높이 차와 표면에 투영된 중력 방향이 변한다. 현재 Solver는 instance model 행렬과 World Gravity를 push constant로 받아 GPU에서 GeometryDrive의 HeightDrive × DirectionDrive를 매 step 계산한다. RawFlux cache가 ON이면 Pass 1에서 만든 방향별 flux를 Pass 2가 재사용한다. 이 경로의 실측 GPU 비용 없이 사전 계산 방식의 이득을 가정하지 않는다.

[[../../05_ADR/0043-Rotation-Invariant-Transfer-Cache|ADR 0043]]의 순수 회전 TransferWeight buffer 갱신 제거가 선행한다. 이 후속 작업은 불필요한 GPU queue 대기를 해결하는 수단이 아니라, 남아 있는 GPU 중력 계산을 조회로 바꿀지 평가하는 별도 최적화다.

## 검토할 방식

1. 고정된 Mesh·크기·Meso 형상과 World Gravity에서 물체에 대한 **local gravity 방향**별 이웃 GeometryDrive를 CPU에서 준비한다. 전체 State량과 source saturation은 매 step 바뀌므로 표에 bake하지 않는다.
2. 모든 회전 각도를 균등 간격으로 나열하지 않는다. 단일 축 뒤집기 연출에는 1차원 회전 경로를, 자유 회전에는 local gravity 방향의 구면을 사용한다. 높이·방향 항의 보간 오차가 큰 구간을 더 촘촘히 분할한다. 단순히 양 끝값 차이만 보면 구간 내부 극값을 놓칠 수 있으므로 중간 표본 또는 오차 상한으로 확인한다.
3. CPU는 선택된 표본들을 Scene 준비 시 한 번 계산해 GPU에 상주시키고, Solver는 현재 local gravity 방향의 표본을 조회·보간한다. 회전할 때마다 CPU가 표본을 조회해 기존 GPU buffer에 다시 업로드하는 경로는 기본안이 아니다. 그것은 동기화·전송 비용을 되살릴 수 있다.
4. 적층 feedback으로 유효 위치나 normal이 바뀌면 정적 조회표가 낡는다. 이 경우 GPU에서 동적 부분을 계산할지, 제한된 부분만 갱신할지, 조회 경로를 비활성화할지 결정한다.

## 비용과 결정 기준

- 예시 메모리 가정: Surface 하나가 256×256 texel이고 texel당 8 이웃 슬롯, 표본·간선당 float32 값 하나를 padding 없이 저장하며 instance별 GPU buffer로 복제한다. 이때 각도 표본 하나는 256×256×8×4 byte = 2 MiB, 16개는 instance당 32 MiB다. 6 Surface인 같은 해상도 Cube 한 instance면 16개 표본에 192 MiB다. buffer allocation overhead, 보간 인덱스, 높이·방향을 별도 저장할 경우의 추가분은 제외한다.
- 먼저 ADR 0043을 반영한 뒤 기존 GPU 경로의 Pass 1 시간, 회전 중 frame time, CPU 대기 시간을 반복 측정한다.
- 표본 생성 시간, GPU 메모리, buffer 읽기 대 산술 연산, 보간 오차와 시각적 연속성을 기존 경로와 비교한다. 조회가 빨라지지 않거나 메모리 비용이 과도하면 기존 GPU 직접 계산을 유지한다.
- Flat·경사·오목한 홈, 0°·90°·180° 및 표본 사이 각도에서 직접 계산과 조회 결과를 비교한다. State 양 보존과 방향 부호가 바뀌는 구간을 포함한다.

## Related

- [[../../05_ADR/0015-Geometry-Driven-Transport|ADR 0015 — GPU GeometryDrive]]
- [[../../05_ADR/0021-Directional-RawFlux-Cache|ADR 0021 — Pass 간 RawFlux 재사용]]
- [[../../05_ADR/0043-Rotation-Invariant-Transfer-Cache|ADR 0043 — 회전 불변 캐시]]
- [[../../05_ADR/0044-Macro-Meso-Concavity-Field|ADR 0044 — 유효 형상]]
