#!/usr/bin/env bash
# scripts/serve.sh - Build the website and serve the static export locally.
#
# Usage:
#   ./scripts/serve.sh           # http://localhost:3000
#   ./scripts/serve.sh -p 4000   # custom port

set -euo pipefail

PORT=3000
while [[ $# -gt 0 ]]; do
  case "$1" in
    -p|--port) PORT="$2"; shift 2 ;;
    *) echo "Unknown arg: $1" >&2; exit 1 ;;
  esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SITE_DIR="$(cd "$SCRIPT_DIR/../website" && pwd)"

cd "$SITE_DIR"
npm run build

echo "Serving website/build/ on http://localhost:$PORT"
npx --yes serve@latest build -l "$PORT"
