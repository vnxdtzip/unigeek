#!/usr/bin/env bash
# Orchestrator: run every per-feature WebAuthn test in dependency order.
#
# Usage:
#   ./test-all.sh                # full sweep
#   ./test-all.sh --skip-pin     # stop before setpin.sh (useful for fresh-device dev loops)
#   ./test-all.sh --no-reset     # don't reset at the start or end
#
# Each child script can also be run standalone — they all source _lib.sh
# and call find_dev. Override TEST_PIN / TEST_PIN_NEW / RP_ID via env.

set -u
DIR="$(cd "$(dirname "$0")" && pwd)"
. "$DIR/_lib.sh"

# Persist workdir for the whole sweep so per-script `mk_*_input` outputs
# survive across files (cm.sh consumes passkey.sh's resident creds).
export WA_NO_CLEANUP=1
trap 'rm -rf "$TMPDIR_WA"' EXIT

SKIP_PIN=0
NO_RESET=0
while [ $# -gt 0 ]; do
  case "$1" in
    --skip-pin) SKIP_PIN=1; shift ;;
    --no-reset) NO_RESET=1; shift ;;
    *) echo "Unknown arg: $1"; exit 2 ;;
  esac
done

find_dev

# Section header banner
section() {
  echo
  echo "${C_BLD}════════════════════════════════════════════════════════════════════════════════${C_RST}"
  echo "${C_BLD}  $1${C_RST}"
  echo "${C_BLD}════════════════════════════════════════════════════════════════════════════════${C_RST}"
}

run_step() {
  local script="$DIR/$1"
  if [ ! -x "$script" ]; then
    note "skipping (not executable): $1"
    return
  fi
  bash "$script" || true   # don't abort the sweep on a single failure
}

# ── Phase 1: discovery + clean slate ──────────────────────────────────
section "PHASE 1 — discovery + clean slate"
run_step "getinfo.sh"
[ "$NO_RESET" -eq 0 ] && run_step "reset.sh"

# ── Phase 2: no-PIN tests ─────────────────────────────────────────────
section "PHASE 2 — register / passkey / hmac-secret / largeBlob (no PIN required)"
run_step "register.sh"
run_step "passkey.sh"
run_step "hmac-secret.sh"
run_step "largeblob.sh"

# ── Phase 3: PIN-protected tests ──────────────────────────────────────
if [ "$SKIP_PIN" -eq 1 ]; then
  note "Skipping PIN/CM/ACfg phases (--skip-pin)"
else
  section "PHASE 3 — set PIN, exercise CM and AuthenticatorConfig"
  run_step "setpin.sh"
  run_step "cm.sh"
  run_step "acfg.sh"
  run_step "changepin.sh"
fi

# ── Phase 4: clean up ─────────────────────────────────────────────────
if [ "$NO_RESET" -eq 0 ]; then
  section "PHASE 4 — final reset"
  run_step "reset.sh"
fi

echo
echo "${C_BLD}═══ done ═══${C_RST}"
