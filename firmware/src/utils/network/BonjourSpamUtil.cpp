#include "utils/network/BonjourSpamUtil.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_system.h>
#include <string.h>

namespace {

WiFiUDP  s_udp;
bool     s_running = false;
bool     s_enabled[BonjourSpamUtil::CAT_COUNT] = { true, true, true, true, true };
uint32_t s_sent = 0;
uint32_t s_perCat[BonjourSpamUtil::CAT_COUNT] = { 0 };
char     s_lastInst[64] = {};
BonjourSpamUtil::Category s_lastCat = BonjourSpamUtil::CAT_SPOTIFY;

// Round-robin cursor — tick() sends one phantom per call and advances.
uint8_t s_curCat = 0;
uint8_t s_curIdx = 0;

const IPAddress MCAST_IP(224, 0, 0, 251);
constexpr uint16_t MCAST_PORT = 5353;

struct Cat {
  const char* label;
  const char* svcType;     // e.g. "_spotify-connect._tcp"
  uint16_t    port;
  const char* names[BonjourSpamUtil::INSTANCES_PER_CAT];
};

const Cat CATEGORIES[] = {
  {
    "Spotify Connect",
    "_spotify-connect._tcp",
    4070,
    {
      "Living Room",   "Kitchen Sonos",  "Mom's Speaker",     "Office Pod",
      "Bedroom",       "Garage Speaker", "Bathroom HomePod",  "Patio Bose",
    }
  },
  {
    "AirPlay",
    "_airplay._tcp",
    7000,
    {
      "Apple TV Office", "HomePod mini",     "Living Room TV",  "Kitchen Display",
      "Bedroom Mac",     "Garage AppleTV",   "Office TV",       "Hidden Speaker",
    }
  },
  {
    "Google Cast",
    "_googlecast._tcp",
    8009,
    {
      "Living Room TV", "Bedroom Chromecast", "Kitchen Hub",   "Office Display",
      "Mom's TV",       "404-NOT-FOUND",      "Garage Cast",   "Patio Display",
    }
  },
  {
    "Printer",
    "_ipp._tcp",
    631,
    {
      "HP LaserJet Office", "Canon Bedroom",  "Brother Lobby", "Epson Garage",
      "404 PRINTER",        "Backup Printer", "Color Lobby",   "Old Office",
    }
  },
  {
    "Workstation",
    "_smb._tcp",
    445,
    {
      "DESKTOP-FBI-VAN", "MOMS-MACBOOK",   "JOHNS-PC-9001",  "404-NOT-FOUND",
      "BURN-NOTICE",     "PRINTER-CLOSET", "CTRL-ALT-DEL",   "STILL-WATCHING",
    }
  },
};
static_assert(sizeof(CATEGORIES) / sizeof(CATEGORIES[0]) == BonjourSpamUtil::CAT_COUNT,
              "CATEGORIES size must match CAT_COUNT");

// Encode a dot-separated DNS name with length-prefixed labels; appends final
// zero byte. Returns new pos, or -1 if the buffer would overflow / a label
// is invalid.
int encodeName(uint8_t* buf, int pos, int cap, const char* name)
{
  while (*name) {
    const char* dot = strchr(name, '.');
    int len = dot ? (int)(dot - name) : (int)strlen(name);
    if (len <= 0 || len > 63) return -1;
    if (pos + 1 + len > cap)  return -1;
    buf[pos++] = (uint8_t)len;
    memcpy(buf + pos, name, len);
    pos += len;
    name += len;
    if (*name == '.') name++;
  }
  if (pos >= cap) return -1;
  buf[pos++] = 0;
  return pos;
}

inline void writeU16(uint8_t* buf, int& pos, uint16_t v) {
  buf[pos++] = (v >> 8) & 0xFF;
  buf[pos++] = v & 0xFF;
}

inline void writeU32(uint8_t* buf, int& pos, uint32_t v) {
  buf[pos++] = (v >> 24) & 0xFF;
  buf[pos++] = (v >> 16) & 0xFF;
  buf[pos++] = (v >>  8) & 0xFF;
  buf[pos++] = v & 0xFF;
}

bool sendAnnouncement(const Cat& cat, const char* instance)
{
  uint8_t buf[512];
  int pos = 0;

  // Header: response + AA, 1 PTR answer, 3 additional (SRV/TXT/A)
  writeU16(buf, pos, 0x0000);  // ID
  writeU16(buf, pos, 0x8400);  // Flags: QR=1, AA=1
  writeU16(buf, pos, 0);       // QDCOUNT
  writeU16(buf, pos, 1);       // ANCOUNT
  writeU16(buf, pos, 0);       // NSCOUNT
  writeU16(buf, pos, 3);       // ARCOUNT

  String svcFull = String(cat.svcType) + ".local";
  String inst    = String(instance) + "." + cat.svcType + ".local";

  char host[24];
  snprintf(host, sizeof(host), "fake-%lx.local",
           (unsigned long)(esp_random() & 0xFFFFFFu));

  // ── PTR answer: svcFull -> inst ──
  pos = encodeName(buf, pos, sizeof(buf), svcFull.c_str());
  if (pos < 0) return false;
  writeU16(buf, pos, 12);        // PTR
  writeU16(buf, pos, 0x0001);    // IN (shared, no cache-flush)
  writeU32(buf, pos, 4500);      // TTL
  int rdlenPos = pos; pos += 2;
  int rdStart = pos;
  pos = encodeName(buf, pos, sizeof(buf), inst.c_str());
  if (pos < 0) return false;
  uint16_t rdlen = (uint16_t)(pos - rdStart);
  buf[rdlenPos] = (rdlen >> 8) & 0xFF;
  buf[rdlenPos + 1] = rdlen & 0xFF;

  // ── SRV additional: inst -> host:port ──
  pos = encodeName(buf, pos, sizeof(buf), inst.c_str());
  if (pos < 0) return false;
  writeU16(buf, pos, 33);        // SRV
  writeU16(buf, pos, 0x8001);    // IN + cache-flush
  writeU32(buf, pos, 120);
  rdlenPos = pos; pos += 2;
  rdStart = pos;
  writeU16(buf, pos, 0);         // priority
  writeU16(buf, pos, 0);         // weight
  writeU16(buf, pos, cat.port);  // port
  pos = encodeName(buf, pos, sizeof(buf), host);
  if (pos < 0) return false;
  rdlen = (uint16_t)(pos - rdStart);
  buf[rdlenPos] = (rdlen >> 8) & 0xFF;
  buf[rdlenPos + 1] = rdlen & 0xFF;

  // ── TXT additional: empty (single zero-length string) ──
  pos = encodeName(buf, pos, sizeof(buf), inst.c_str());
  if (pos < 0) return false;
  writeU16(buf, pos, 16);        // TXT
  writeU16(buf, pos, 0x8001);
  writeU32(buf, pos, 4500);
  writeU16(buf, pos, 1);         // RDLENGTH = 1
  if (pos >= (int)sizeof(buf)) return false;
  buf[pos++] = 0;                // empty TXT string

  // ── A additional: host -> IP (random last octet of our subnet) ──
  pos = encodeName(buf, pos, sizeof(buf), host);
  if (pos < 0) return false;
  writeU16(buf, pos, 1);         // A
  writeU16(buf, pos, 0x8001);
  writeU32(buf, pos, 120);
  writeU16(buf, pos, 4);         // RDLENGTH
  if (pos + 4 > (int)sizeof(buf)) return false;
  IPAddress local = WiFi.localIP();
  buf[pos++] = local[0];
  buf[pos++] = local[1];
  buf[pos++] = local[2];
  buf[pos++] = (uint8_t)(esp_random() & 0xFF);

  if (!s_udp.beginPacket(MCAST_IP, MCAST_PORT)) return false;
  s_udp.write(buf, pos);
  return s_udp.endPacket() != 0;
}

}  // namespace

// ── public ────────────────────────────────────────────────────────────────

const char* BonjourSpamUtil::categoryLabel(Category c) {
  if (c >= CAT_COUNT) return "?";
  return CATEGORIES[c].label;
}

bool BonjourSpamUtil::begin() {
  if (s_running) return true;
  if (WiFi.status() != WL_CONNECTED) return false;
  if (!s_udp.begin(0)) return false;
  s_running = true;
  return true;
}

void BonjourSpamUtil::end() {
  if (!s_running) return;
  s_udp.stop();
  s_running = false;
}

bool BonjourSpamUtil::isRunning() { return s_running; }

void BonjourSpamUtil::setEnabled(Category c, bool on) {
  if (c < CAT_COUNT) s_enabled[c] = on;
}

bool BonjourSpamUtil::isEnabled(Category c) {
  return c < CAT_COUNT ? s_enabled[c] : false;
}

uint8_t BonjourSpamUtil::tick() {
  if (!s_running) return 0;

  // Skip past disabled categories.
  for (uint8_t tries = 0; tries < CAT_COUNT; tries++) {
    if (s_enabled[s_curCat]) break;
    s_curIdx = 0;
    s_curCat = (s_curCat + 1) % CAT_COUNT;
  }
  if (!s_enabled[s_curCat]) return 0;  // nothing enabled

  const char* name = CATEGORIES[s_curCat].names[s_curIdx];
  bool ok = sendAnnouncement(CATEGORIES[s_curCat], name);
  if (ok) {
    s_sent++;
    s_perCat[s_curCat]++;
    strncpy(s_lastInst, name, sizeof(s_lastInst) - 1);
    s_lastInst[sizeof(s_lastInst) - 1] = '\0';
    s_lastCat = (Category)s_curCat;
  }

  // Advance cursor: next instance, wrapping to next enabled category.
  s_curIdx++;
  if (s_curIdx >= INSTANCES_PER_CAT) {
    s_curIdx = 0;
    s_curCat = (s_curCat + 1) % CAT_COUNT;
  }
  return ok ? 1 : 0;
}

uint32_t BonjourSpamUtil::packetsSent() { return s_sent; }

uint32_t BonjourSpamUtil::packetsForCategory(Category c) {
  return c < CAT_COUNT ? s_perCat[c] : 0;
}

const char* BonjourSpamUtil::lastInstance() { return s_lastInst; }

BonjourSpamUtil::Category BonjourSpamUtil::lastCategory() { return s_lastCat; }

void BonjourSpamUtil::resetCounter() {
  s_sent = 0;
  for (uint8_t i = 0; i < CAT_COUNT; i++) s_perCat[i] = 0;
  s_lastInst[0] = '\0';
  s_curCat = 0;
  s_curIdx = 0;
}
