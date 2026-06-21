#include "BLEDeviceSpamMenuScreen.h"
#include "core/Device.h"
#include "core/ScreenManager.h"
#include "screens/ble/BLEMenuScreen.h"
#include "screens/ble/BLEAndroidSpamScreen.h"
#include "screens/ble/BLEiOSSpamScreen.h"
#include "screens/ble/BLESamsungSpamScreen.h"
#include "screens/ble/BLEWindowsSpamScreen.h"
#include "screens/ble/BLEAllSpamScreen.h"

void BLEDeviceSpamMenuScreen::onInit()
{
  setItems(_items);
}

void BLEDeviceSpamMenuScreen::onItemSelected(uint8_t index)
{
  switch (index) {
    case 0: Screen.push(new BLEAndroidSpamScreen()); break;
    case 1: Screen.push(new BLEiOSSpamScreen());     break;
    case 2: Screen.push(new BLESamsungSpamScreen()); break;
    case 3: Screen.push(new BLEWindowsSpamScreen()); break;
    case 4: Screen.push(new BLEAllSpamScreen());     break;
  }
}

void BLEDeviceSpamMenuScreen::onBack()
{
  Screen.goBack();
}