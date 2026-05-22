#include "core/Device.h"
#include "core/Display.h"
#include "core/Navigation.h"
#include "core/Power.h"
#include <Wire.h>

static DisplayImpl displayImpl;
static PowerImpl powerImpl;
static NavigationImpl navImpl;

Device* Device::createInstance() {
    // Battery ADC and Backlight initialization
    pinMode(LCD_BAT_VOLT, INPUT);

    ledcSetup(LCD_BL_CH, 1000, 8);
    ledcAttachPin(LCD_BL, LCD_BL_CH);
    ledcWrite(LCD_BL_CH, 255);

    // I2C bus on the touch variant carries both the on-board CST820 touch IC
    // and any Grove peripheral on pins 18/17 — same bus, treated as ExI2C
    // (free, retargetable) for module compatibility.
    Wire.begin(SDA, SCL);

    auto* dev = new Device(displayImpl, powerImpl, &navImpl);
    dev->ExI2C = &Wire;
    return dev;
}

void Device::boardHook() {
    // Empty stub for boards with no specific loop logic
}


