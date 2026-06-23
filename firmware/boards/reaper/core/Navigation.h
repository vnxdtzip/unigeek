//
// RF Reaper — 6 buttons, all active LOW: SEL (0), UP (41), DOWN (40),
// RIGHT (38), LEFT (39), BACK/ESC (21). Dedicated back button, so no
// LEFT-hold gymnastics — each direction maps straight through.
//

#pragma once

#include "core/INavigation.h"
#include "pins_arduino.h"

class NavigationImpl : public INavigation
{
public:
  void begin() override {
    pinMode(BTN_SEL,   INPUT_PULLUP);
    pinMode(BTN_UP,    INPUT_PULLUP);
    pinMode(BTN_DOWN,  INPUT_PULLUP);
    pinMode(BTN_RIGHT, INPUT_PULLUP);
    pinMode(BTN_LEFT,  INPUT_PULLUP);
    pinMode(BTN_BACK,  INPUT_PULLUP);
  }

  void update() override {
    if (digitalRead(BTN_BACK)  == LOW) { updateState(DIR_BACK);  return; }
    if (digitalRead(BTN_UP)    == LOW) { updateState(DIR_UP);    return; }
    if (digitalRead(BTN_DOWN)  == LOW) { updateState(DIR_DOWN);  return; }
    if (digitalRead(BTN_LEFT)  == LOW) { updateState(DIR_LEFT);  return; }
    if (digitalRead(BTN_RIGHT) == LOW) { updateState(DIR_RIGHT); return; }
    if (digitalRead(BTN_SEL)   == LOW) { updateState(DIR_PRESS); return; }
    updateState(DIR_NONE);
  }
};
