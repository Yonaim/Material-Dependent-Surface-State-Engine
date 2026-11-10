#!/usr/bin/env bash
# 성능 측정에 필요한 실행 환경을 검사하고 benchmark 작업을 시작한다.
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exec python3 "$SCRIPT_DIR/Performance/run_matrix.py" "$@"
