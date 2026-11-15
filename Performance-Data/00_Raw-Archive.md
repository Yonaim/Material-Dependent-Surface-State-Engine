# 성능 원자료 보관 범위

기존 측정의 비교표·manifest·설정 JSON과 약 19.5GB의 raw 로그·capture를 이 폴더에 함께 보관한다.

```text
Performance-Data/
```

과거 비교표를 재검증할 때는 해당 manifest의 실행 조건과 같은 실행 폴더의 `raw/`를 함께 읽는다. 대부분의 `raw/`는 Git에서 제외되므로 별도 백업 여부를 확인한다. 2026-10-04 두 실행의 JSONL 36개만 기존처럼 추적 대상으로 남겼다. 2026-10-05 두 실행의 manifest·summary는 이전 `06_Development/Performance/Results/`에서 이 폴더로 옮겼으며, 당시 로컬 JSONL은 없는 상태다. 새 실험에서는 [에이전트 볼트의 성능 측정 규칙](../Docs/Agent-Vault/05_Experiments/02_Performance-Measurements.md)을 따른다. 이 인덱스는 성능 측정이나 결과 해석이 아니다.
