//
// Created by L Shaf on 2026-02-19.
//

#pragma once

#include "core/ScreenMirror.h"

#ifdef DISPLAY_BACKEND_M5GFX
  #include <M5GFX.h>
  class IDisplay : public lgfx::LGFX_Device
  {
  public:
    virtual void setBrightness(uint8_t brightness) = 0;
    virtual void powerOff() { setBrightness(0); }
  };
  using Sprite = LGFX_Sprite;
#else
  #include <TFT_eSPI.h>
  class IDisplay : public TFT_eSPI
  {
  public:
    virtual void setBrightness(uint8_t brightness) = 0;
    virtual void powerOff() { setBrightness(0); }

    // ── Screen-mirror capture taps for *direct* panel draws ──────────────────
    // Sprites mirror via CaptureSprite; these shadow the fast-path draws so
    // direct Uni.Lcd.* calls also reach the host. Each draws to the panel as
    // usual, then records the same geometry into Mirror (a no-op when not
    // streaming). Reached because screens call through the IDisplay static
    // type; base-internal sub-draws (drawString glyphs, drawLine, drawCircle)
    // funnel through the virtual drawPixel below, so they're captured too.
    // `using` keeps the base's other overloads visible (avoids name-hiding).
    using TFT_eSPI::pushImage;

    void drawPixel(int32_t x, int32_t y, uint32_t color) override {
      TFT_eSPI::drawPixel(x, y, color);
      Mirror.fill((int16_t)x, (int16_t)y, 1, 1, (uint16_t)color);
    }
    void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
      TFT_eSPI::fillRect(x, y, w, h, color);
      Mirror.fill((int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, (uint16_t)color);
    }
    void fillScreen(uint32_t color) {
      TFT_eSPI::fillScreen(color);
      Mirror.fill(0, 0, (int16_t)width(), (int16_t)height(), (uint16_t)color);
    }
    void drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
      TFT_eSPI::drawRect(x, y, w, h, color);
      Mirror.fill((int16_t)x, (int16_t)y,           (int16_t)w, 1,          (uint16_t)color);
      Mirror.fill((int16_t)x, (int16_t)(y + h - 1), (int16_t)w, 1,          (uint16_t)color);
      Mirror.fill((int16_t)x,           (int16_t)y, 1,          (int16_t)h, (uint16_t)color);
      Mirror.fill((int16_t)(x + w - 1), (int16_t)y, 1,          (int16_t)h, (uint16_t)color);
    }
    void drawFastHLine(int32_t x, int32_t y, int32_t w, uint32_t color) {
      TFT_eSPI::drawFastHLine(x, y, w, color);
      Mirror.fill((int16_t)x, (int16_t)y, (int16_t)w, 1, (uint16_t)color);
    }
    void drawFastVLine(int32_t x, int32_t y, int32_t h, uint32_t color) {
      TFT_eSPI::drawFastVLine(x, y, h, color);
      Mirror.fill((int16_t)x, (int16_t)y, 1, (int16_t)h, (uint16_t)color);
    }
    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t* data) {
      TFT_eSPI::pushImage(x, y, w, h, data);
      Mirror.image((int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, data);
    }
    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* data) {
      TFT_eSPI::pushImage(x, y, w, h, data);
      Mirror.image((int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, data);
    }
  };

  // Drop-in replacement for TFT_eSprite that also mirrors every pushSprite()
  // into ScreenMirror for screen streaming. The codebase uses the concrete
  // `Sprite` alias everywhere, so this captures all blits with no screen edits.
  // When the mirror is inactive each override is one bool check. Method bodies
  // live in core/ScreenMirror.cpp to avoid an include cycle. `using` keeps the
  // base's other pushSprite overloads visible (else they'd be name-hidden).
  class CaptureSprite : public TFT_eSprite
  {
  public:
    CaptureSprite(TFT_eSPI* tft) : TFT_eSprite(tft) {}
    using TFT_eSprite::pushSprite;
    void pushSprite(int32_t x, int32_t y);
    void pushSprite(int32_t x, int32_t y, uint16_t transparent);
  };
  using Sprite = CaptureSprite;
#endif