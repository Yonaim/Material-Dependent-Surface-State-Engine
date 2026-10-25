# 에이전트 문서

- 이 저장소의 작업 문서는 `Docs/MDSSP-Agent-Vault/`를 기준으로 한다. 작업을 시작할 때 `Docs/MDSSP-Agent-Vault/00_Home.md`를 읽는다. 시뮬레이션 변경은 `02_Architecture/`, 현행 선택 이유는 `03_Development/02_Current-Choices.md`, 진행 중인 일은 `03_Development/01_Current-Work.md`를 참고한다.
- 개인 볼트의 Decision·학습 기록은 역사적 자료다. 현재 동작은 코드·테스트와 에이전트 볼트에서 확인하고, 불일치하면 문서를 갱신한다.

# 성능 개선 기록
- 성능 최적화나 성능 비교 작업을 할 때는 먼저 `Docs/MDSSP-Agent-Vault/04_Experiments/02_Performance-Measurements.md`의 측정 규칙을 읽고 따른다.
- 실험 기록은 Codex가 작성한다. `Docs/MDSSP-Agent-Vault/01_Project-Policy/Templates/03_Experiment.md`를 기준으로 위 성능 문서에 실험 섹션을 추가하거나 기존의 같은 질문에 해당하는 섹션을 갱신한다. 실행마다 별도 설명 문서를 만들지 않는다.
- 최적화 전 기준 측정을 확보하고, 한 가지 변경 후 같은 조건으로 비교한다. 화면의 숫자를 사람이 옮겨 적는 대신 자동 출력한 원자료를 기준으로 표와 해석을 채운다.
- 실행 일시, 코드 버전, 실제 설정, 측정 구간, 결과 폴더, 정확성 확인 내용과 채택 여부를 기록한다. 사용자가 다시 이 정보를 정리하도록 넘기지 않는다.
- 미실행, 실행 실패, 지원하지 않는 계측 항목은 각각 그대로 표시한다. 예상 수치를 실측으로 쓰거나 개선 전 기록이 없는 결과에 개선율을 붙이지 않는다. 실제 성능을 측정하지 않은 문서·설정 정리 작업에는 실험 결과를 만들지 않는다.
- 기존 실험의 자동 산출물은 `Performance-Data/`에 둔다. 폴더 구성·현재 자료 색인은 `Performance-Data/README.md`를 따른다. 새 실험은 색인의 study/experiment 구조와 파일 역할을 따른다. 대부분의 raw는 Git에서 제외되며 저장소 문서 읽기 작업에 불러오지 않는다. 2026-10-04 두 실행의 JSONL 36개는 Git 추적 대상으로 남긴다. 실험을 재검증할 때 해당 실행의 요약과 manifest·raw만 선택해 확인한다.
