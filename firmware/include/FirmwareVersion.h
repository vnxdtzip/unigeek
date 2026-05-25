#pragma once

// FIRMWARE_VERSION is injected by the release CI workflow as a build flag:
//   PLATFORMIO_BUILD_SRC_FLAGS: -DFIRMWARE_VERSION='"<git tag>"'
// Local PlatformIO builds don't set it, so fall back to "dev".
//
// Include this header anywhere you need the firmware version string instead
// of redefining the fallback in each translation unit.

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif
