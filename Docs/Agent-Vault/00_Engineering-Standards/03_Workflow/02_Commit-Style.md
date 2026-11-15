# Commit Message Style

기본 형식 (프로젝트 기존 규칙이 있으면 그것을 우선):

```text
<Type>(<Scope>): <한국어 변경 요약>
```

- Scope는 선택 사항이며 실제 모듈명·책임 범위를 사용한다. 없으면 `<Type>: <요약>`.
- 제목은 한 줄로, 간결하고 구체적인 변경을 적고 끝에 마침표를 붙이지 않는다.
- 커밋 유형의 범용 후보: `Feat`, `Fix`, `Docs`, `Style`, `Refactor`, `Perf`, `Test`, `Build`, `CI`, `Chore`, `Revert`.
- 기존 저장소가 Conventional Commits 등 다른 규칙을 사용하면 혼합하지 않고 기존 규칙을 따른다.
- 무관한 작업을 한 커밋에 섞지 않는다. 중요한 맥락은 필요할 때만 본문에 추가한다.
- 에이전트가 commit/push를 수행할 권한이 있는지는 별도 프로젝트 지침을 따른다. 규칙 문서만으로 자동 커밋 권한이 생기지 않는다.

예시:

```text
Feat(Renderer): 디버그 오버레이 추가
Fix: 잘못된 입력 인덱스 처리
Refactor(Simulation): 프레임 조율 함수 분리
Docs: API 제약사항 명확화
```
