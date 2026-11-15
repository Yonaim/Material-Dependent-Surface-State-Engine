#!/usr/bin/env bash
# 기존 빌드의 실행 파일을 찾아 명령행 인수를 전달한다.
set -euo pipefail

PROJECT_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

"$PROJECT_ROOT/Scripts/build.sh"
exec "$PROJECT_ROOT/Build/bin/MDSS" "$@"
