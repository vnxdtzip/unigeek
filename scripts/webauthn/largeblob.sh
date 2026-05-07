#!/usr/bin/env bash
# largeBlob (CTAP2 0x0C) + largeBlobKey extension (0x05/0x07 in MC/GA responses).
. "$(dirname "$0")/_lib.sh"

find_dev

case_header "GetInfo advertises largeBlobs" "options.largeBlobs=true + maxSerializedLargeBlobArray=4096"
fido2-token -I "$DEV" 2>/dev/null \
  | grep -E "largeBlob|maxSerializedLargeBlobArray|extensions" \
  || note "fido2-token output didn't surface largeBlob fields — re-check /-I version"

case_header "Read largeBlob array (initial state)" \
  "first read with no file: device synthesizes empty 0x80 + LEFT16(SHA256) sentinel"
if fido2-token -G -b "$TMPDIR_WA/lb_init.bin" "$DEV" 2>/dev/null; then
  ok
  hint "Initial blob ($(wc -c < "$TMPDIR_WA/lb_init.bin" | tr -d ' ') bytes):"
  xxd "$TMPDIR_WA/lb_init.bin" | head -3
  hint "Expect: 80 6e 34 0b 9c ff b3 7a 98 9c a5 44 e6 bb 78 0a 2c"
else
  note "fido2-token -G -b not in this libfido2 version"
  note "Fallback: python -c \"import fido2; ...\" (see comments below)"
fi

case_header "Register a cred with largeBlobKey extension" \
  "MC with -h flag asks for both hmac-secret AND largeBlobKey; response includes 0x05"
mk_cred_input "$TMPDIR_WA/mc_lb.in" "$RP_ID" lbuser user-lbuser
press_device
if fido2-cred -M -h -r -i "$TMPDIR_WA/mc_lb.in" "$DEV" > "$TMPDIR_WA/mc_lb.out" 2>&1; then
  ok
  hint "Serial log should show: 'MC ok: respLen=... +largeBlobKey'"
else
  bad 1
  cat "$TMPDIR_WA/mc_lb.out"
fi

note "Setting the array requires the host to encrypt with the largeBlobKey + build a"
note "hash-trailed CBOR blob. Easiest from python-fido2:"
cat <<'PY'
    from fido2.hid import CtapHidDevice
    from fido2.ctap2 import Ctap2, LargeBlobs
    dev = next(CtapHidDevice.list_devices())
    lb = LargeBlobs(Ctap2(dev))
    arr = lb.read_blob_array()
    print("entries:", len(arr))
PY
