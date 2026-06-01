#pragma once

#include "ui/templates/ListScreen.h"

class KeyboardMenuScreen : public ListScreen {
public:
  const char* title() override { return "HID"; }

  void onInit()             override;
  void onItemSelected(uint8_t index) override;
  void onBack()             override;

private:
#ifdef DEVICE_HAS_WEBAUTHN
  ListItem _items[4] = {
    {"USB MouseKeyboard"},
    {"BLE MouseKeyboard"},
    {"USB Web Authn"},
    {"USB Mass Storage"},
  };
#elif defined(DEVICE_HAS_USB_HID)
  ListItem _items[3] = {
    {"USB MouseKeyboard"},
    {"BLE MouseKeyboard"},
    {"USB Mass Storage"},
  };
#else
  ListItem _items[1] = {
    {"BLE MouseKeyboard"},
  };
#endif
};