#include "WebAuthnManageScreen.h"

#ifdef DEVICE_HAS_WEBAUTHN

#include "core/ScreenManager.h"
#include "screens/utility/webauthn/WebAuthnBackupScreen.h"
#include "screens/utility/webauthn/WebAuthnGenerateScreen.h"
#include "screens/utility/webauthn/WebAuthnPasskeyListScreen.h"
#include "utils/webauthn/CredentialStore.h"

void WebAuthnManageScreen::onInit()
{
  webauthn::CredentialStore::init();
  _items[0].label = webauthn::CredentialStore::hasMaster()
                    ? "Regenerate BIP39"
                    : "Generate BIP39";
  setItems(_items);
}

void WebAuthnManageScreen::onItemSelected(uint8_t index)
{
  switch (index) {
    case 0: Screen.push(new WebAuthnGenerateScreen());    break;
    case 1: Screen.push(new WebAuthnBackupScreen());      break;
    case 2: Screen.push(new WebAuthnPasskeyListScreen()); break;
  }
}

#endif  // DEVICE_HAS_WEBAUTHN
