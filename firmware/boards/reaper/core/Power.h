//
// RF Reaper power — BQ25896 charger + BQ27220 fuel gauge on Grove I2C (SDA=47, SCL=48).
//

#pragma once

#include "core/IPower.h"
#include "pins_arduino.h"
#include <Wire.h>
#include <esp_sleep.h>

// ─── BQ27220 Fuel Gauge ───────────────────────────────────
#define BQ27220_ADDR        0x55
#define BQ27220_REG_SOC     0x1C

// ─── BQ25896 Charger ─────────────────────────────────────
#define BQ25896_ADDR        0x6B
#define BQ25896_REG_STATUS  0x0B
#define BQ25896_REG_BATFET  0x09   // bit5: BATFET_DIS (true power-off)

class PowerImpl : public IPower
{
public:
  void begin() override
  {
    Wire.begin(GROVE_SDA, GROVE_SCL);
    Wire.setClock(400000);

    // Reset to defaults + enable ADC continuous measure
    _writeReg(BQ25896_ADDR, 0x14, 0x80);
    delay(10);
    _writeReg(BQ25896_ADDR, 0x02, 0x1D);

    // Charge target voltage 4208mV
    _writeReg(BQ25896_ADDR, 0x06, 0x5E);

    // Charge current 512mA (safe default)
    _writeReg(BQ25896_ADDR, 0x04, 0x08);
  }

  uint8_t getBatteryPercentage() override
  {
    uint16_t soc = _readReg16(BQ27220_ADDR, BQ27220_REG_SOC);
    if (soc > 100) soc = 100;
    return (uint8_t)soc;
  }

  bool isCharging() override
  {
    uint8_t status = _readReg8(BQ25896_ADDR, BQ25896_REG_STATUS);
    uint8_t chrg = (status >> 3) & 0x03;
    return (chrg == 0x01 || chrg == 0x02);
  }

  void powerOff() override
  {
    // Screen dark whether we cut power or fall through to sleep.
    digitalWrite(LCD_BL, LOW);

    // Open the BQ25896 BATFET — a true power-off on battery (the rail drops).
    // On USB the charger keeps the rail alive, so we fall through into deep
    // sleep as a low-power standby instead.
    uint8_t r09 = _readReg8(BQ25896_ADDR, BQ25896_REG_BATFET);
    _writeReg(BQ25896_ADDR, BQ25896_REG_BATFET, r09 | 0x20);

    // Wake on the back button (BTN_BACK = GPIO21, active LOW).
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BTN_BACK, 0);
    esp_deep_sleep_start();
  }

private:
  uint16_t _readReg16(uint8_t addr, uint8_t reg)
  {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)addr, (uint8_t)2);
    if (Wire.available() < 2) return 0;
    uint16_t lo = Wire.read();
    uint16_t hi = Wire.read();
    return (hi << 8) | lo;
  }

  uint8_t _readReg8(uint8_t addr, uint8_t reg)
  {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)addr, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0;
  }

  void _writeReg(uint8_t addr, uint8_t reg, uint8_t val)
  {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
  }
};
