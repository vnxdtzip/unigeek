#pragma once

#include "ui/templates/BaseScreen.h"
#include <NimBLEDevice.h>

// Microsoft "Swift Pair" BLE advertisement spam — pops the "New device found"
// toast on Windows 10/11 with Swift Pair enabled.
class BLEWindowsSpamScreen : public BaseScreen {
public:
  const char* title()    override { return "Windows Spam"; }
  bool inhibitPowerOff() override { return true; }

  ~BLEWindowsSpamScreen() override;
  void onInit()   override;
  void onUpdate() override;
  void onRender() override;

private:
  static constexpr const char* _spinner = "-\\|/";
  uint8_t  _spinIdx     = 0;
  uint32_t _lastSpamMs  = 0;
  uint32_t _lastDrawMs  = 0;
  uint32_t _spamStartMs = 0;
  bool     _spam1minFired = false;
  bool     _chromeDrawn   = false;

  NimBLEAdvertising* _pAdv = nullptr;

  void _spam();
  void _stop();
};
