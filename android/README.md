# UniGeek — Android companion

Kotlin + Jetpack Compose app that mirrors the website's three tools, in the same
engineering-dark visual language:

| Tool | Transport | Notes |
|---|---|---|
| **Remote** | USB-OTG serial | Screen-mirror (ctx `S`) + D-pad / touch / keyboard control. |
| **File Manager** | USB-OTG serial **or** BLE (Nordic UART) | Browse / download / upload / edit / rename / delete / mkdir. |
| **Firmware Update** | USB-OTG serial | Flashes the per-board `.bin` from the CDN via a ROM-loader port of esptool. |

All three speak the exact same wire protocol as the firmware and the web clients
(`A5 5A | ctx type seq | len | payload | crc32`), so a device that works with the
website works here unchanged.

## Build

Open the `android/` folder in **Android Studio** (Koala or newer) and let it sync —
that generates the Gradle wrapper and `local.properties` for you. Then Run.

Command line (after a wrapper exists):

    ./gradlew :app:assembleDebug
    ./gradlew :app:installDebug

Toolchain: AGP 8.5.2 · Gradle 8.7 · Kotlin 1.9.24 · Compose Compiler 1.5.14 ·
JDK 17 · minSdk 26 · targetSdk 34. `usb-serial-for-android` comes from JitPack
(repo already wired in `settings.gradle.kts`).

> The binary `gradle/wrapper/gradle-wrapper.jar` is not committed. Android Studio
> creates it on first sync, or run `gradle wrapper --gradle-version 8.7` once with a
> system Gradle.

## How it maps to the website

- **Wire protocol** — `protocol/` (`Proto`, `Crc32`, `encodeFrame`, `FrameParser`)
  is a 1:1 port of the framing in `RemoteAccessClient.js` / `FileManagerClient.js`.
- **Transports** — `transport/` has `UsbSerialTransport` (async reader) and
  `BleTransport` (native GATT over NUS). The flasher drives the USB port directly
  because it needs synchronous reads + DTR/RTS.
- **Firmware** — `feature/install/Firmware.kt` resolves `.bin`s through the same
  `bin-unigeek.xid.run` proxy and pulls the version→board map live from
  `release-notes/_boards.json`. `Boards.kt` mirrors `website/content/boards.js`.

## Known caveats (v1)

- **Flasher is stub-less + uncompressed.** It uses the ROM loader directly (no
  esptool stub, no deflate), so flashing is slower than the website but has no
  embedded chip blobs. It validates the chip family (S3 vs not) before writing.
- **Native-USB-CDC boards** (most ESP32-S3) may ignore the auto-reset lines. If
  sync fails, the app tells the user to enter download mode manually
  (hold BOOT, tap RESET) and press Flash again.
- **Remote is USB-only** — the firmware only mirrors the screen over UART.
- **Fonts** — Geist / Geist Mono aren't bundled; the app falls back to the system
  sans + monospace. Drop the TTFs into `app/src/main/res/font` and point `Type.kt`
  at them for pixel-parity with the site.
- Downloaded files land in `Downloads/` (Android 10+) or the app's external files
  dir on older releases.
