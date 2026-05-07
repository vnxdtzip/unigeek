#!/usr/bin/env bash
# Dump GetInfo and confirm the options/extensions/top-level fields we expect.
. "$(dirname "$0")/_lib.sh"

find_dev

case_header "GetInfo (CTAP2 0x04)" \
  "AAGUID + extensions + options + algorithms + maxSerializedLargeBlobArray + minPINLength"

hint "Expect: aaguid e96b5d29-4318-4c6e-8f8f-a4a5e2b3c1d0"
hint "Expect extensions: hmac-secret, largeBlobKey"
hint "Expect options: rk, up, alwaysUv, credMgmt, authnrCfg, clientPin, largeBlobs, pinUvAuthToken, setMinPINLength"
hint "Expect maxSerializedLargeBlobArray: 4096"
hint "Expect minPINLength: 4 (or whatever ACfg has raised it to)"

run fido2-token -I "$DEV"
