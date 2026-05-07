#!/usr/bin/env bash
# Set the initial PIN (subcmd 0x03). Fails if a PIN is already set — use changepin.sh.
. "$(dirname "$0")/_lib.sh"

find_dev

case_header "ClientPIN setPIN (subcmd 0x03)" "set initial PIN to '$TEST_PIN'"
hint "fido2-token -S prompts twice for the new PIN; we pipe both via stdin"
printf '%s\n%s\n' "$TEST_PIN" "$TEST_PIN" | fido2-token -S "$DEV"
rc=$?
if [ $rc -eq 0 ]; then ok; else bad $rc; fi

case_header "ClientPIN getPINRetries (subcmd 0x01)" "GetInfo shows 'pin retries: 8'"
run fido2-token -I "$DEV" | grep -E "pin retries|clientPin"
