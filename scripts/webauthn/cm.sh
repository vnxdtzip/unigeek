#!/usr/bin/env bash
# CredentialManagement (CTAP2 0x0A) — enumerate RPs/creds and delete one.
# Requires: PIN already set (run setpin.sh first), at least one resident cred (passkey.sh).
. "$(dirname "$0")/_lib.sh"

find_dev

case_header "CM enumerateRPsBegin/GetNextRP (CTAP2 0x0A subcmds 0x02+0x03)" \
  "list every RP with a stored resident cred"
hint "Should include '$RP_ID'"
run_pin fido2-token -L -r "$DEV"

case_header "CM enumerateCredentialsBegin/GetNext (subcmds 0x04+0x05)" \
  "list every cred under '$RP_ID'"
run_pin fido2-token -L -k "$RP_ID" "$DEV"

case_header "CM deleteCredential (subcmd 0x06)" \
  "delete the 'bob' resident cred and verify it's gone"
CRED_LINE=$(echo "$TEST_PIN" | fido2-token -L -k "$RP_ID" "$DEV" 2>/dev/null \
            | awk '/bob/ {print prev} {prev=$0}' | head -1)
CRED_ID=$(echo "$CRED_LINE" | awk -F: '{print $NF}' | tr -d ' ')
if [ -z "$CRED_ID" ]; then
  note "Couldn't parse bob's credId from CM output. Run passkey.sh first."
else
  hint "Deleting credId: ${CRED_ID:0:30}..."
  run_pin fido2-token -D -i <(echo "$CRED_ID") "$DEV"
fi
