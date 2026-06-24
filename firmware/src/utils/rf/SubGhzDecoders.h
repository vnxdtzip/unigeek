//
// SubGhzDecoders — brand/manufacturer protocol decoders ported from Flipper
// Zero firmware (lib/subghz/protocols/*). Each decoder is a state machine fed
// the captured raw pulse train (alternating HIGH/LOW durations in µs); when a
// frame is recognised the Signal is labelled with the real protocol name
// (CAME, Princeton, Nice FLO, Holtek, Linear, ...) instead of staying RAW.
//
// Fed from CC1101Util::pollReceive() on a completed raw frame. The phase of the
// captured buffer (which edge is HIGH) is unknown, so decode() tries both
// parities and each state machine self-syncs on its own header.
//
// Reference: https://github.com/flipperdevices/flipperzero-firmware
//            lib/subghz/protocols/  (GPLv3)
//

#pragma once
#include <Arduino.h>
#include "CC1101Util.h"

class SubGhzDecoders {
public:
  // Run every brand decoder over a completed raw frame.
  //   dur   : array of pulse durations in µs (alternating HIGH/LOW)
  //   count : number of valid entries in `dur`
  // On a match, fills out.protocol (brand name) + key/bit/te and returns true.
  // out.rawData / out.frequency are left for the caller to populate.
  static bool decode(const unsigned int* dur, uint16_t count, CC1101Util::Signal& out);

  // Match produced by a single decoder.
  struct Match {
    const char* name = nullptr;
    uint64_t    key  = 0;
    uint8_t     bits = 0;
    uint16_t    te   = 0;
  };
};
