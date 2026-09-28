# 볼트 사용 가이드

> **한 줄 요약:** 이 가이드는 프로젝트마다 같은 문서 구조를 사용할 때 참고하는 공통 안내다.

이 가이드는 프로젝트마다 같은 문서 구조를 사용할 때 참고하는 공통 안내다. `00_Start/`에는 이 가이드와 문서 템플릿만 둔다. 프로젝트 고유의 정책, 설계, 작업 현황은 해당 위치에 기록하고 이 폴더의 원본은 수정하지 않는다.

## 시작하기

1. 프로젝트의 볼트 폴더를 Obsidian에서 **Open folder as vault**로 연다.
2. `01_Project-Policy/`에서 공식 용어와 작성·개발 규칙을 확인한다.
3. `02_Research/0000_Research-Index.md`에서 관련 연구와 프로젝트 적용 검토를 확인한다.
4. `03_Planning/00_Project-Overview/0000_Project-Plan.md`와 `0001_Roadmap.md`에서 범위와 일정을 확인한다.
5. `04_Architecture/0000_Overview.md`에서 시스템 구성을 파악하고 `05_ADR/README.md`에서 주제별 ADR 색인을 확인한다.
6. 실제 구현 메모·실험은 `06_Development/`, 테스트 기준·결과는 `07_Testing/`, 원본 자료는 `08_Assets/`에서 확인한다.
7. 진행할 작업과 완료 여부는 볼트 루트의 `TODO.md`에서 확인한다.

## 문서 구조

```text
프로젝트 볼트/
├── TODO.md                      프로젝트별 작업 목록
├── 00_Start/                   공통 가이드와 재사용 템플릿
│   ├── 0000_Vault-Guide.md
│   ├── 0001_Document-Writing-Guide.md
│   └── Templates/
├── 01_Project-Policy/          프로젝트 규칙, 공식 용어, 수식 표기 규칙
├── 02_Research/                관련 연구·기술 근거, 쉬운 요약·상세 설명·적용 검토
├── 03_Planning/                전체 계획, 로드맵, 주차별 목표
│   ├── 00_Project-Overview/
│   │   ├── 0000_Project-Plan.md
│   │   └── 0001_Roadmap.md
│   ├── 01_Weekly-Overview/     전체 주차별 목표 요약
│   └── 02_Weekly-Details/
│       └── Week-XX/            주차별 상세 구현 계획
├── 04_Architecture/            전체 구조, 기능별 설계, 시스템 정의 수식
├── 05_ADR/                     설계 결정과 근거
│   ├── Architecture/           시스템 구조와 GPU/API 계약
│   ├── Assets/                 에셋 연결과 전처리
│   ├── Rendering/              렌더링 결정
│   └── Simulation/             상태, 형상과 Solver 결정
├── 06_Development/
│   ├── Notes/                  구현 메모, 계산 순서, 최적화
│   ├── Experiments/            가설과 측정 결과
│   └── Debugging/              문제와 해결 기록
├── 07_Testing/               테스트 계획, 회귀 기준과 테스트 결과
└── 08_Assets/
    ├── Images/                 스크린샷과 그림
    └── Documents/              원본 자료와 참고 문서
```

`01_Project-Policy/`에는 프로젝트에서 지킬 규칙과 용어의 공식 의미를 둔다. `02_Research/`에는 관련 연구의 쉬운 요약과 상세 설명, 프로젝트 적용 검토를 둔다. 수식의 기호·단위 같은 표기 규칙도 여기에 둔다. 프로젝트 범위와 로드맵은 `03_Planning/00_Project-Overview/0000_Project-Plan.md`와 `0001_Roadmap.md`, 주차별 목표 요약은 `01_Weekly-Overview/`, 주차별 상세 구현은 `02_Weekly-Details/`에 기록한다. 시스템을 정의하는 수식은 `04_Architecture/`, 실제 계산 순서와 최적화는 `06_Development/Notes/`, 공통 테스트 기준은 `07_Testing/`에 기록한다. 원본 참고 자료는 `08_Assets/`에 둔다.

## 문서 작성

- 표, 문단, 순서도와 code block의 선택 기준은 [[00_Start/0001_Document-Writing-Guide|문서 작성 가이드]]를 따른다.
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
