# MDSSP Agent Vault 병합 변경 기록

2026-11-11. **문서 자료 병합만 진행**, 엔진 소스·셰이더·GPU를 빌드/실행하지 않음.

- `Universal-Agent-Vault.zip`을 기준으로 루트, `00_Engineering-Standards/` (Assets/Build, Assets/Formatting 포함), Obsidian 환경과 공통 규칙 유지. `.git/` 내부 이력은 배포 ZIP에서 제외(새 저장소가 기존 범용 템플릿 Git에 연결되지 않도록).
- `01_Project-Context.zip`의 실제 4개 MDSS 문서 적용.
- `Archive.zip`의 `02_Architecture` 51개 원문을 가져오고 System Map→04, Code Reading Guide→05로 번호 조정; `02_Architecture/03_Decisions.md`로 결정 요약 통합.
- `03_Decisions/`, `06_Research/` 폴더 삭제. 연구 4개 문서와 `04_Development/04`~`08`의 연구 프로토콜은 개인 볼트로 이관.
- `04_Development/00_Current-Work.md`에는 구현/검증 대기인 GPU A/B 작업만 남김. Wetting과 Gravity Cache의 미채택 아이디어는 개인 볼트로 이동.
- 기존 긴 실험 자료 2개는 `05_Experiments/Records/`에 출처와 수치를 보존하고, 상위 01/02는 가벼운 탐색·검증 인덱스로 변경.
- 개발 이관 감사 기록(`04_Development/09`~`16`)은 본 배포물에 포함하지 않음. 원본 `Archive.zip`에서 복원 가능.
- Markdown 링크는 새 경로로 대체했으며, 파일 존재 여부는 패키징 시 재검사함.
