#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

"$PROJECT_ROOT/Scripts/build.sh"
exec "$PROJECT_ROOT/Build/bin/MDSS" "$@"
