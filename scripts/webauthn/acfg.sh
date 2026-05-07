#!/usr/bin/env bash
# AuthenticatorConfig (CTAP2 0x0D) — toggleAlwaysUv (0x02) + setMinPINLength (0x03).
# Requires: PIN already set (setpin.sh).
. "$(dirname "$0")/_lib.sh"

find_dev

case_header "ACfg setMinPINLength=8 (subcmd 0x03)" "raise the minimum PIN length from 4 to 8"
run_pin fido2-token -S -l 8 "$DEV"
hint "Re-check via getinfo.sh — minPINLength should now read 8"

case_header "ACfg toggleAlwaysUv (subcmd 0x02)" \
  "flip the alwaysUv flag — observable in GetInfo options"
run_pin fido2-token -S -a "$DEV"
hint "Re-check via getinfo.sh — options.alwaysUv should now be flipped"
