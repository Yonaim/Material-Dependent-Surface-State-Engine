# 커밋 메시지 작성 규칙

커밋 메시지는 변경 내용을 짧고 일관되게 찾을 수 있도록 아래 형식을 따른다.

## 형식

```text
<Type>: <lowercase description>
```

- `Type`은 아래 표에 있는 키워드만 사용한다. 임의의 키워드는 추가하지 않는다.
- 키워드는 첫 글자만 대문자로 쓴다. 예: `Feat`, `Docs`, `Chore`.
- 콜론 뒤 설명은 소문자로 시작한다.
- 설명은 변경 사항을 구체적이고 간결하게 나타내며, 끝에 마침표를 붙이지 않는다.
- 제목은 한 줄로 작성한다. 추가 맥락이 꼭 필요할 때만 빈 줄 뒤에 본문을 쓴다.

## 허용 키워드

| 키워드 | 사용 목적 | 예시 |
|---|---|---|
| `Feat` | 사용자에게 보이는 기능 또는 엔진 기능 추가 | `Feat: add surface state debug view` |
| `Fix` | 잘못된 동작이나 결함 수정 | `Fix: prevent invalid surface neighbor access` |
| `Docs` | 문서 추가·수정 | `Docs: document surface mapping decisions` |
| `Style` | 동작 변경 없는 코드 형식·표기 정리 | `Style: format surface state sources` |
| `Refactor` | 외부 동작을 바꾸지 않는 코드 구조 개선 | `Refactor: separate profile validation logic` |
| `Perf` | 측정 또는 명확한 근거가 있는 성능 개선 | `Perf: reduce surface buffer upload cost` |
| `Test` | 테스트 또는 테스트 데이터 변경 | `Test: cover missing profile state fields` |
| `Build` | 빌드 시스템·컴파일 설정 변경 | `Build: add surface state test target` |
| `CI` | 지속적 통합 및 자동화 파이프라인 변경 | `CI: run contract tests on pull requests` |
| `Chore` | 기능·문서·테스트 변경에 속하지 않는 유지보수 | `Chore: update third-party dependency` |
| `Revert` | 이전 커밋 되돌리기 | `Revert: remove surface debug overlay` |

한 커밋에 여러 종류의 변경이 섞이면 핵심 변경을 기준으로 키워드 하나를 고른다. 서로 독립적인 변경은 별도 커밋으로 나눈다.

## 예시

```text
Feat: add shared surface geometry data
Docs: clarify weekly implementation scope
Refactor: extract SRProfile validation
```

기존 기록 중 `Add`, `Update`, `Revise`처럼 키워드와 콜론 형식을 따르지 않는 메시지는 과거 기록으로 유지한다. 새 커밋에서는 허용 키워드와 위 형식을 사용한다.
