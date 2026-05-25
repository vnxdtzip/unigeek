# Media / Camera Controls

Accessed from **HID > USB MouseKeyboard > Media / Camera** or **HID > BLE MouseKeyboard > Media / Camera**.

Send standard HID **Consumer Control** keys to the host — camera shutter, play/pause, track skip, volume, brightness, lock, eject — without typing.

## Setup

1. Open **HID** from the main menu
2. Pick **USB MouseKeyboard** (ESP32-S3 boards only) or **BLE MouseKeyboard**
3. For BLE: pair the device with the host (it advertises as `UniGeek`)
4. Select **Media / Camera**
5. Choose an action — it is sent as a single press/release

## Available actions

| Action | Usage code | Common host behaviour |
|---|---|---|
| **Camera Shutter** | `Vol+` (0x00E9) | Triggers shutter in the iOS Camera app and most Android camera apps (the standard BT-remote trick) |
| **Play / Pause** | 0x00CD | Universal media play/pause |
| **Next Track** | 0x00B5 | Skip to next track |
| **Previous Track** | 0x00B6 | Skip to previous track |
| **Stop** | 0x00B7 | Stop playback |
| **Fast Forward** | 0x00B3 | Seek forward |
| **Rewind** | 0x00B4 | Seek backward |
| **Volume Up** | 0x00E9 | Raise system volume |
| **Volume Down** | 0x00EA | Lower system volume |
| **Mute** | 0x00E2 | Toggle mute |
| **Brightness Up** | 0x006F | Raise display brightness (macOS / some Windows) |
| **Brightness Down** | 0x0070 | Lower display brightness |
| **Lock Screen** | `AL Lock` (0x019E) | Lock the host (macOS / ChromeOS; ignored by some Windows builds) |
| **Eject** | 0x00B8 | Eject optical media (mostly macOS) |

> [!tip]
> Camera Shutter is **Volume Up** under the hood. iOS and Android both bind hardware Vol+ to the shutter when the Camera app is in the foreground, so it works without any pairing tricks.

> [!note]
> USB mode requires an ESP32-S3 board (T-Lora Pager, Cardputer, Cardputer ADV, T-Display S3, CoreS3, StickC S3). All other boards must use BLE.

> [!note]
> Brightness, Lock, and Eject availability depends on the host OS. macOS responds to all three; Windows and Linux behaviour varies by build.

## Achievements

| Achievement | Tier |
|------------|------|
| **Remote Control** | Bronze |
