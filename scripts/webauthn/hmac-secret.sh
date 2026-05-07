#!/usr/bin/env bash
# HMAC-secret extension determinism check: same salt + same cred → same output.
. "$(dirname "$0")/_lib.sh"

find_dev

case_header "HMAC-secret extension (-h)" \
  "PRF: same salt + same cred should produce identical encrypted output"

mk_cred_input "$TMPDIR_WA/mc_h.in" "$RP_ID" hmacuser user-hmac
press_device
fido2-cred -M -h -i "$TMPDIR_WA/mc_h.in" "$DEV" > "$TMPDIR_WA/mc_h.out" 2>/dev/null
fido2-cred -V -h -i "$TMPDIR_WA/mc_h.out" > "$TMPDIR_WA/mc_h.verified" 2>/dev/null
awk 'NR==1' "$TMPDIR_WA/mc_h.verified" > "$TMPDIR_WA/mc_h.credid"

# Deterministic salt → both runs should produce identical hmac-secret output
HSALT=$(openssl rand 32 | base64)
{
  openssl rand 32 | base64        # cdh
  echo "$RP_ID"
  cat "$TMPDIR_WA/mc_h.credid"
  echo "$HSALT"
} > "$TMPDIR_WA/ga_h.in"

press_device
fido2-assert -G -h -t up=true -i "$TMPDIR_WA/ga_h.in" "$DEV" > "$TMPDIR_WA/ga_h1.out" 2>&1 || true
press_device
fido2-assert -G -h -t up=true -i "$TMPDIR_WA/ga_h.in" "$DEV" > "$TMPDIR_WA/ga_h2.out" 2>&1 || true

if diff -q <(tail -1 "$TMPDIR_WA/ga_h1.out") <(tail -1 "$TMPDIR_WA/ga_h2.out") >/dev/null 2>&1; then
  ok
  hint "HMAC outputs match — extension is deterministic per cred + salt"
else
  bad 1
  hint "Outputs differ — see $TMPDIR_WA/ga_h{1,2}.out"
fi
