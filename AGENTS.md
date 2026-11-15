# Project Agent Instructions

- **모든 코드 작업 전에** [`Docs/Agent-Vault/AGENTS.md`](Docs/Agent-Vault/AGENTS.md) 및 [`Docs/Agent-Vault/01_Document-Map.md`](Docs/Agent-Vault/01_Document-Map.md)를 읽고 해당 문서의 작업 절차를 따른다.
- 프로젝트 고유 용어·제약·GPU 인터페이스를 변경하기 전에 해당 **Contracts / Flows / Source**를 확인한다.
- 코드 변경 후 영향 문서를 검토·동기화하고 [`Work Log`](Docs/Agent-Vault/04_Development/01_Work-Log.md)에 기록한다.
- 기존 C++ 명명 관례를 무작정 새 프로필로 변경하지 않는다. 동작과 성능을 먼저 보존한다.
- 이 ZIP은 루트 `CMakeLists.txt` 없이 전달되었다. 완전한 프로젝트를 복구하기 전에는 전체 빌드 가능하다고 주장하지 않는다.
