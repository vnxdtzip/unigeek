#!/usr/bin/env bash
# Change PIN (subcmd 0x04) plus negative test for too-short PIN.
. "$(dirname "$0")/_lib.sh"

find_dev

case_header "Negative: changePIN to 3-char PIN (default min=4)" "must FAIL with PIN_POLICY_VIOLATION"
echo "${C_DIM}  $ printf '%s\\n123\\n123\\n' \"$TEST_PIN\" | fido2-token -C $DEV  (expect failure)${C_RST}"
printf '%s\n%s\n%s\n' "$TEST_PIN" "123" "123" | fido2-token -C "$DEV" 2>&1 | tail -3
rc=${PIPESTATUS[1]}
if [ $rc -ne 0 ]; then
  ok
  hint "Correctly rejected (negative test passed)"
else
  bad 0
  hint "Device accepted a too-short PIN — should not happen"
fi

case_header "ClientPIN changePIN to '$TEST_PIN_NEW'" "valid change above the current minimum"
printf '%s\n%s\n%s\n' "$TEST_PIN" "$TEST_PIN_NEW" "$TEST_PIN_NEW" | fido2-token -C "$DEV"
rc=$?
if [ $rc -eq 0 ]; then
  ok
  hint "PIN updated. New TEST_PIN for subsequent scripts: $TEST_PIN_NEW"
  TEST_PIN="$TEST_PIN_NEW"
  export TEST_PIN
else
  bad $rc
fi
