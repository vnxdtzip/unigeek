//
// RF Reaper (Smoochiee) — Device factory
//

#include "core/Device.h"
#include "Navigation.h"
#include "Display.h"
#include "Power.h"
#include "LedRing.h"
#include <Wire.h>
#include <esp_heap_caps.h>

static DisplayImpl    display;
static NavigationImpl navigation;
static PowerImpl      power;
static ExtSpiClass    sharedSpi(HSPI);  // LCD + SD + CC1101 + NRF24 share SCK/MOSI/MISO

void Device::boardHook() {
  ledRing.update();
}

Device* Device::createInstance() {
  // Route all malloc to PSRAM first (falls back to internal RAM as needed)
  if (psramFound()) heap_caps_malloc_extmem_enable(0);

  // Backlight on
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  // Assert every CS on the shared bus high before SPI init
  const uint8_t spi_cs_pins[] = { LCD_CS, SD_CS, CC1101_CS_PIN, NRF24_CSN_PIN };
  for (auto pin : spi_cs_pins) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
  }

  Wire.begin(GROVE_SDA, GROVE_SCL);  // Grove I2C (ExI2C) — BQ25896 + BQ27220 live here too
  sharedSpi.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, -1);

  ledRing.begin();

  auto* dev = new Device(display, power, &navigation, nullptr, &sharedSpi, nullptr);
  dev->ExI2C = &Wire;  // Grove I2C — also used by BQ25896/BQ27220 in Power.h
  return dev;
}
