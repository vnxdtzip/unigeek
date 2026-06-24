//
// RF Reaper — WS2812B RGB ring (16 px)
//
// Selectable patterns driven from Device::boardHook(). Mirrors the T-Embed
// ring; the "encoder" pattern is a no-op scroll here (no rotary encoder), so
// it renders a static rainbow.
//

#pragma once

class LedRing {
public:
  void begin();                        // idempotent FastLED init
  void update();                       // call every frame; self-throttles to ~40 ms

  // Fed by NavigationImpl whenever an encoder emits a step (+1 = CW, -1 = CCW).
  // Unused on the button-only Reaper, kept for cross-board source parity.
  static void addEncoderDelta(int steps);

private:
  bool _began = false;
};

extern LedRing ledRing;
