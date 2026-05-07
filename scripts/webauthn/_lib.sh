# Common helpers for the per-feature WebAuthn test scripts.
# Source from each test like:  . "$(dirname "$0")/_lib.sh"
#
# Each script can run standalone (it'll find the device + set up TMPDIR)
# or be orchestrated by scripts/webauthn/test-all.sh (which exports DEV
# and TMPDIR so the lib's auto-init becomes a no-op).
#
# Env overrides:
#   TEST_PIN        — PIN used by *-pin and CM/ACfg scripts (default 123456)
#   TEST_PIN_NEW    — PIN to switch to in changepin.sh    (default 12345678)
#   RP_ID           — relying party id for register/passkey (default test.example.com)
#   TMPDIR          — workdir for cred/assert files       (auto-mktemp if unset)

set -u

TEST_PIN="${TEST_PIN:-123456}"
TEST_PIN_NEW="${TEST_PIN_NEW:-12345678}"
RP_ID="${RP_ID:-test.example.com}"

# ── Colors ────────────────────────────────────────────────────────────
if [ -t 1 ]; then
  C_RED=$(tput setaf 1); C_GRN=$(tput setaf 2); C_YEL=$(tput setaf 3)
  C_BLU=$(tput setaf 4); C_DIM=$(tput dim);     C_BLD=$(tput bold)
  C_RST=$(tput sgr0)
else
  C_RED=""; C_GRN=""; C_YEL=""; C_BLU=""; C_DIM=""; C_BLD=""; C_RST=""
fi

case_header() {
  local title="$1" what="$2"
  echo
  echo "${C_BLU}${C_BLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${C_RST}"
  echo "${C_BLU}${C_BLD}  $title${C_RST}"
  echo "${C_BLU}  ${C_DIM}$what${C_RST}"
  echo "${C_BLU}${C_BLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${C_RST}"
}

note() { echo "${C_YEL}  ⚠  $*${C_RST}"; }
hint() { echo "${C_DIM}  ↳  $*${C_RST}"; }

ok()  { echo "${C_GRN}  ✓ PASS${C_RST}"; }
bad() { echo "${C_RED}  ✗ FAIL (rc=$1)${C_RST}"; }

run() {
  echo "${C_DIM}  $ $*${C_RST}"
  "$@"
  local rc=$?
  if [ $rc -eq 0 ]; then ok; else bad $rc; fi
  return $rc
}

# Run a command with the test PIN piped on stdin.
run_pin() {
  echo "${C_DIM}  $ echo PIN | $*${C_RST}"
  echo "$TEST_PIN" | "$@"
  local rc=$?
  if [ $rc -eq 0 ]; then ok; else bad $rc; fi
  return $rc
}

press_device() {
  echo "${C_YEL}  ⮕  Press the device button when prompted (will time out in 30 s)${C_RST}"
}

# Build a fido2-cred input file:
#   line 1: client_data_hash (base64)
#   line 2: relying_party_id
#   line 3: user_name
#   line 4: user_id (base64)
mk_cred_input() {
  local out="$1" rp="$2" uname="$3" uid="$4"
  {
    openssl rand 32 | base64
    echo "$rp"
    echo "$uname"
    echo -n "$uid" | base64
  } > "$out"
}

# Build a fido2-assert input file:
#   line 1: client_data_hash (base64)
#   line 2: relying_party_id
#   line 3: credential_id (base64) — optional, omit for empty allowList
mk_assert_input() {
  local out="$1" rp="$2" cid="${3:-}"
  {
    openssl rand 32 | base64
    echo "$rp"
    [ -n "$cid" ] && echo "$cid"
  } > "$out"
}

# Locate the FIDO device. Sets DEV (exported) and aborts if libfido2
# isn't installed or no device is connected.
find_dev() {
  if [ -n "${DEV:-}" ]; then return 0; fi
  if ! command -v fido2-token >/dev/null 2>&1; then
    echo "${C_RED}libfido2 not installed (need fido2-token / fido2-cred / fido2-assert)${C_RST}" >&2
    echo "${C_DIM}  macOS:  brew install libfido2${C_RST}" >&2
    echo "${C_DIM}  Linux:  apt install libfido2-dev libfido2-tools${C_RST}" >&2
    exit 1
  fi
  DEV="$(fido2-token -L | head -1 | awk -F: '{print $1":"$2}' | tr -d ' ')"
  if [ -z "$DEV" ]; then
    echo "${C_RED}No FIDO device found. Open HID > WebAuthn (USB) on the device.${C_RST}" >&2
    exit 1
  fi
  export DEV
  echo "${C_GRN}Device:${C_RST} $DEV"
}

# Workdir for temp inputs/outputs.
if [ -z "${TMPDIR_WA:-}" ]; then
  TMPDIR_WA="$(mktemp -d /tmp/unigeek-fido-XXXXXX)"
  export TMPDIR_WA
  # When this lib is sourced standalone (not from test-all.sh), trap cleanup.
  if [ -z "${WA_NO_CLEANUP:-}" ]; then
    trap 'rm -rf "$TMPDIR_WA"' EXIT
  fi
fi
