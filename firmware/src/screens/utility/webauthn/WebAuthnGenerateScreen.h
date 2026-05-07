#pragma once

#include <Arduino.h>  // pulls pins_arduino.h so DEVICE_HAS_WEBAUTHN is defined

#ifdef DEVICE_HAS_WEBAUTHN

#include "ui/templates/ListScreen.h"
#include "utils/network/WifiUtility.h"

// Generate (or regenerate) the FIDO master key with WiFi+NTP-fueled entropy.
//
// Flow:
//   1. Warn (sterner copy when regenerating, since regen wipes existing creds)
//   2. WiFi list: scan → pick (ListScreen-rendered, same as Wifi > Network) →
//      WifiUtility::connectWithPrompt
//   3. NTP: configTime + wait for sntp_get_sync_status() == COMPLETED
//   4. RandomSeed::reseed() + WebAuthnCrypto::reseed() — refresh entropy
//      pools now that RF is active and RTC set
//   5. CredentialStore::generateMaster(force=isRegen)
//   6. BIP-39 encode the new master to 24 word indices
//   7. Page through the 24 words (adaptive layout, sprite-blit per page)
//   8. Done — user has written them down before exiting
//
// Master.bin no longer auto-generates on first WebAuthn op — the user must
// drive this screen at least once. See CredentialStore::hasMaster() / init().
class WebAuthnGenerateScreen : public ListScreen {
public:
  const char* title()    override;
  bool inhibitPowerOff() override { return true; }

  void onInit()                       override;
  void onUpdate()                     override;
  void onRender()                     override;
  void onItemSelected(uint8_t index)  override;
  void onBack()                       override;

private:
  enum State : uint8_t {
    ST_WARNING,        // intro + UP confirm (custom-rendered)
    ST_WIFI_LIST,      // ListScreen-rendered scan results
    ST_NTP_SYNC,       // post-WiFi: waiting for NTP completion
    ST_GENERATING,     // reseed + generate + encode (single tick)
    ST_WORDS,          // paged display
    ST_DONE,           // "have you written them down?"
    ST_ERROR,          // _err set; PRESS exits
  };

  static constexpr uint8_t kWordCount = 24;
  static constexpr uint8_t kMaxScans  = WifiUtility::MAX_WIFI;

  struct Layout {
    uint8_t  cols;
    uint8_t  rows;
    uint8_t  wordsPerPage;
    uint8_t  pageCount;
    int16_t  rowH;
    int16_t  headerH;
    int16_t  footerH;
  };
  Layout _layout();

  State        _state        = ST_WARNING;
  bool         _isRegen      = false;
  uint8_t      _page         = 0;
  uint16_t     _wordIdx[kWordCount];
  bool         _wordsReady   = false;
  uint32_t     _ntpStartMs   = 0;
  bool         _generateRan  = false;
  const char*  _err          = nullptr;

  // WiFi scan buffers — same shape as NetworkMenuScreen
  WifiUtility::ScannedWifi _scanned[kMaxScans];
  ListItem                 _scannedItems[kMaxScans];
  uint8_t                  _scannedCount = 0;

  // State entry helpers
  void _enterWifiList();
  bool _connectAndAdvance(uint8_t index);  // calls connectWithPrompt + transitions
  bool _runGenerate();                      // reseed + generateMaster + encode

  // Render helpers (called from onRender per state)
  void _drawWarning();
  void _drawSync(const char* line1, const char* line2);
  void _drawWords();
  void _drawDone();
  void _drawError();
};

#endif  // DEVICE_HAS_WEBAUTHN
