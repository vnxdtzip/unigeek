"""Windows-native CTAPHID probe.

Bypasses the browser + webauthn.dll. Talks straight to any FIDO HID
device the OS has bound, sends INIT, prints GetInfo. If this works,
the device's CTAP layer is alive and the issue is at the browser /
webauthn.dll layer.

Run from PowerShell with the device on the WebAuthn screen:
    python scripts/webauthn/win_probe.py
"""
from __future__ import annotations

import sys

from fido2.hid import CtapHidDevice
from fido2.ctap2 import Ctap2


def list_all_hid_windows():
    """Enumerate ALL HID devices on Windows via SetupAPI WITHOUT the
    FIDO-usage-page filter. Returns a list of dicts (path, vid, pid,
    usage_page, usage, error) for every HID interface visible to this
    process. Lets us tell the difference between:
      - device hidden from user-mode (path missing entirely)
      - device visible but FIDO usage page not advertised (firmware bug)
      - device visible + FIDO usage page set but fido2 still rejects (filter bug)
    """
    import ctypes
    from ctypes import wintypes
    from fido2.hid.windows import (
        DeviceInterfaceData, DeviceInterfaceDetailData,
        DIGCF_DEVICEINTERFACE, DIGCF_PRESENT,
        FILE_SHARE_READ, FILE_SHARE_WRITE, OPEN_EXISTING,
        INVALID_HANDLE_VALUE, GUID, HidCapabilities, PHIDP_PREPARSED_DATA,
        HIDP_STATUS_SUCCESS, hid, kernel32, setupapi,
        get_vid_pid,
    )

    out = []
    hid_guid = GUID()
    hid.HidD_GetHidGuid(ctypes.byref(hid_guid))
    collection = setupapi.SetupDiGetClassDevsA(
        ctypes.byref(hid_guid), None, None, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT
    )
    try:
        idx = 0
        info = DeviceInterfaceData()
        info.cbSize = ctypes.sizeof(DeviceInterfaceData)
        while True:
            ok = setupapi.SetupDiEnumDeviceInterfaces(
                collection, 0, ctypes.byref(hid_guid), idx, ctypes.byref(info)
            )
            idx += 1
            if not ok:
                break

            dw_len = wintypes.DWORD()
            setupapi.SetupDiGetDeviceInterfaceDetailA(
                collection, ctypes.byref(info), None, 0,
                ctypes.byref(dw_len), None,
            )
            if dw_len.value == 0:
                continue
            buf = ctypes.create_string_buffer(dw_len.value)
            detail = DeviceInterfaceDetailData.from_buffer(buf)
            detail.cbSize = ctypes.sizeof(DeviceInterfaceDetailData)
            ok = setupapi.SetupDiGetDeviceInterfaceDetailA(
                collection, ctypes.byref(info),
                ctypes.byref(detail), dw_len.value, None, None,
            )
            if not ok:
                continue
            path = ctypes.string_at(detail.DevicePath)

            entry = {"path": path.decode("ascii", "replace"),
                     "vid": None, "pid": None,
                     "usage_page": None, "usage": None,
                     "error": None}

            dev = kernel32.CreateFileA(
                path, 0,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                None, OPEN_EXISTING, 0, None,
            )
            if dev == INVALID_HANDLE_VALUE:
                entry["error"] = "CreateFile failed (handle denied)"
                out.append(entry)
                continue
            try:
                pp = PHIDP_PREPARSED_DATA(0)
                if not hid.HidD_GetPreparsedData(dev, ctypes.byref(pp)):
                    entry["error"] = "HidD_GetPreparsedData failed"
                    out.append(entry)
                    continue
                try:
                    caps = HidCapabilities()
                    if hid.HidP_GetCaps(pp, ctypes.byref(caps)) != HIDP_STATUS_SUCCESS:
                        entry["error"] = "HidP_GetCaps failed"
                        out.append(entry)
                        continue
                    entry["usage_page"] = caps.UsagePage
                    entry["usage"] = caps.Usage
                    try:
                        vid, pid = get_vid_pid(dev)
                        entry["vid"], entry["pid"] = vid, pid
                    except Exception as e:
                        entry["error"] = f"get_vid_pid: {e}"
                    out.append(entry)
                finally:
                    hid.HidD_FreePreparsedData(pp)
            finally:
                kernel32.CloseHandle(dev)
    finally:
        setupapi.SetupDiDestroyDeviceInfoList(collection)
    return out


def main() -> int:
    raw = list_all_hid_windows()
    print(f"=== ALL HID interfaces visible to this process: {len(raw)} ===")
    matches = []
    for d in raw:
        vidpid = ""
        if d["vid"] is not None:
            vidpid = f" VID={d['vid']:04x} PID={d['pid']:04x}"
        usage = ""
        if d["usage_page"] is not None:
            usage = f" UP={d['usage_page']:04x} U={d['usage']:04x}"
        err = f"  [{d['error']}]" if d["error"] else ""
        is_303a = (d["vid"] == 0x303A) if d["vid"] is not None else False
        is_fido = (d["usage_page"] == 0xF1D0) if d["usage_page"] is not None else False
        marker = ""
        if is_303a:
            marker += " 303A!"
            matches.append(d)
        if is_fido:
            marker += " FIDO!"
        print(f"  {d['path'][:90]}{vidpid}{usage}{err}{marker}")

    print(f"\n=== matches for our device (VID 303A): {len(matches)} ===")
    for m in matches:
        print(f"  {m}")

    devs = list(CtapHidDevice.list_devices())
    print(f"\n=== FIDO HID devices fido2 sees (UP=F1D0, U=01): {len(devs)} ===")
    for d in devs:
        print(f"  - {d.descriptor}")
    if not devs:
        print("\nFAIL: no FIDO HID devices.")
        print("Check: WebAuthn screen open on device, USB cable plugged in,")
        print("Device Manager shows 'HID-compliant fido' under HID.")
        return 1

    target = None
    for d in devs:
        path = repr(d.descriptor).lower()
        if "vid_303a" in path or "303a" in path:
            target = d
            break
    if target is None:
        target = devs[0]
        print(f"\n(no 303A match, using first device)")

    print(f"\nopening: {target.descriptor}")
    try:
        ctap2 = Ctap2(target)
    except Exception as e:
        print(f"FAIL: Ctap2(dev) raised: {e!r}")
        return 2

    info = ctap2.info
    print("\n=== CTAP2 GetInfo ===")
    print(f"versions:           {info.versions}")
    print(f"extensions:         {info.extensions}")
    print(f"aaguid:             {info.aaguid.hex()}")
    print(f"options:            {info.options}")
    print(f"max_msg_size:       {info.max_msg_size}")
    print(f"pin_uv_protocols:   {info.pin_uv_protocols}")
    print(f"transports:         {info.transports}")
    print(f"algorithms:         {info.algorithms}")
    print(f"max_large_blob:     {info.max_large_blob}")
    print(f"min_pin_length:     {info.min_pin_length}")
    print("\nPASS: device responded to CTAP2 GetInfo")
    return 0


if __name__ == "__main__":
    sys.exit(main())