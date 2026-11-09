# Mandatory Documentation Workflow

> **모든 코드 작업 전 관련 문서를 실제로 읽고, 작업 후 영향받은 문서를 확인·갱신한다.** 매번 전체 볼트를 읽거나 수정하라는 뜻은 아니다.

## Before: 관련 문서 탐색과 열람

1. [`AGENTS.md`](../../AGENTS.md) + [`01_Document-Map.md`](../../01_Document-Map.md)를 읽는다.
2. 작업 유형, 소스 경로 패턴, 핵심 키워드·약어, 예상 변경 범위를 정한다.
3. [`Code Style`](../00_Core/00_Code-Style.md), 활성 언어 규칙, 관련 Requirements/Constraints/Glossary/Flows/Contracts/Source 및 검증 기록에서 **직접 필요한 부분**을 선택하고 읽는다.
4. 문서가 없거나 불일치하면 소스와 테스트를 확인하고, 문서 미비/불일치를 작업 범위에 포함한다.

토큰 사용을 줄이기 위해 **짧은 라우팅 맵 → 해당 문서의 관련 절 → 필요한 소스/테스트** 순으로 이동한다. 읽기 범위를 좁히되 필수 제약과 계약을 누락하지 않는다.

## During: 영향 추적

- 변경된 파일·함수·API·데이터·순서·상태·소유권과 영향받는 호출자·사용자를 확인한다.
- 요구사항/제약과 테스트 수용 기준을 만족하는지 점검한다.
- 새로운 용어, 변경된 모듈, 문서 위치 이동은 맵 갱신 대상으로 삼는다.
- 중요한 설계 선택의 근거가 새로 생기면 ADR 필요 여부를 결정한다.

## After: 문서 동기화

| 변경 종류 | 검토 및 갱신 대상 |
|---|---|
| 기능 추가·요구 변경 | [`Requirements`](../../01_Project-Context/02_Requirements.md), 관련 Flow, 테스트 |
| 기술 제약 변경 | [`Constraints`](../../01_Project-Context/03_Constraints.md), 관련 ADR |
| 용어/데이터 의미 변경 | [`Glossary`](../../01_Project-Context/01_Glossary.md), 관련 Contract |
| 실행 단계·순서 변경 | [`Main Flow`](../../02_Architecture/00_Flows/00_Main-Flow.md) 및 하위 Flow |
| GPU 셰이더·리소스 인터페이스 변경 | 해당 GLSL `layout` 선언·필수 주석, CPU Descriptor/Push Constant/Barrier 구현, [`Contract Register`](../../02_Architecture/01_Contracts/00_Contract-Register.md), 관련 Flow·테스트 |
| 공개 API·ABI·불변조건 변경 | [`Contract Register`](../../02_Architecture/01_Contracts/00_Contract-Register.md), Doxygen, 테스트 |
| 모듈 책임·경로·진입점 변경 | [`Source Map`](../../02_Architecture/02_Source/00_Source-Map.md), Document Map |
| 검증/성능 측정 | [`Validation & Tests`](../../05_Experiments/01_Validation-and-Tests.md), [`Performance`](../../05_Experiments/02_Performance-Measurements.md) |
| 주요 아키텍처 결정 | [`ADR Index`](../../03_Decisions/00_ADR-Index.md) 및 새 ADR |

- **코드 변경 작업마다 무조건** [`01_Work-Log.md`](../../04_Development/01_Work-Log.md)에 1건을 추가한다. 여러 파일의 하나의 논리적 작업은 한 건으로 묶는다.
- 다른 문서가 변경되지 않더라도 `영향 없음 — 근거`를 기록한다.
- 문서 내용은 한 위치를 정본으로 유지하고 다른 문서는 해당 문서를 링크한다.
- 문서 변경만 있는 작업은 Work Log가 필수는 아니지만 필요하면 기록한다.
- 검증하지 않은 내용·성능·수치를 확정 결과로 적지 않는다.

## 에이전트 종료 보고 (간단히)

`읽은 문서: … / 수정 코드: … / 문서 갱신: …(또는 영향 없음, 이유) / 검증: 실행한 명령·결과 또는 미실행 사유`
