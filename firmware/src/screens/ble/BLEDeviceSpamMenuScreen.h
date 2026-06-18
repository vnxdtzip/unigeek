#pragma once

#include "ui/templates/ListScreen.h"

class BLEDeviceSpamMenuScreen : public ListScreen {
public:
  const char* title() override { return "Device Spam"; }

  void onInit() override;
  void onItemSelected(uint8_t index) override;
  void onBack() override;

private:
  ListItem _items[5] = {
    {"Android Spam"},
    {"iOS Spam"},
    {"Samsung Spam"},
    {"Windows Spam"},
    {"Spam All"},
  };
};