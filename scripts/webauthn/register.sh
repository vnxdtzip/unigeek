#!/usr/bin/env bash
# Basic non-resident MakeCredential + GetAssertion-with-allowList round trip.
# Verifies the device can register a server-side cred and prove possession.
. "$(dirname "$0")/_lib.sh"

find_dev

case_header "MakeCredential non-resident (CTAP2 0x01)" "basic register, no rk, no PIN"
mk_cred_input "$TMPDIR_WA/mc1.in" "$RP_ID" alice user-alice
press_device
run fido2-cred -M -i "$TMPDIR_WA/mc1.in" "$DEV" > "$TMPDIR_WA/mc1.out"

if [ -s "$TMPDIR_WA/mc1.out" ]; then
  fido2-cred -V -i "$TMPDIR_WA/mc1.out" > "$TMPDIR_WA/mc1.verified" 2>&1
  awk 'NR==1' "$TMPDIR_WA/mc1.verified" > "$TMPDIR_WA/mc1.credid"
  hint "credId: $(awk 'NR==1 {print substr($0,1,40)"..."}' "$TMPDIR_WA/mc1.credid" 2>/dev/null)"
fi

case_header "GetAssertion with allowList (CTAP2 0x02)" "non-discoverable signin against the freshly registered cred"
if [ ! -s "$TMPDIR_WA/mc1.credid" ]; then
  note "Skipping — no credId from MakeCredential"
else
  mk_assert_input "$TMPDIR_WA/ga1.in" "$RP_ID" "$(cat "$TMPDIR_WA/mc1.credid")"
  press_device
  run fido2-assert -G -i "$TMPDIR_WA/ga1.in" "$DEV"
fi
