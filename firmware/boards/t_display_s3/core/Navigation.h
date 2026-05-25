#pragma once
#include "core/INavigation.h"

#ifdef DEVICE_HAS_TOUCH
#include <Wire.h>
#include "core/Device.h"
#endif

// T-Display S3 navigation.
//
// Base variant (`t_display_s3`): two physical buttons (BTN_UP, BTN_B). BTN_B
// uses a double-tap state machine — single click = DOWN, double click = PRESS,
// long press = BACK. BTN_UP held = continuous UP.
//
// Touch variant (`t_display_s3_touch`, DEVICE_HAS_TOUCH): identical button
// behaviour, plus an additional touch input source. Touch is polled ONLY when
// no button is active so the two paths never fight. Zone layout matches CYD:
//
//     Left  1/4 (display_x < 80)        BACK
//     Right 3/4, top    third (≈57px)   UP
//     Right 3/4, middle third           PRESS
//     Right 3/4, bottom third           DOWN
//
// Touch IC is CST820 / CST816-compatible at 0x15 — same 6-byte read at reg
// 0x01 we already use for CYD's CST816S backend. INT pin (16) goes LOW while
// a finger is down; RST (21) is pulsed at boot.

class NavigationImpl : public INavigation {
public:
    void begin() override {
        pinMode(BTN_UP, INPUT_PULLUP);
        pinMode(BTN_B,  INPUT_PULLUP);
#ifdef DEVICE_HAS_TOUCH
        // Wire bus is initialised in Device::createInstance() before begin().
        pinMode(TOUCH_INT, INPUT);
        pinMode(TOUCH_RST, OUTPUT);
        digitalWrite(TOUCH_RST, LOW);
        delay(10);
        digitalWrite(TOUCH_RST, HIGH);
        delay(50);
#endif
    }

    void update() override {
        bool btnUp   = (digitalRead(BTN_UP) == LOW);
        bool btnDown = (digitalRead(BTN_B)  == LOW);
        uint32_t now = millis();

        // ── Button state machine (unchanged from base t_display_s3) ────────
        bool buttonsBusy = btnUp || btnDown || _btnBWasDown ||
                           _waitDblClick || _btnBLong || _syntheticDown;

        if (buttonsBusy) {
            if (btnUp) {
                _waitDblClick = false;
                updateState(DIR_UP);
                return;
            }

            if (_syntheticDown) {
                _syntheticDown = false;
                updateState(DIR_NONE);
                return;
            }

            if (btnDown && !_btnBWasDown) {
                _btnBWasDown = true;

                if (_waitDblClick && (now - _lastDownTime) <= DBL_CLICK_MS) {
                    _waitDblClick = false;
                    _heldDir = DIR_PRESS;
                    updateState(DIR_PRESS);
                } else {
                    _waitDblClick = true;
                    _lastDownTime = now;
                    _heldDir      = DIR_NONE;
                    updateState(DIR_NONE);
                }
                return;
            }

            if (btnDown) {
                uint32_t held = now - _lastDownTime;

                if (!_btnBLong && held > LONG_PRESS_MS) {
                    _btnBLong     = true;
                    _waitDblClick = false;
                    _heldDir      = DIR_NONE;
                    updateState(DIR_BACK);
                    return;
                }
                if (_btnBLong) { updateState(DIR_NONE); return; }

                if (_waitDblClick) {
                    if (held > DBL_CLICK_MS) {
                        _waitDblClick = false;
                        _heldDir = DIR_DOWN;
                        updateState(DIR_DOWN);
                    } else {
                        updateState(DIR_NONE);
                    }
                } else {
                    updateState(_heldDir);
                }
                return;
            }

            // btnDown just released
            _btnBWasDown = false;
            _heldDir     = DIR_NONE;

            if (_btnBLong) {
                _btnBLong = false;
                updateState(DIR_NONE);
                return;
            }

            if (_waitDblClick) {
                if ((now - _lastDownTime) > DBL_CLICK_MS) {
                    _waitDblClick  = false;
                    _syntheticDown = true;
                    updateState(DIR_DOWN);
                    return;
                } else {
                    updateState(DIR_NONE);
                    return;
                }
            }
            updateState(DIR_NONE);
            return;
        }

#ifdef DEVICE_HAS_TOUCH
        // ── Touch fallback (buttons idle) ──────────────────────────────────
        // Rate-limit polling — 50 Hz is more than enough for zone nav.
        if (now - _lastTouchPoll < 20) {
            updateState(_touchDir);
            return;
        }
        _lastTouchPoll = now;

        uint16_t tx, ty;
        bool touched = _readTouch(&tx, &ty);

        if (!touched) {
            if (++_noTouchCnt < kNoTouchThreshold) {
                updateState(_touchDir);
                return;
            }
            _touchDir = DIR_NONE;
            updateState(DIR_NONE);
            return;
        }
        _noTouchCnt = 0;

        Direction dir;
        if (tx < (uint16_t)kBackEnd) {
            dir = DIR_BACK;
        } else {
            if      (ty < (uint16_t)kZoneH)         dir = DIR_UP;
            else if (ty < (uint16_t)(kZoneH * 2))   dir = DIR_PRESS;
            else                                     dir = DIR_DOWN;
        }

        _lastTouchX = (int16_t)tx;
        _lastTouchY = (int16_t)ty;
        _touchDir   = dir;
        updateState(dir);
#else
        updateState(DIR_NONE);
#endif
    }

#ifdef DEVICE_HAS_TOUCH
protected:
    void _doDrawOverlay() override {
        if (_touchDir == _lastOverlayDir) return;
        if (_lastOverlayDir != DIR_NONE) _paintZone(_lastOverlayDir, false);
        if (_touchDir       != DIR_NONE) _paintZone(_touchDir,       true);
        _lastOverlayDir = _touchDir;
    }
#endif

private:
    static constexpr uint32_t DBL_CLICK_MS  = 250;
    static constexpr uint32_t LONG_PRESS_MS = 600;

    uint32_t  _lastDownTime  = 0;
    bool      _btnBWasDown   = false;
    bool      _waitDblClick  = false;
    bool      _syntheticDown = false;
    bool      _btnBLong      = false;
    Direction _heldDir       = DIR_NONE;

#ifdef DEVICE_HAS_TOUCH
    // ── Touch state ────────────────────────────────────────────────────────
    // Display geometry in landscape (ROTATION 3): 320 wide × 170 tall.
    static constexpr int16_t kScreenW = TFT_HEIGHT;   // 320
    static constexpr int16_t kScreenH = TFT_WIDTH;    // 170
    static constexpr int16_t kBackEnd = kScreenW / 4; // 80
    static constexpr int16_t kZoneH   = kScreenH / 3; // ~57
    static constexpr uint8_t kNoTouchThreshold = 3;

    uint32_t  _lastTouchPoll  = 0;
    uint8_t   _noTouchCnt     = 0;
    Direction _touchDir       = DIR_NONE;
    Direction _lastOverlayDir = DIR_NONE;

    // Read CST820 / CST816S — 6 bytes from reg 0x01 (gesture, npts, xH, xL,
    // yH, yL). Coordinates are in the panel's portrait-native frame
    // (TOUCH_NATIVE_W × TOUCH_NATIVE_H = 170 × 320); rotation 3 maps them to
    // landscape display space: display_x = raw_y, display_y = TOUCH_NATIVE_W
    // − raw_x − 1. The board ships with the touch IC mounted to match this
    // mapping — flip the raw_x / raw_y swap below if your unit is mirrored.
    bool _readTouch(uint16_t* tx, uint16_t* ty) {
        if (digitalRead(TOUCH_INT) != LOW) return false;
        Wire.beginTransmission((uint8_t)TOUCH_I2C_ADDR);
        Wire.write(0x01);
        if (Wire.endTransmission(false) != 0) return false;
        Wire.requestFrom((uint8_t)TOUCH_I2C_ADDR, (uint8_t)6);
        if (Wire.available() < 6) return false;
        Wire.read();                        // gesture ID
        uint8_t npts = Wire.read() & 0x0F;  // finger count
        uint8_t xH   = Wire.read() & 0x0F;
        uint8_t xL   = Wire.read();
        uint8_t yH   = Wire.read() & 0x0F;
        uint8_t yL   = Wire.read();
        if (!npts) return false;

        uint16_t rawX = ((uint16_t)xH << 8) | xL;
        uint16_t rawY = ((uint16_t)yH << 8) | yL;
        if (rawX >= TOUCH_NATIVE_W) rawX = TOUCH_NATIVE_W - 1;
        if (rawY >= TOUCH_NATIVE_H) rawY = TOUCH_NATIVE_H - 1;

        *tx = rawY;                                // 0..kScreenW-1
        *ty = (uint16_t)(TOUCH_NATIVE_W - 1 - rawX);  // 0..kScreenH-1
        return true;
    }

    void _paintZone(Direction d, bool lit) {
        constexpr uint16_t LIT_RED   = 0xF800;
        constexpr uint16_t LIT_GREEN = 0x07E0;
        constexpr uint16_t LIT_BLUE  = 0x001F;

        auto& lcd = Uni.Lcd;
        Sprite bar(&lcd);

        switch (d) {
            case DIR_BACK:
                bar.createSprite(2, kScreenH);
                bar.fillSprite(lit ? LIT_RED : TFT_BLACK);
                bar.pushSprite(0, 0);
                break;
            case DIR_UP:
                bar.createSprite(2, kZoneH - 1);
                bar.fillSprite(lit ? LIT_GREEN : TFT_BLACK);
                bar.pushSprite(kScreenW - 2, 0);
                break;
            case DIR_PRESS:
                bar.createSprite(2, kZoneH - 1);
                bar.fillSprite(lit ? LIT_BLUE : TFT_BLACK);
                bar.pushSprite(kScreenW - 2, kZoneH);
                break;
            case DIR_DOWN:
                bar.createSprite(2, kScreenH - kZoneH * 2);
                bar.fillSprite(lit ? LIT_GREEN : TFT_BLACK);
                bar.pushSprite(kScreenW - 2, kZoneH * 2);
                break;
            default:
                return;
        }
        bar.deleteSprite();
    }
#endif
};
