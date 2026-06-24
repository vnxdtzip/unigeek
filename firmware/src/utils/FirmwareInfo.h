#pragma once

// Build-time identity of the running firmware. Both macros are injected as build
// flags so the UART/BLE INFO response (and the About screen) can report them:
//   FIRMWARE_VERSION — by the release CI workflow (PLATFORMIO_BUILD_SRC_FLAGS).
//   FIRMWARE_BOARD   — by scripts/patch.py, set to the PlatformIO env name.
// The fallbacks below only apply to ad-hoc builds that bypass those.

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif

#ifndef FIRMWARE_BOARD
#define FIRMWARE_BOARD "unknown"
#endif
