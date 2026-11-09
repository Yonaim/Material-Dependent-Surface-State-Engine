#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
APP="$PROJECT_ROOT/Build/bin/MDSS"

if [[ ! -x "$APP" ]]; then
    echo "Error: built executable not found at $APP. Run ./Scripts/build.sh first." >&2
    exit 1
fi

exec "$APP" "$@"
