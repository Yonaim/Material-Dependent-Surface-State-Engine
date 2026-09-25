# 볼트 사용 가이드

이 가이드는 프로젝트마다 같은 문서 구조를 사용할 때 참고하는 공통 안내다. `00_Start/`에는 이 가이드와 문서 템플릿만 둔다. 프로젝트 고유의 정책, 설계, 작업 현황은 해당 위치에 기록하고 이 폴더의 원본은 수정하지 않는다.

## 시작하기

1. 프로젝트의 볼트 폴더를 Obsidian에서 **Open folder as vault**로 연다.
2. `02_Planning/00_Project-Overview/0000_Project-Plan.md`와 `02_Planning/00_Project-Overview/0001_Roadmap.md`에서 프로젝트 범위와 주차별 계획을 확인한다.
3. `03_Architecture/0000_Overview.md`에서 시스템 전체 구성을 파악한다.
4. 공식 용어·문서 규칙은 `01_Project-Policy/`, 설계 결정 근거는 `04_ADR/`에서 확인한다.
5. 진행할 작업과 완료 여부는 볼트 루트의 `TODO.md`에서 확인한다.

## 문서 구조

```text
프로젝트 볼트/
├── TODO.md                      프로젝트별 작업 목록
├── 00_Start/                   공통 가이드와 재사용 템플릿
│   ├── 0000_Vault-Guide.md
│   └── Templates/
├── 01_Project-Policy/          프로젝트 규칙, 공식 용어, 수식 표기 규칙
├── 02_Planning/                전체 계획, 로드맵, 주차별 목표
│   ├── 00_Project-Overview/
│   │   ├── 0000_Project-Plan.md
│   │   └── 0001_Roadmap.md
│   ├── 03_Weekly-Overview/     전체 주차별 목표 요약
│   └── 04_Weekly-Details/
│       └── Week-XX/            주차별 상세 구현 계획
├── 03_Architecture/            전체 구조, 기능별 설계, 시스템 정의 수식
├── 04_ADR/                     설계 결정과 근거
├── 05_Development/
│   ├── Notes/                  구현 메모, 계산 순서, 최적화
│   ├── Experiments/            가설과 측정 결과
│   └── Debugging/              문제와 해결 기록
└── 06_Assets/
    ├── Images/                 스크린샷과 그림
    └── Documents/              원본 자료와 참고 문서
```

`01_Project-Policy/`에는 프로젝트에서 지킬 규칙과 용어의 공식 의미를 둔다. 수식의 기호·단위 같은 표기 규칙도 여기에 둔다. 프로젝트 범위와 로드맵은 `02_Planning/00_Project-Overview/0000_Project-Plan.md`와 `0001_Roadmap.md`, 주차별 목표 요약은 `03_Weekly-Overview/`, 주차별 상세 구현은 `04_Weekly-Details/`에 기록한다. 시스템을 정의하는 수식은 `03_Architecture/`, 실제 계산 순서와 최적화는 `05_Development/Notes/`에 기록한다.

## 문서 작성

- `Templates/`의 파일을 대상 디렉터리에 **복사**해 새 문서를 만든다. 템플릿 원본은 프로젝트 기록으로 사용하지 않는다.
- 문서 첫머리에 상태와 근거 자료를 적고, 미확정 사항은 확정된 내용과 구분한다.
- 설계 결정은 ADR에 이유와 대안을 남긴다. 실험에는 가설과 측정 조건·결과를, 디버깅 기록에는 재현 조건과 검증 결과를 남긴다.
- 문서·폴더 이름을 바꾸면 Obsidian 내부 링크도 함께 확인한다. 완료 여부는 구현이나 실험 결과를 확인한 뒤 `TODO.md`에 반영한다.

## 파일 번호

- 각 디렉터리의 문서와 참고 자료는 `0000_이름` 형식의 네 자리 번호로 정렬한다.
- 해당 디렉터리의 진입점, Overview, Guide, Index는 `0000`을 사용한다.
- 나머지 문서는 권장 읽기 또는 구현 순서대로 번호를 부여한다.
- ADR의 선행 번호는 문서 ID이므로 `0001-이름` 형식을 유지한다.
- 파일 번호가 바뀌면 Markdown 링크와 `.obsidian`의 최근 파일 경로도 함께 갱신한다.
