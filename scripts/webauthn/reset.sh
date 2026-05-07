#!/usr/bin/env bash
# Reset the authenticator — wipes master + creds + counter + PIN + config + largeBlob.
. "$(dirname "$0")/_lib.sh"

find_dev

case_header "Reset (CTAP2 0x07)" "wipe everything; recover the device to a fresh state"

note "Spec: must be confirmed within 10 s of plug-in. Replug if you've been connected longer."
press_device
run fido2-token -R "$DEV"

hint "All previously registered credentials are now invalid (intended)."
hint "After this you'll need to run Manage WebAuthn > BIP39 Generate before any host can register."
