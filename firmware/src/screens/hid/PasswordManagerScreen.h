#pragma once

#include "ui/templates/ListScreen.h"
#include "utils/keyboard/HIDKeyboardUtil.h"

class PasswordManagerScreen : public ListScreen {
public:
  // keyboard is borrowed — owned and begin()/end()-managed by the parent KeyboardScreen.
  PasswordManagerScreen(HIDKeyboardUtil* keyboard, int mode);

  const char* title()                  override;

  void onInit()                        override;
  void onUpdate()                      override;
  void onRender()                      override;
  void onItemSelected(uint8_t index)   override;
  void onBack()                        override;

private:
  enum State : uint8_t {
    STATE_LOCKED,
    STATE_MENU,
    STATE_ADD,
    STATE_VIEW,
  };

  static constexpr uint8_t     kMaxEntries = 16;
  static constexpr const char* kDir    = "/unigeek/hid/passwords";
  static constexpr const char* kMaster = "/unigeek/hid/passwords/.master";
  static constexpr const char* kVault  = "/unigeek/hid/passwords/.vault";

  // Password derivation source. SRC_LEGACY = SHA-256(masterPw || fields).
  // SRC_WEBAUTHN = HMAC-SHA-256(master.bin, masterPw || fields) — binds the
  // password to both the typed master AND this device's webauthn master key.
  static constexpr uint8_t SRC_LEGACY   = 0;
  static constexpr uint8_t SRC_WEBAUTHN = 1;

  struct Entry {
    char    label[33];
    uint8_t type;      // 0=alphanum  1=alpha  2=alphanum+sym
    uint8_t caseMode;  // 0=lower  1=upper  2=mixed
    uint8_t length;    // 5–60
    uint8_t source;    // SRC_LEGACY / SRC_WEBAUTHN
  };

  HIDKeyboardUtil* _keyboard;   // non-owning
  int              _mode;
  State            _state     = STATE_LOCKED;
  bool             _firstRun  = false;
  bool             _holdFired = false;

  char    _masterPw[65] = {};

  Entry    _entries[kMaxEntries];
  uint8_t  _entryCount = 0;

  ListItem _menuItems[kMaxEntries + 2];

  // Add form
  String   _pendingLabel;
  uint8_t  _pendingType     = 0;
  uint8_t  _pendingCase     = 0;
  uint8_t  _pendingLen      = 16;  // within 8–34
  uint8_t  _pendingSource   = SRC_LEGACY;
  char     _addTypeBuf[20]  = {};
  char     _addCaseBuf[12]  = {};
  char     _addLenBuf[4]    = {};
  char     _addLblBuf[33]   = {};
  char     _addSrcBuf[12]   = {};
  ListItem _addItems[6];

  // View
  uint8_t  _viewIdx         = 0;
  char     _viewPw[35]      = {};
  bool     _viewFirstRender = false;

  void _tryUnlock();
  void _reloadMenu();
  void _loadVault();
  void _saveVault();
  void _enterAdd();
  void _updateAddLabels();
  void _saveEntry();
  void _enterView(uint8_t index);
  void _showEntryOptions(uint8_t index);
  void _deleteEntry(uint8_t index);
  void _typePassword();
  void _renderView();

  // Returns false when source=SRC_WEBAUTHN but master.bin is missing
  // (no webauthn build, or user hasn't run BIP39 Generate yet).
  bool   _generatePassword(const Entry& e, char* out, uint8_t maxLen);
  bool   _verifyMaster(const char* pw);
  void   _setMaster(const char* pw);
  void   _sha256(const uint8_t* data, size_t len, uint8_t out[32]);
  void   _sha256str(const String& s, uint8_t out[32]);
  void   _hmacSha256(const uint8_t* key, size_t keyLen,
                     const uint8_t* data, size_t dataLen,
                     uint8_t out[32]);
  bool   _waMasterAvailable();
  void _xorCrypt(const uint8_t* in, size_t len, uint8_t* out);
};