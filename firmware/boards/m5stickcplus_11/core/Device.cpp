//
// Created by L Shaf on 2026-02-16.
//

#include "core/Device.h"
 #include "core/ConfigManager.h"
#include "Navigation.h"
#include "EncoderNavigation.h"
#include "Display.h"
#include "Power.h"
#include "Speaker.h"
#include "lib/AXP192.h"
#include <Wire.h>

AXP192 axp;

static DisplayImpl      display(&axp);
static NavigationImpl   navigation(&axp);
static EncoderNavigation encoderNavigation(&axp);
static PowerImpl        power(&axp);
static SpeakerBuzzer    speaker;
static ExtSpiClass      extSpi(VSPI);  // Grove port SPI (display uses HSPI)

Device* Device::createInstance() {
  pinMode(BTN_B, INPUT_PULLUP);
  pinMode(BTN_A, INPUT_PULLUP);
  Wire1.begin(INTERNAL_SDA, INTERNAL_SCL);  // Wire1: AXP192 + BM8563 share same internal I2C bus
  // Note: Wire (ExI2C / encoder HAT) is begun by applyNavMode() — see below.

  // Grove port SPI — pins stored but NOT begun here.
  // GPIO 32/33 are shared with GPS UART2 (TX=32, RX=33).
  // CC1101Util::begin() calls extSpi.begin() when the bus is actually needed,
  // and GPS Serial2.begin() can freely claim the pins when CC1101 is idle.
  extSpi.setPins(V_SPI_SCK, V_SPI_MISO, V_SPI_MOSI, -1);

  auto* dev = new Device(display, power, &navigation, nullptr, &extSpi, &speaker);
  dev->ExI2C = &Wire;   // Grove I2C — Wire1 is reserved for AXP192+RTC
  dev->InI2C = &Wire1;  // AXP192 + BM8563 RTC
  return dev;
}

void Device::applyNavMode() {
  String mode = Config.get(APP_CONFIG_NAV_MODE, APP_CONFIG_NAV_MODE_DEFAULT);
  // Wire is shared between ExI2C (Grove, 32/33) and the EncoderC HAT (0/26).
  // arduino-esp32 v2.0.17's TwoWire::begin() does not cleanly switch pins on a
  // second call — end() first, then let the chosen nav own the bus.
  Wire.end();
  if (mode == "encoder") {
    switchNavigation(&encoderNavigation);  // encoder.begin() → Wire.begin(0, 26, 200kHz)
  } else {
    Wire.begin(GROVE_SDA, GROVE_SCL);
    switchNavigation(&navigation);
  }
}

void Device::boardHook() {
  static unsigned long _btnAHeld = 0;
  if (digitalRead(BTN_A) == LOW) {
    if (_btnAHeld == 0) _btnAHeld = millis();
    else if (millis() - _btnAHeld >= 3000) {
      Config.set(APP_CONFIG_NAV_MODE, APP_CONFIG_NAV_MODE_DEFAULT);
      Config.save(Storage);
      applyNavMode();
      _btnAHeld = 0;
    }
  } else {
    _btnAHeld = 0;
  }
}