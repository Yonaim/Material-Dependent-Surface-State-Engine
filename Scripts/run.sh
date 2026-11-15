#!/usr/bin/env bash
# 기존 빌드의 실행 파일을 찾아 명령행 인수를 전달한다.
set -euo pipefail

PROJECT_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
APP="$PROJECT_ROOT/Build/bin/MDSS"

if [[ ! -x "$APP" ]]; then
    echo "Error: built executable not found at $APP. Run ./Scripts/build.sh first." >&2
    exit 1
fi

exec "$APP" "$@"
