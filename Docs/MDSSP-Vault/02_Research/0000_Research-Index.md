# 연구 자료 색인

> **한 줄 요약:** `02_Research/`는 프로젝트 관련 논문·기술 연구를 읽고 프로젝트와의 관계, 연구 요지, 적용할 부분과 한계를 정리하는 공간이다.

상태: **연구 노트 보관 기준** · 2026-09-28

## 목적과 위치

`02_Research/`는 프로젝트 관련 논문·기술 연구를 읽고 **프로젝트와의 관계, 쉬운 연구 요약, 상세 설명, 적용할 부분과 한계**를 정리하는 공간이다. 시작 안내·정책 다음, 계획·설계 앞에 두어 조사 근거에서 설계로 이어지는 흐름을 만든다.

| 위치 | 책임 |
|---|---|
| `02_Research/` | 외부 연구의 이해·비교·프로젝트 적용 검토 |
| `08_Assets/Documents/` | 원본 PDF 등 참고 자료. 노트에서는 DOI·공개 원문 또는 로컬 원본을 링크 |
| `04_Architecture/` | 프로젝트에 채택한 계약과 수식 |
| [[05_ADR/README\|`05_ADR/`]] | 채택·보류·기각 이유와 대안, 주제별 색인 |
| `06_Development/Experiments/` | 프로젝트 구현으로 수행한 실험 조건·측정 결과 |
| `00_Start/Templates/0006_Research-Note.md` | 연구 노트 양식 |

원문 설명과 프로젝트의 해석을 구분한다. 논문에서 증명한 조건을 프로젝트가 충족하지 않으면 해당 증명을 구현의 보장으로 인용하지 않는다. 적용 아이디어는 검토 상태로 두고 확정한 결정만 ADR/Architecture에 연결한다. 확인한 범위(초록·특정 절·전문)와 출처·버전·날짜를 기록한다.

## 연구 목록

| 문서 | 프로젝트 연관 | 적용 상태 |
|---|---|---|
| [[0001_Bound-Preserving-Transport\|포화 상한을 보존하는 유한체적 전달 계산]] | State/Capacity 의미, Transport 보존·비음수, timestep 검증 | 검증 관점 참고. 상한 보존 방식은 ADR 0020에 직접 채택하지 않음 |

## 작성 흐름

```mermaid
flowchart LR
  Source[논문 / 공식 연구 자료] --> Note[Research Note: 관계와 요지]
  Note --> Review[프로젝트 적용 검토]
  Review --> ADR[ADR: 결정과 대안]
  ADR --> Architecture[Architecture: 채택 계약]
  Architecture --> Implementation[구현]
  Implementation --> Experiment[프로젝트 실험 / 검증]
  Experiment --> Note
```

[[../00_Start/Templates/0006_Research-Note|연구 노트 템플릿]]을 복사해 `0001_`, `0002_` 순서로 추가하고 이 표에 연결한다.
