//
// RF Reaper (Smoochiee) — ESP32-S3, 16MB flash, 8MB OPI PSRAM
//
// Ported from Bruce's "reaper board" (boards/reaper/pins_arduino.h). Sub-GHz
// focused: CC1101 + NRF24 on the shared display SPI bus, ST7789 170x320 IPS,
// 6 navigation buttons (dedicated back), BQ25896 charger + BQ27220 fuel gauge,
// and a 16-pixel WS2812B RGB ring.
//
// Not ported (no driver path in this firmware): LoRa (SX12xx), AW9523 IO
// expander, BQ25896 5V boost. Pins are listed at the bottom for reference.
//

#pragma once
#include <stdint.h>

// ─── Shared SPI Bus (LCD + SD + CC1101 + NRF24) ───────────
#define SPI_MOSI_PIN  18
#define SPI_MISO_PIN  8
#define SPI_SCK_PIN   17

static const uint8_t MOSI = SPI_MOSI_PIN;
static const uint8_t MISO = SPI_MISO_PIN;
static const uint8_t SCK  = SPI_SCK_PIN;

// ─── SD Card ──────────────────────────────────────────────
#define SD_CS  3

static const uint8_t SS = SD_CS;

// ─── I2C (Grove — charger, fuel gauge, external modules) ──
#define GROVE_SDA  47
#define GROVE_SCL  48

static const uint8_t SDA = GROVE_SDA;
static const uint8_t SCL = GROVE_SCL;

// ─── LCD ──────────────────────────────────────────────────
#define LCD_CS   7
#define LCD_DC   15
#define LCD_RST  16
#define LCD_BL     6
#define LCD_BL_CH  7

// ─── Navigation buttons (active LOW) ──────────────────────
#define BTN_SEL    0    // OK / Select (also BOOT)
#define BTN_UP    41
#define BTN_DOWN  40
#define BTN_RIGHT 38
#define BTN_LEFT  39
#define BTN_BACK  21    // dedicated ESC / back

// ─── IR (M5 IR Mod / onboard LEDs) ────────────────────────
#define IR_TX      2
#define IR_RX_PIN  1

// ─── CC1101 Sub-GHz ───────────────────────────────────────
#define CC1101_CS_PIN    9
#define CC1101_GDO0_PIN  46

// ─── NRF24L01+ ────────────────────────────────────────────
#define NRF24_CE_PIN     14
#define NRF24_CSN_PIN    13

// ─── RGB LED ring (WS2812B) ───────────────────────────────
#define RGB_LED    45   // addressable data pin
#define LED_COUNT  16   // pixels in the ring

// ─── TFT_eSPI config ──────────────────────────────────────
#define DISABLE_ALL_LIBRARY_WARNINGS 1
#define USER_SETUP_LOADED 1

#define ST7789_DRIVER
#define TFT_RGB_ORDER  TFT_RGB
#define TFT_WIDTH   170
#define TFT_HEIGHT  320
#define TFT_MOSI    SPI_MOSI_PIN
#define TFT_SCLK    SPI_SCK_PIN
#define TFT_MISO    SPI_MISO_PIN
#define TFT_CS      LCD_CS
#define TFT_DC      LCD_DC
#define TFT_RST     LCD_RST
#define TFT_BL      LCD_BL
#define TFT_BACKLIGHT_ON  1
#define TFT_DEFAULT_ORIENTATION  1
#define USE_HSPI_PORT
#define TOUCH_CS    -1
#define SMOOTH_FONT 1
#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  20000000

// ─── Firmware Feature Flags ───────────────────────────────
#define DEVICE_HAS_4WAY_NAV       // dedicated up/down/left/right buttons
#define DEVICE_HAS_LED_RING       // WS2812B ring → LED Effect setting
#define DEVICE_HAS_USB_HID        // ESP32-S3 native USB OTG
#define DEVICE_HAS_WEBAUTHN       // FIDO2 / WebAuthn USB security key (CTAP2 + U2F)
#define APP_MENU_POWER_OFF        // BQ25896 can power off the device

// ─── Reference: peripherals present on the board, no driver here ──────────
// LoRa (SX12xx):  CS=4  RST=43  BUSY=5  IRQ=42  (shared SPI bus)
// IO expander:    AW9523 on Grove I2C  (GPS_EN, VIBRO, CC RX/TX, logo LED)
// PMIC boost:     BQ25896 5V OTG output
