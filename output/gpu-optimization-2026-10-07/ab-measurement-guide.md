# GPU 최적화 A/B 측정 가이드

## 공통 조건

1. 같은 `.Scene`, `.SRProfile`, 카메라, 해상도, 장치, 표시 효과와 simulation 입력을 사용한다. 변화가 계속 퍼지는 장면이면 측정 시작 시점과 frame 수를 동일하게 맞춘다.
2. Debug build의 Performance 탭에서 비교할 항목 하나만 바꾼다. 변경 시 profiling 평균이 초기화된다. shader/버퍼 재생성 직후 프레임은 제외하고 충분히 워밍업한 뒤 안정된 평균을 기록한다.
3. 각 조건을 여러 번 반복하고 GPU `Coverage sample`, Occupancy/Height/Normal/smoothing, Solver Pass1/Pass2, Render Pass, 전체 frame time을 함께 기록한다. GPU 시간이 0 또는 미지원으로 나오면 그 항목은 비교 결과로 해석하지 않는다.
4. 성능과 함께 경계, top, sides, 타일 전파, state 소멸 뒤 잔상, 동일 메시를 공유하는 여러 instance의 분리 상태를 눈으로 확인한다.

## 비교 순서

| 비교 | A | B/C | 주로 볼 지표 |
| --- | --- | --- | --- |
| Coverage scheduling | Overlay tile culling OFF | ON | Coverage sample, 전체 frame time, 변경 없는 active 영역의 결과 |
| 공통 render tile | 16×16 | 8×8 / 32×32 | Coverage, Occupancy, Height, Normal, smoothing, frame time |
| WG channel mask | Active Channel Mask ON, Per-WG OFF | Active Channel Mask ON, Per-WG ON | Solver Pass1/Pass2, 이웃 WG로의 전파 및 소멸 |
| Base draw | Base Mesh ON | OFF | Render Pass |
| Top draw | Overlay Top ON | OFF | Render Pass |
| Side draw | Overlay Sides ON | OFF | Render Pass |

Draw 원인 분리 측정은 기준 상태에서 세 draw를 모두 ON으로 둔 뒤 한 항목씩만 OFF로 바꾸어 반복한다. 각 비교가 끝나면 원래 설정으로 복귀한다. `Overlay tile culling OFF`는 Coverage 전체 정점 계산 경로도 사용하므로 sparse Coverage의 기준 조건이다.

## 재현 가능한 실행 예

프로젝트 루트에서 다음처럼 실행한다. benchmark 경로는 실제 장면과 출력 위치로 바꾼다. `--per-wg-channel-mask`를 생략하면 해당 옵션은 OFF다.

```sh
Build/bin/MDSS --benchmark-scene Assets/Scenes/BrickCube.Scene --benchmark-resolution 256 --benchmark-warmup-frames 30 --benchmark-measure-frames 120 --benchmark-output /tmp/mdssp-tile16.json --render-tile-size 16
Build/bin/MDSS --benchmark-scene Assets/Scenes/BrickCube.Scene --benchmark-resolution 256 --benchmark-warmup-frames 30 --benchmark-measure-frames 120 --benchmark-output /tmp/mdssp-tile8-wg.json --render-tile-size 8 --per-wg-channel-mask
Build/bin/MDSS --benchmark-scene Assets/Scenes/BrickCube.Scene --benchmark-resolution 256 --benchmark-warmup-frames 30 --benchmark-measure-frames 120 --benchmark-output /tmp/mdssp-tile32-wg.json --render-tile-size 32 --per-wg-channel-mask
```

위 CLI는 tile 크기와 WG mask를 고정한다. draw 토글 및 Overlay tile culling A/B는 Performance 탭에서 측정한다. benchmark JSON의 GPU 세부 항목과 UI의 평균을 비교할 때는 동일 build와 프레임 구간을 사용한다.
