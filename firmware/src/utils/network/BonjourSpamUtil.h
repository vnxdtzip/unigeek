#pragma once
#include <Arduino.h>

// Floods the LAN with fake DNS-SD service announcements over mDNS so
// nearby devices show phantom AirPlay speakers, Spotify Connect targets,
// Chromecasts, printers, and SMB workstations. Each phantom resolves to
// a fake hostname so connect attempts time out.
//
// AirDrop is intentionally NOT a category — AirDrop discovery uses Apple
// Wireless Direct Link (AWDL), which can't be spoofed from a Wi-Fi-only
// device.
class BonjourSpamUtil
{
public:
  enum Category : uint8_t {
    CAT_SPOTIFY,
    CAT_AIRPLAY,
    CAT_CAST,
    CAT_PRINTER,
    CAT_WORKSTATION,
    CAT_COUNT
  };

  static constexpr uint8_t INSTANCES_PER_CAT = 8;

  static const char* categoryLabel(Category c);

  static bool begin();
  static void end();
  static bool isRunning();

  static void setEnabled(Category c, bool on);
  static bool isEnabled(Category c);

  // Broadcast every enabled category once. Returns total packets sent.
  static uint8_t tick();

  static uint32_t packetsSent();                 // total across all categories
  static uint32_t packetsForCategory(Category c);
  static const char* lastInstance();             // c-string of last instance broadcast
  static Category    lastCategory();             // category of last broadcast
  static void     resetCounter();
};
