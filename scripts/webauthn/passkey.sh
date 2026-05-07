#!/usr/bin/env bash
# Resident credentials (rk=true) + empty-allowList GetAssertion.
# Registers two passkeys for the same RP, then signs in without typing a username.
# Implicitly tests GetNextAssertion (0x08) — the second cred comes back via GNA.
. "$(dirname "$0")/_lib.sh"

find_dev

case_header "MakeCredential resident, user 'alice' (rk=true)" \
  "passkey #1 stored on /unigeek/utility/fido/credentials/"
mk_cred_input "$TMPDIR_WA/mc_a.in" "$RP_ID" alice user-alice
press_device
run fido2-cred -M -r -i "$TMPDIR_WA/mc_a.in" "$DEV" > "$TMPDIR_WA/mc_a.out"
hint "Serial log expected: 'MC: resident cred written for rpId=$RP_ID'"

case_header "MakeCredential resident, user 'bob' (rk=true, same RP)" \
  "passkey #2 — sets up the GetNextAssertion test below"
mk_cred_input "$TMPDIR_WA/mc_b.in" "$RP_ID" bob user-bob
press_device
run fido2-cred -M -r -i "$TMPDIR_WA/mc_b.in" "$DEV" > "$TMPDIR_WA/mc_b.out"

case_header "GetAssertion empty allowList (CTAP2 0x02 + 0x08 GNA)" \
  "discoverable signin: GA returns alice + numberOfCredentials=2, GNA returns bob"
mk_assert_input "$TMPDIR_WA/ga_empty.in" "$RP_ID"
press_device
hint "Expected output: TWO 'credential id:' blocks (one per resident cred)"
run fido2-assert -G -i "$TMPDIR_WA/ga_empty.in" "$DEV"
hint "Serial log expected: 'GA: GNA armed credIdx=1/2' then 'GNA ok: idx=2/2'"
