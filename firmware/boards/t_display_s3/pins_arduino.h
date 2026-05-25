#pragma once
#include <stdint.h>

#define BTN_UP 0
#define BTN_B  14

#define LCD_BAT_VOLT  4
#define LCD_POWER_ON  15

#define LCD_BL    38
#define LCD_BL_CH 7

#define DEVICE_HAS_USB_HID
#define DEVICE_HAS_WEBAUTHN  // FIDO2 / WebAuthn USB security key (CTAP2 + U2F)

// Define standard SPI pins to avoid SD.h compilation errors, even if unused
#define SS 10
#define MOSI 11
#define MISO 13
#define SCK 12

// ─── I2C (Grove + on-board touch when DEVICE_HAS_TOUCH) ──
static const uint8_t SDA = 18;
static const uint8_t SCL = 17;

// ─── Touch (LilyGo T-Display S3 Touch variant only) ──────
// CST820 / CST816-compatible capacitive controller at 0x15. INT goes LOW
// while a finger is down; RST is held LOW briefly at boot to reset the IC.
#ifdef DEVICE_HAS_TOUCH
#define TOUCH_INT       16
#define TOUCH_RST       21
#define TOUCH_I2C_ADDR  0x15
// Touch panel reports finger coordinates in the display's native portrait
// frame (170 wide × 320 tall). The screen runs in landscape (rotation 3),
// so the nav code swaps and mirrors axes to match.
#define TOUCH_NATIVE_W  170
#define TOUCH_NATIVE_H  320
#endif

#define USER_SETUP_LOADED 1
#define DISABLE_ALL_LIBRARY_WARNINGS 1
#define ST7789_DRIVER
#define USE_HSPI_PORT
#define INIT_SEQUENCE_3

#define CGRAM_OFFSET
#define TFT_RGB_ORDER TFT_RGB

#define TFT_INVERSION_ON

#define TFT_PARALLEL_8_BIT
#define TFT_WIDTH 170
#define TFT_HEIGHT 320

#define TFT_CS 6
#define TFT_DC 7
#define TFT_RST 5
#define TFT_WR 8
#define TFT_RD 9

#define TFT_D0 39
#define TFT_D1 40
#define TFT_D2 41
#define TFT_D3 42
#define TFT_D4 45
#define TFT_D5 46
#define TFT_D6 47
#define TFT_D7 48

#define TFT_BL -1
#define TFT_BACKLIGHT_ON HIGH

#define TFT_DEFAULT_ORIENTATION 3
#define SMOOTH_FONT
